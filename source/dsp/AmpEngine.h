#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include "ToneStack.h"

/**
    AmpEngine — the algorithmic (non-NAM) guitar amp.

    Signal flow:
        input -> pre-drive bass cut (voicing-dependent tightness)
              -> pre-gain (drive * voicing trim, with power-supply sag)
              -> cascaded tube-style gain stages   (oversampled 4x)
                     stage 1 (asymmetric soft clip) -> interstage DC block
                     -> stage 2 (soft clip)
              -> interactive tone stack (Marshall / Fender network)
              -> presence (power-amp high shelf)

    A handful of amp "voicings" (Fender clean, Plexi, JCM800, Rectifier) change
    the tone-stack circuit, the gain staging and the input tightness.
*/
class AmpEngine
{
public:
    enum class Model { Modern, FenderClean, Plexi, JCM800, Rectifier };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate  = spec.sampleRate;
        numChannels = (int) spec.numChannels;

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            spec.numChannels, osFactor,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple);
        oversampler->initProcessing (spec.maximumBlockSize);

        inputHP.prepare (spec);
        presence.prepare (spec);
        toneStack.prepare (spec);

        driveSmoothed.reset (sampleRate, 0.02);

        const double osRate = sampleRate * (double) (1 << osFactor);
        dcR = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 18.0 / osRate);

        dcX1.assign ((size_t) numChannels, 0.0f);
        dcY1.assign ((size_t) numChannels, 0.0f);

        applyVoicing();
        reset();
        updateFilters();
    }

    void reset()
    {
        if (oversampler != nullptr)
            oversampler->reset();

        inputHP.reset();
        presence.reset();
        toneStack.reset();

        std::fill (dcX1.begin(), dcX1.end(), 0.0f);
        std::fill (dcY1.begin(), dcY1.end(), 0.0f);
        sagEnv = 0.0f;
    }

    void setDriveDb  (float db)  { driveSmoothed.setTargetValue (juce::Decibels::decibelsToGain (db)); }
    void setBass     (float v)   { toneStack.setParams (v, midV, trebV); bassV = v; }
    void setMid      (float v)   { toneStack.setParams (bassV, v, trebV); midV = v; }
    void setTreble   (float v)   { toneStack.setParams (bassV, midV, v); trebV = v; }
    void setPresence (float v)   { presenceAmt = v; filtersDirty = true; }

    void setModel (Model m)
    {
        if (m != model)
        {
            model = m;
            applyVoicing();
            filtersDirty = true;
        }
    }

    float getLatencySamples() const
    {
        return oversampler != nullptr ? (float) oversampler->getLatencyInSamples() : 0.0f;
    }

    void processBlock (juce::dsp::AudioBlock<float>& block)
    {
        if (filtersDirty)
            updateFilters();

        const auto numChannelsBlk = block.getNumChannels();
        const auto numSamples     = block.getNumSamples();

        {
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            inputHP.process (ctx);
        }

        // Power-supply sag.
        float sumSq = 0.0f;
        if (numSamples > 0)
            for (size_t n = 0; n < numSamples; ++n)
            {
                const float s = block.getSample (0, (int) n);
                sumSq += s * s;
            }
        const float rms = std::sqrt (sumSq / (float) juce::jmax ((size_t) 1, numSamples));
        sagEnv += 0.2f * (rms - sagEnv);
        const float sagGain = 1.0f / (1.0f + 1.5f * sagEnv);

        // Pre-gain (drive * voicing trim), base rate.
        for (size_t n = 0; n < numSamples; ++n)
        {
            const float drive = driveSmoothed.getNextValue() * driveTrim * sagGain;
            for (size_t ch = 0; ch < numChannelsBlk; ++ch)
                block.setSample ((int) ch, (int) n, block.getSample ((int) ch, (int) n) * drive);
        }

        // Cascaded gain stages, oversampled.
        auto up = oversampler->processSamplesUp (block);
        {
            const auto upCh = up.getNumChannels();
            const auto upN  = up.getNumSamples();
            for (size_t ch = 0; ch < upCh; ++ch)
            {
                float x1 = dcX1[ch], y1 = dcY1[ch];
                for (size_t n = 0; n < upN; ++n)
                {
                    float s = up.getSample ((int) ch, (int) n);
                    s = std::tanh (s + biasAmt) - tanhBias;      // stage 1 (asymmetric)
                    const float y = s - x1 + dcR * y1;           // interstage DC block
                    x1 = s; y1 = y; s = y;
                    s = std::tanh (s * stage2Gain);              // stage 2
                    up.setSample ((int) ch, (int) n, s);
                }
                dcX1[ch] = x1; dcY1[ch] = y1;
            }
        }
        oversampler->processSamplesDown (block);

        // Interactive tone stack + presence.
        toneStack.process (block);
        {
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            presence.process (ctx);
        }
    }

private:
    using Filter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    void applyVoicing()
    {
        switch (model)
        {
            case Model::FenderClean:
                toneStack.setType (ToneStack::Type::Fender);
                driveTrim = 0.5f;  stage2Gain = 1.0f; biasAmt = 0.05f; inputHPFreq = 40.0;  break;
            case Model::Plexi:
                toneStack.setType (ToneStack::Type::Marshall);
                driveTrim = 1.3f;  stage2Gain = 1.6f; biasAmt = 0.20f; inputHPFreq = 110.0; break;
            case Model::JCM800:
                toneStack.setType (ToneStack::Type::Marshall);
                driveTrim = 2.2f;  stage2Gain = 2.4f; biasAmt = 0.25f; inputHPFreq = 130.0; break;
            case Model::Rectifier:
                toneStack.setType (ToneStack::Type::Marshall);
                driveTrim = 3.0f;  stage2Gain = 2.6f; biasAmt = 0.20f; inputHPFreq = 150.0; break;
            case Model::Modern:
            default:
                toneStack.setType (ToneStack::Type::Marshall);
                driveTrim = 1.0f;  stage2Gain = 1.8f; biasAmt = 0.15f; inputHPFreq = 95.0;  break;
        }
        tanhBias = std::tanh (biasAmt);
        toneStack.setParams (bassV, midV, trebV);
    }

    void updateFilters()
    {
        using Coefs = juce::dsp::IIR::Coefficients<float>;
        *inputHP.state  = *Coefs::makeHighPass (sampleRate, inputHPFreq);
        *presence.state = *Coefs::makeHighShelf (sampleRate, 5000.0, 0.707,
                              juce::Decibels::decibelsToGain (
                                  juce::jmap (presenceAmt, 0.0f, 1.0f, 0.0f, 10.0f)));
        filtersDirty = false;
    }

    static constexpr size_t osFactor = 2; // 4x

    double sampleRate  = 44100.0;
    int    numChannels = 2;
    Model  model = Model::Modern;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    juce::SmoothedValue<float> driveSmoothed { 1.0f };

    // Per-voicing parameters.
    float  driveTrim = 1.0f, stage2Gain = 1.8f, biasAmt = 0.15f, tanhBias = std::tanh (0.15f);
    double inputHPFreq = 95.0;

    float  bassV = 0.5f, midV = 0.5f, trebV = 0.5f, presenceAmt = 0.3f;
    float  sagEnv = 0.0f;
    bool   filtersDirty = true;

    std::vector<float> dcX1, dcY1;
    float dcR = 0.999f;

    Filter inputHP, presence;
    ToneStack toneStack;
};

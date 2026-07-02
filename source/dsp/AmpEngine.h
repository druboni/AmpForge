#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

/**
    AmpEngine — the algorithmic (non-NAM) guitar amp signal chain.

    Signal flow:
        input -> pre-drive bass cut (tighten lows before distortion)
              -> pre-gain (drive, with power-supply sag)
              -> cascaded tube-style gain stages   (oversampled 4x)
                     stage 1 (asymmetric soft clip) -> interstage DC block
                     -> stage 2 (soft clip)
              -> 3-band tone stack (bass / mid / treble)
              -> presence (high shelf)

    The cabinet and master stages live in the processor so they are shared
    with the NAM amp path.
*/
class AmpEngine
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate  = spec.sampleRate;
        numChannels = (int) spec.numChannels;

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            spec.numChannels, osFactor,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple);
        oversampler->initProcessing (spec.maximumBlockSize);

        inputHP.prepare (spec);
        toneLow.prepare (spec);
        toneMid.prepare (spec);
        toneHigh.prepare (spec);
        presence.prepare (spec);

        *inputHP.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 95.0);

        driveSmoothed.reset (sampleRate, 0.02);

        // Interstage DC-blocker coefficient, referenced to the oversampled rate.
        const double osRate = sampleRate * (double) (1 << osFactor);
        dcR = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 18.0 / osRate);

        dcX1.assign ((size_t) numChannels, 0.0f);
        dcY1.assign ((size_t) numChannels, 0.0f);

        reset();
        updateFilters();
    }

    void reset()
    {
        if (oversampler != nullptr)
            oversampler->reset();

        inputHP.reset();
        toneLow.reset();
        toneMid.reset();
        toneHigh.reset();
        presence.reset();

        std::fill (dcX1.begin(), dcX1.end(), 0.0f);
        std::fill (dcY1.begin(), dcY1.end(), 0.0f);
        sagEnv = 0.0f;
    }

    // Parameter setters ------------------------------------------------------
    void setDriveDb   (float db)   { driveSmoothed.setTargetValue (juce::Decibels::decibelsToGain (db)); }
    void setBass      (float v)    { bass = v;        filtersDirty = true; }
    void setMid       (float v)    { mid = v;         filtersDirty = true; }
    void setTreble    (float v)    { treble = v;      filtersDirty = true; }
    void setPresence  (float v)    { presenceAmt = v; filtersDirty = true; }

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

        // Pre-drive bass cut: keeps the low end tight instead of flubby once
        // it hits the gain stages.
        {
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            inputHP.process (ctx);
        }

        // Power-supply "sag": louder input pulls the effective drive down a
        // touch, then it recovers — a subtle compression/bloom.
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

        // Pre-gain into the non-linearity (base rate, linear).
        for (size_t n = 0; n < numSamples; ++n)
        {
            const float drive = driveSmoothed.getNextValue() * sagGain;
            for (size_t ch = 0; ch < numChannelsBlk; ++ch)
                block.setSample ((int) ch, (int) n, block.getSample ((int) ch, (int) n) * drive);
        }

        // Cascaded gain stages in the oversampled domain.
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

                    // Stage 1: asymmetric soft clip (even harmonics).
                    s = std::tanh (s + biasAmt) - tanhBias;

                    // Interstage DC blocker (removes the offset the asymmetry adds).
                    const float y = s - x1 + dcR * y1;
                    x1 = s;
                    y1 = y;
                    s  = y;

                    // Stage 2: hotter soft clip for compression/sustain.
                    s = std::tanh (s * stage2Gain);

                    up.setSample ((int) ch, (int) n, s);
                }
                dcX1[ch] = x1;
                dcY1[ch] = y1;
            }
        }
        oversampler->processSamplesDown (block);

        // Tone stack + presence (base rate).
        {
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            toneLow.process (ctx);
            toneMid.process (ctx);
            toneHigh.process (ctx);
            presence.process (ctx);
        }
    }

private:
    using Filter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    void updateFilters()
    {
        const auto db = [] (float v) { return juce::jmap (v, 0.0f, 1.0f, -15.0f, 15.0f); };
        using Coefs = juce::dsp::IIR::Coefficients<float>;

        *toneLow.state  = *Coefs::makeLowShelf  (sampleRate, 120.0,  0.707,
                              juce::Decibels::decibelsToGain (db (bass)));
        *toneMid.state  = *Coefs::makePeakFilter (sampleRate, 750.0,  0.707,
                              juce::Decibels::decibelsToGain (db (mid)));
        *toneHigh.state = *Coefs::makeHighShelf (sampleRate, 3000.0, 0.707,
                              juce::Decibels::decibelsToGain (db (treble)));
        *presence.state = *Coefs::makeHighShelf (sampleRate, 5000.0, 0.707,
                              juce::Decibels::decibelsToGain (
                                  juce::jmap (presenceAmt, 0.0f, 1.0f, 0.0f, 10.0f)));
        filtersDirty = false;
    }

    static constexpr size_t osFactor = 2; // 2^2 = 4x oversampling

    static constexpr float biasAmt    = 0.15f;
    static constexpr float stage2Gain = 1.8f;
    const float tanhBias = std::tanh (biasAmt);

    double sampleRate  = 44100.0;
    int    numChannels = 2;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    juce::SmoothedValue<float> driveSmoothed  { 1.0f };

    float bass = 0.5f, mid = 0.5f, treble = 0.5f, presenceAmt = 0.3f;
    float sagEnv = 0.0f;
    bool  filtersDirty = true;

    // Interstage DC-blocker state (per channel).
    std::vector<float> dcX1, dcY1;
    float dcR = 0.999f;

    Filter inputHP, toneLow, toneMid, toneHigh, presence;
};

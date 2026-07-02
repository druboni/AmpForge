#pragma once

#include <juce_dsp/juce_dsp.h>
#include "CabinetSim.h"

/**
    AmpEngine — the guitar amp signal chain.

    Signal flow:
        input -> pre-gain (drive, with power-supply sag)
              -> tube-style waveshaper   (oversampled 4x to tame aliasing)
              -> 3-band tone stack (bass / mid / treble)
              -> presence (high shelf)
              -> cabinet (convolution)
              -> master level
*/
class AmpEngine
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            spec.numChannels, osFactor,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple);
        oversampler->initProcessing (spec.maximumBlockSize);

        toneLow.prepare (spec);
        toneMid.prepare (spec);
        toneHigh.prepare (spec);
        presence.prepare (spec);
        cabinet.prepare (spec);

        driveSmoothed.reset (sampleRate, 0.02);
        masterSmoothed.reset (sampleRate, 0.02);

        reset();
        updateFilters();
    }

    void reset()
    {
        if (oversampler != nullptr)
            oversampler->reset();

        toneLow.reset();
        toneMid.reset();
        toneHigh.reset();
        presence.reset();
        cabinet.reset();
        sagEnv = 0.0f;
    }

    // Parameter setters ------------------------------------------------------
    void setDriveDb   (float db)   { driveSmoothed.setTargetValue (juce::Decibels::decibelsToGain (db)); }
    void setMasterDb  (float db)   { masterSmoothed.setTargetValue (juce::Decibels::decibelsToGain (db)); }
    void setBass      (float v)    { bass = v;        filtersDirty = true; }
    void setMid       (float v)    { mid = v;         filtersDirty = true; }
    void setTreble    (float v)    { treble = v;      filtersDirty = true; }
    void setPresence  (float v)    { presenceAmt = v; filtersDirty = true; }
    void setCabBypass (bool b)     { cabBypass = b; }

    float getLatencySamples() const
    {
        return oversampler != nullptr ? (float) oversampler->getLatencyInSamples() : 0.0f;
    }

    void processBlock (juce::dsp::AudioBlock<float>& block)
    {
        if (filtersDirty)
            updateFilters();

        const auto numChannels = block.getNumChannels();
        const auto numSamples  = block.getNumSamples();

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

        // 1) Pre-gain into the non-linearity (base rate, linear).
        for (size_t n = 0; n < numSamples; ++n)
        {
            const float drive = driveSmoothed.getNextValue() * sagGain;
            for (size_t ch = 0; ch < numChannels; ++ch)
                block.setSample ((int) ch, (int) n, block.getSample ((int) ch, (int) n) * drive);
        }

        // 2) Waveshaper in the oversampled domain.
        auto up = oversampler->processSamplesUp (block);
        {
            const auto upCh = up.getNumChannels();
            const auto upN  = up.getNumSamples();
            for (size_t ch = 0; ch < upCh; ++ch)
                for (size_t n = 0; n < upN; ++n)
                    up.setSample ((int) ch, (int) n, waveshape (up.getSample ((int) ch, (int) n)));
        }
        oversampler->processSamplesDown (block);

        // 3) Tone stack + presence + cabinet (base rate).
        {
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            toneLow.process (ctx);
            toneMid.process (ctx);
            toneHigh.process (ctx);
            presence.process (ctx);
        }
        cabinet.process (block, cabBypass);

        // 4) Master.
        for (size_t n = 0; n < numSamples; ++n)
        {
            const float master = masterSmoothed.getNextValue();
            for (size_t ch = 0; ch < numChannels; ++ch)
                block.setSample ((int) ch, (int) n, block.getSample ((int) ch, (int) n) * master);
        }
    }

private:
    using Filter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    static float waveshape (float x) noexcept
    {
        // tanh soft clip with a touch of asymmetry for even harmonics.
        constexpr float bias = 0.15f;
        return std::tanh (x + bias) - std::tanh (bias);
    }

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

    double sampleRate = 44100.0;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    juce::SmoothedValue<float> driveSmoothed  { 1.0f };
    juce::SmoothedValue<float> masterSmoothed { 1.0f };

    float bass = 0.5f, mid = 0.5f, treble = 0.5f, presenceAmt = 0.3f;
    float sagEnv = 0.0f;
    bool  cabBypass = false;
    bool  filtersDirty = true;

    Filter toneLow, toneMid, toneHigh, presence;
    CabinetSim cabinet;
};

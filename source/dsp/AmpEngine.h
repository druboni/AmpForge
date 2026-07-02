#pragma once

#include <juce_dsp/juce_dsp.h>

/**
    AmpEngine — the guitar amp signal chain.

    Signal flow:
        input -> pre-gain (drive) -> tube-style waveshaper
              -> 3-band tone stack (bass / mid / treble)
              -> presence (high shelf) -> cabinet low-pass
              -> master level

    All parameters are plain floats set from the processor each block.
*/
class AmpEngine
{
public:
    AmpEngine() = default;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

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
        toneLow.reset();
        toneMid.reset();
        toneHigh.reset();
        presence.reset();
        cabinet.reset();
    }

    // Parameter setters (values already mapped to sensible ranges) ----------
    void setDriveDb   (float db)   { driveSmoothed.setTargetValue (juce::Decibels::decibelsToGain (db)); }
    void setMasterDb  (float db)   { masterSmoothed.setTargetValue (juce::Decibels::decibelsToGain (db)); }
    void setBass      (float v)    { bass = v;      filtersDirty = true; }
    void setMid       (float v)    { mid = v;       filtersDirty = true; }
    void setTreble    (float v)    { treble = v;    filtersDirty = true; }
    void setPresence  (float v)    { presenceAmt = v; filtersDirty = true; }
    void setCabCutoff (float hz)   { cabCutoff = hz; filtersDirty = true; }

    // Process one channel of audio in place ---------------------------------
    void processBlock (juce::dsp::AudioBlock<float>& block)
    {
        if (filtersDirty)
            updateFilters();

        const auto numChannels = block.getNumChannels();
        const auto numSamples  = block.getNumSamples();

        for (size_t n = 0; n < numSamples; ++n)
        {
            const float drive  = driveSmoothed.getNextValue();
            const float master = masterSmoothed.getNextValue();

            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                float x = block.getSample ((int) ch, (int) n);

                // Pre-gain drive into the non-linearity.
                x *= drive;

                // Asymmetric tube-style soft clipping.
                x = waveshape (x);

                block.setSample ((int) ch, (int) n, x * master);
            }
        }

        // Tone stack + cabinet run as block filters after the non-linearity.
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        toneLow.process (ctx);
        toneMid.process (ctx);
        toneHigh.process (ctx);
        presence.process (ctx);
        cabinet.process (ctx);
    }

private:
    using Filter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    static float waveshape (float x) noexcept
    {
        // tanh soft clip with a touch of asymmetry for even harmonics.
        const float bias = 0.15f;
        return std::tanh (x + bias) - std::tanh (bias);
    }

    void updateFilters()
    {
        // Tone stack: gentle shelves/peak driven by 0..1 knob values,
        // mapped to +/- 15 dB.
        const auto db = [] (float v) { return juce::jmap (v, 0.0f, 1.0f, -15.0f, 15.0f); };

        *toneLow.state  = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sampleRate, 120.0,  0.707, juce::Decibels::decibelsToGain (db (bass)));
        *toneMid.state  = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, 750.0,  0.707, juce::Decibels::decibelsToGain (db (mid)));
        *toneHigh.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, 3000.0, 0.707, juce::Decibels::decibelsToGain (db (treble)));
        *presence.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, 5000.0, 0.707, juce::Decibels::decibelsToGain (
                juce::jmap (presenceAmt, 0.0f, 1.0f, 0.0f, 10.0f)));

        *cabinet.state  = *juce::dsp::IIR::Coefficients<float>::makeLowPass (
            sampleRate, juce::jlimit (1000.0, 12000.0, (double) cabCutoff));

        filtersDirty = false;
    }

    double sampleRate = 44100.0;

    juce::SmoothedValue<float> driveSmoothed  { 1.0f };
    juce::SmoothedValue<float> masterSmoothed { 1.0f };

    float bass = 0.5f, mid = 0.5f, treble = 0.5f, presenceAmt = 0.3f;
    float cabCutoff = 5000.0f;
    bool  filtersDirty = true;

    Filter toneLow, toneMid, toneHigh, presence, cabinet;
};

#pragma once

#include <juce_dsp/juce_dsp.h>

/**
    DS2Pedal — a BOSS DS-2 Turbo Distortion style pedal.

    Like the DS-1 (hard silicon-diode clipping) but hotter, with the DS-2's
    two-position Turbo switch:

        Mode I   — voiced close to a DS-1: aggressive but even.
        Mode II  — "Turbo": extra pre-gain plus a pushed midrange for a
                   thicker, more saturated lead voice.

    Controls: Dist / Tone / Level, plus the Turbo mode. Always oversamples 4x
    (even bypassed) so the reported latency stays constant.
*/
class DS2Pedal
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            spec.numChannels, osFactor,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple);
        oversampler->initProcessing (spec.maximumBlockSize);

        inputHP.prepare (spec);
        midBoost.prepare (spec);
        toneLow.prepare (spec);
        toneHigh.prepare (spec);
        notch.prepare (spec);

        gainSmoothed.reset (sampleRate, 0.02);
        levelSmoothed.reset (sampleRate, 0.02);

        using Coefs = juce::dsp::IIR::Coefficients<float>;
        *inputHP.state  = *Coefs::makeHighPass (sampleRate, 90.0);
        *notch.state    = *Coefs::makePeakFilter (sampleRate, 500.0, 1.2,
                              juce::Decibels::decibelsToGain (-3.0f));
        // Engaged only in Turbo (mode II): a broad mid push into the clipper.
        *midBoost.state = *Coefs::makePeakFilter (sampleRate, 800.0, 0.8,
                              juce::Decibels::decibelsToGain (6.0f));

        updateTone();
        reset();
    }

    void reset()
    {
        if (oversampler != nullptr)
            oversampler->reset();

        inputHP.reset();
        midBoost.reset();
        toneLow.reset();
        toneHigh.reset();
        notch.reset();
    }

    void setEnabled (bool e)  { enabled = e; }
    void setDist    (float v) { gainSmoothed.setTargetValue (
                                    juce::Decibels::decibelsToGain (juce::jmap (v, 0.0f, 1.0f, 0.0f, 52.0f))); }
    void setLevel   (float v) { levelSmoothed.setTargetValue (
                                    juce::Decibels::decibelsToGain (juce::jmap (v, 0.0f, 1.0f, -30.0f, 6.0f))); }
    void setTone    (float v) { if (! juce::approximatelyEqual (v, tone)) { tone = v; toneDirty = true; } }
    void setMode    (int m)   { turbo = (m >= 1); }   // 0 = Mode I, 1 = Mode II (Turbo)

    float getLatencySamples() const
    {
        return oversampler != nullptr ? (float) oversampler->getLatencyInSamples() : 0.0f;
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const auto numChannels = block.getNumChannels();
        const auto numSamples  = block.getNumSamples();

        if (! enabled)
        {
            auto upBypass = oversampler->processSamplesUp (block);
            juce::ignoreUnused (upBypass);
            oversampler->processSamplesDown (block);
            return;
        }

        if (toneDirty)
            updateTone();

        // 1) Input high-pass, plus the Turbo mid push in mode II (base rate).
        {
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            inputHP.process (ctx);
            if (turbo)
                midBoost.process (ctx);
        }

        // 2) Pre-gain — Turbo mode drives the clipper noticeably harder.
        const float modeGain = turbo ? 1.7f : 1.0f;
        for (size_t n = 0; n < numSamples; ++n)
        {
            const float g = gainSmoothed.getNextValue() * modeGain;
            for (size_t ch = 0; ch < numChannels; ++ch)
                block.setSample ((int) ch, (int) n, block.getSample ((int) ch, (int) n) * g);
        }

        // 3) Hard diode clipping in the oversampled domain. Turbo clips a little
        //    harder (lower threshold) for a more compressed, saturated feel.
        const float th = turbo ? 0.4f : 0.5f;
        auto up = oversampler->processSamplesUp (block);
        {
            const auto upCh = up.getNumChannels();
            const auto upN  = up.getNumSamples();
            for (size_t ch = 0; ch < upCh; ++ch)
                for (size_t n = 0; n < upN; ++n)
                    up.setSample ((int) ch, (int) n, hardClip (up.getSample ((int) ch, (int) n), th));
        }
        oversampler->processSamplesDown (block);

        // 4) Tone tilt + character notch (base rate).
        {
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            toneLow.process (ctx);
            toneHigh.process (ctx);
            notch.process (ctx);
        }

        // 5) Output level (base rate).
        for (size_t n = 0; n < numSamples; ++n)
        {
            const float lvl = levelSmoothed.getNextValue();
            for (size_t ch = 0; ch < numChannels; ++ch)
                block.setSample ((int) ch, (int) n, block.getSample ((int) ch, (int) n) * lvl);
        }
    }

private:
    using Filter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    static float hardClip (float x, float th) noexcept
    {
        if (x >  th) return  th + (std::tanh ((x - th)) * 0.02f);
        if (x < -th) return -th + (std::tanh ((x + th)) * 0.02f);
        return x;
    }

    void updateTone()
    {
        using Coefs = juce::dsp::IIR::Coefficients<float>;
        const float lowDb  = juce::jmap (tone, 0.0f, 1.0f,  6.0f, -8.0f);
        const float highDb = juce::jmap (tone, 0.0f, 1.0f, -8.0f, 11.0f);

        *toneLow.state  = *Coefs::makeLowShelf  (sampleRate, 500.0,  0.707,
                              juce::Decibels::decibelsToGain (lowDb));
        *toneHigh.state = *Coefs::makeHighShelf (sampleRate, 2000.0, 0.707,
                              juce::Decibels::decibelsToGain (highDb));
        toneDirty = false;
    }

    static constexpr size_t osFactor = 2; // 4x

    double sampleRate = 44100.0;
    bool   enabled    = false;
    bool   turbo      = false;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    juce::SmoothedValue<float> gainSmoothed  { 1.0f };
    juce::SmoothedValue<float> levelSmoothed { 1.0f };

    float tone = 0.5f;
    bool  toneDirty = true;

    Filter inputHP, midBoost, toneLow, toneHigh, notch;
};

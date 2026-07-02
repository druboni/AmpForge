#pragma once

#include <juce_dsp/juce_dsp.h>

/**
    DistortionPedal — a BOSS DS-1 style distortion stompbox.

    Signal flow (when enabled):
        input high-pass  (tightens the low end going into the clipper)
      -> Dist pre-gain
      -> hard silicon-diode clipping   (oversampled 4x to tame aliasing)
      -> tone tilt (dark <-> bright) with a fixed mid notch for DS-1 character
      -> Level

    It always oversamples (even bypassed) so the reported latency stays
    constant regardless of the enable state.
*/
class DistortionPedal
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
        toneLow.prepare (spec);
        toneHigh.prepare (spec);
        notch.prepare (spec);

        gainSmoothed.reset (sampleRate, 0.02);
        levelSmoothed.reset (sampleRate, 0.02);

        using Coefs = juce::dsp::IIR::Coefficients<float>;
        *inputHP.state = *Coefs::makeHighPass (sampleRate, 90.0);
        *notch.state   = *Coefs::makePeakFilter (sampleRate, 500.0, 1.2,
                             juce::Decibels::decibelsToGain (-4.0f));

        updateTone();
        reset();
    }

    void reset()
    {
        if (oversampler != nullptr)
            oversampler->reset();

        inputHP.reset();
        toneLow.reset();
        toneHigh.reset();
        notch.reset();
    }

    void setEnabled (bool e)  { enabled = e; }
    void setDist    (float v) { gainSmoothed.setTargetValue (
                                    juce::Decibels::decibelsToGain (juce::jmap (v, 0.0f, 1.0f, 0.0f, 45.0f))); }
    void setLevel   (float v) { levelSmoothed.setTargetValue (
                                    juce::Decibels::decibelsToGain (juce::jmap (v, 0.0f, 1.0f, -30.0f, 6.0f))); }
    void setTone    (float v) { if (! juce::approximatelyEqual (v, tone)) { tone = v; toneDirty = true; } }

    float getLatencySamples() const
    {
        return oversampler != nullptr ? (float) oversampler->getLatencyInSamples() : 0.0f;
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const auto numChannels = block.getNumChannels();
        const auto numSamples  = block.getNumSamples();

        // When disabled we still run the oversampler as a pure delay so the
        // plugin's latency does not change when the pedal is toggled.
        if (! enabled)
        {
            auto upBypass = oversampler->processSamplesUp (block);
            juce::ignoreUnused (upBypass);
            oversampler->processSamplesDown (block);
            return;
        }

        if (toneDirty)
            updateTone();

        // 1) Input high-pass (base rate).
        {
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            inputHP.process (ctx);
        }

        // 2) Pre-gain (base rate, linear -> no need to oversample).
        for (size_t n = 0; n < numSamples; ++n)
        {
            const float g = gainSmoothed.getNextValue();
            for (size_t ch = 0; ch < numChannels; ++ch)
                block.setSample ((int) ch, (int) n, block.getSample ((int) ch, (int) n) * g);
        }

        // 3) Hard diode clipping in the oversampled domain.
        auto up = oversampler->processSamplesUp (block);
        {
            const auto upCh = up.getNumChannels();
            const auto upN  = up.getNumSamples();
            for (size_t ch = 0; ch < upCh; ++ch)
                for (size_t n = 0; n < upN; ++n)
                    up.setSample ((int) ch, (int) n, hardClip (up.getSample ((int) ch, (int) n)));
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

    static float hardClip (float x) noexcept
    {
        // Silicon-diode style hard clip with a hint of soft knee.
        constexpr float th = 0.5f;
        if (x >  th) return  th + (std::tanh ((x - th)) * 0.02f);
        if (x < -th) return -th + (std::tanh ((x + th)) * 0.02f);
        return x;
    }

    void updateTone()
    {
        using Coefs = juce::dsp::IIR::Coefficients<float>;
        // tone up -> cut lows / boost highs (brighter & buzzier).
        const float lowDb  = juce::jmap (tone, 0.0f, 1.0f,  6.0f, -8.0f);
        const float highDb = juce::jmap (tone, 0.0f, 1.0f, -8.0f, 10.0f);

        *toneLow.state  = *Coefs::makeLowShelf  (sampleRate, 500.0,  0.707,
                              juce::Decibels::decibelsToGain (lowDb));
        *toneHigh.state = *Coefs::makeHighShelf (sampleRate, 2000.0, 0.707,
                              juce::Decibels::decibelsToGain (highDb));
        toneDirty = false;
    }

    static constexpr size_t osFactor = 2; // 2^2 = 4x oversampling

    double sampleRate = 44100.0;
    bool   enabled    = false;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    juce::SmoothedValue<float> gainSmoothed  { 1.0f };
    juce::SmoothedValue<float> levelSmoothed { 1.0f };

    float tone = 0.5f;
    bool  toneDirty = true;

    Filter inputHP, toneLow, toneHigh, notch;
};

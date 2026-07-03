#pragma once

#include <juce_dsp/juce_dsp.h>

/**
    OverdrivePedal — a BOSS SD-1 Super OverDrive style pedal.

    Softer and lower-gain than the DS-1: op-amp style *asymmetric* soft clipping
    (positive half clips a touch harder than the negative, adding even-order
    harmonics) plus the classic Tube-Screamer-ish midrange hump before the
    clipper. Great as a light grit box or as a boost to push the amp harder.

    Signal flow (when enabled):
        input high-pass  (TS-style low cut so the lows stay tight)
      -> midrange hump   (shapes what hits the clipper -> vocal midrange)
      -> Drive pre-gain
      -> asymmetric soft clip  (oversampled 4x)
      -> tone tilt (gentle)
      -> Level

    Like the DS-1 it always oversamples so the reported latency is constant.
*/
class OverdrivePedal
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
        midHump.prepare (spec);
        toneLow.prepare (spec);
        toneHigh.prepare (spec);

        gainSmoothed.reset (sampleRate, 0.02);
        levelSmoothed.reset (sampleRate, 0.02);

        using Coefs = juce::dsp::IIR::Coefficients<float>;
        *inputHP.state = *Coefs::makeHighPass (sampleRate, 120.0);
        *midHump.state = *Coefs::makePeakFilter (sampleRate, 720.0, 0.7,
                             juce::Decibels::decibelsToGain (4.0f));

        updateTone();
        reset();
    }

    void reset()
    {
        if (oversampler != nullptr)
            oversampler->reset();

        inputHP.reset();
        midHump.reset();
        toneLow.reset();
        toneHigh.reset();
    }

    void setEnabled (bool e)  { enabled = e; }
    void setDrive   (float v) { gainSmoothed.setTargetValue (
                                    juce::Decibels::decibelsToGain (juce::jmap (v, 0.0f, 1.0f, 0.0f, 30.0f))); }
    void setLevel   (float v) { levelSmoothed.setTargetValue (
                                    juce::Decibels::decibelsToGain (juce::jmap (v, 0.0f, 1.0f, -24.0f, 6.0f))); }
    void setTone    (float v) { if (! juce::approximatelyEqual (v, tone)) { tone = v; toneDirty = true; } }

    float getLatencySamples() const
    {
        return oversampler != nullptr ? (float) oversampler->getLatencyInSamples() : 0.0f;
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const auto numChannels = block.getNumChannels();
        const auto numSamples  = block.getNumSamples();

        // Bypassed: still run the oversampler as a pure delay so latency is fixed.
        if (! enabled)
        {
            auto upBypass = oversampler->processSamplesUp (block);
            juce::ignoreUnused (upBypass);
            oversampler->processSamplesDown (block);
            return;
        }

        if (toneDirty)
            updateTone();

        // 1) Input high-pass + midrange hump (base rate, before the clipper).
        {
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            inputHP.process (ctx);
            midHump.process (ctx);
        }

        // 2) Pre-gain (base rate, linear).
        for (size_t n = 0; n < numSamples; ++n)
        {
            const float g = gainSmoothed.getNextValue();
            for (size_t ch = 0; ch < numChannels; ++ch)
                block.setSample ((int) ch, (int) n, block.getSample ((int) ch, (int) n) * g);
        }

        // 3) Asymmetric soft clipping in the oversampled domain.
        auto up = oversampler->processSamplesUp (block);
        {
            const auto upCh = up.getNumChannels();
            const auto upN  = up.getNumSamples();
            for (size_t ch = 0; ch < upCh; ++ch)
                for (size_t n = 0; n < upN; ++n)
                    up.setSample ((int) ch, (int) n, softClipAsym (up.getSample ((int) ch, (int) n)));
        }
        oversampler->processSamplesDown (block);

        // 4) Tone tilt (base rate).
        {
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            toneLow.process (ctx);
            toneHigh.process (ctx);
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

    static float softClipAsym (float x) noexcept
    {
        // Op-amp + asymmetric diode pair: the DC bias makes one half clip
        // sooner, giving the SD-1 its slightly gritty, even-harmonic bite.
        constexpr float bias = 0.08f;
        return std::tanh (x + bias) - std::tanh (bias);
    }

    void updateTone()
    {
        using Coefs = juce::dsp::IIR::Coefficients<float>;
        // Gentler tilt than the DS-1 — overdrive stays smoother up top.
        const float lowDb  = juce::jmap (tone, 0.0f, 1.0f,  2.0f, -4.0f);
        const float highDb = juce::jmap (tone, 0.0f, 1.0f, -6.0f,  8.0f);

        *toneLow.state  = *Coefs::makeLowShelf  (sampleRate, 500.0,  0.707,
                              juce::Decibels::decibelsToGain (lowDb));
        *toneHigh.state = *Coefs::makeHighShelf (sampleRate, 2200.0, 0.707,
                              juce::Decibels::decibelsToGain (highDb));
        toneDirty = false;
    }

    static constexpr size_t osFactor = 2; // 4x

    double sampleRate = 44100.0;
    bool   enabled    = false;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    juce::SmoothedValue<float> gainSmoothed  { 1.0f };
    juce::SmoothedValue<float> levelSmoothed { 1.0f };

    float tone = 0.5f;
    bool  toneDirty = true;

    Filter inputHP, midHump, toneLow, toneHigh;
};

#pragma once

#include <juce_dsp/juce_dsp.h>

/**
    CabinetSim — speaker cabinet emulation via convolution.

    On prepare() it synthesizes a 4x12-style impulse response (low cut, a low
    "thump" bump, a low-mid scoop, a presence peak and a high roll-off) and
    loads it into a zero-latency convolution engine. Call loadIR() to swap in
    a real measured impulse response (.wav) instead.
*/
class CabinetSim
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        convolution.prepare (spec);
        loadDefaultIR();
    }

    void reset() { convolution.reset(); }

    void process (juce::dsp::AudioBlock<float>& block, bool bypass)
    {
        if (bypass)
            return;

        juce::dsp::ProcessContextReplacing<float> ctx (block);
        convolution.process (ctx);
    }

    // Swap in a user-provided impulse response file.
    void loadIR (const juce::File& irFile)
    {
        convolution.loadImpulseResponse (irFile,
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::yes,
            0,
            juce::dsp::Convolution::Normalise::yes);
    }

    void loadDefaultIR()
    {
        constexpr int len = 1024;
        juce::AudioBuffer<float> ir (1, len);
        ir.clear();
        ir.setSample (0, 0, 1.0f); // unit impulse

        // Speaker voicing filters. Running an impulse through them and
        // truncating yields an FIR impulse response with this response.
        using Coefs = juce::dsp::IIR::Coefficients<float>;
        const auto dB = [] (float d) { return juce::Decibels::decibelsToGain (d); };

        juce::dsp::IIR::Filter<float> hp, lowBump, scoop, presence, lp;
        hp.coefficients       = Coefs::makeHighPass  (sampleRate, 85.0);
        lowBump.coefficients  = Coefs::makePeakFilter (sampleRate, 120.0,  0.9, dB (3.0f));
        scoop.coefficients    = Coefs::makePeakFilter (sampleRate, 450.0,  1.1, dB (-5.0f));
        presence.coefficients = Coefs::makePeakFilter (sampleRate, 2600.0, 1.0, dB (5.0f));
        lp.coefficients       = Coefs::makeLowPass   (sampleRate, 4200.0);

        auto* d = ir.getWritePointer (0);
        for (int i = 0; i < len; ++i)
        {
            float s = d[i];
            s = hp.processSample (s);
            s = lowBump.processSample (s);
            s = scoop.processSample (s);
            s = presence.processSample (s);
            s = lp.processSample (s);
            d[i] = s;
        }

        convolution.loadImpulseResponse (std::move (ir), sampleRate,
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::no,
            juce::dsp::Convolution::Normalise::yes);
    }

private:
    double sampleRate = 44100.0;
    juce::dsp::Convolution convolution;
};

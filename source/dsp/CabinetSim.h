#pragma once

#include <juce_dsp/juce_dsp.h>

/**
    CabinetSim — speaker cabinet emulation via convolution.

    On prepare() it synthesizes a 4x12-style impulse response and loads it into
    a zero-latency convolution engine. Two built-in voicings are provided
    (Modern V30-ish and Vintage greenback-ish). Call loadIR() to swap in a real
    measured impulse response (.wav) instead.

    Note: these built-in cabs are *synthesized* voicings (no copyrighted IRs are
    bundled). For maximum realism, load a real measured IR via loadIR().
*/
class CabinetSim
{
public:
    enum class Voicing { Modern4x12, Vintage4x12 };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        convolution.prepare (spec);
        loadDefaultIR();
    }

    void reset() { convolution.reset(); }

    void setVoicing (Voicing v)
    {
        if (v != voicing)
        {
            voicing = v;
            usingCustomIR = false;
            loadDefaultIR();
        }
    }

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
        usingCustomIR = true;
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

        using Coefs = juce::dsp::IIR::Coefficients<float>;
        const auto dB = [] (float d) { return juce::Decibels::decibelsToGain (d); };

        // Voicing parameters: two different 4x12 characters.
        double hpF, bumpF, bumpQ, bumpDb, scoopF, scoopQ, scoopDb, presF, presQ, presDb, lpF;
        if (voicing == Voicing::Modern4x12)      // tighter, more presence (V30-ish)
        {
            hpF = 90.0;  bumpF = 130.0; bumpQ = 0.9; bumpDb = 2.5f;
            scoopF = 500.0; scoopQ = 1.2; scoopDb = -6.0;
            presF = 2800.0; presQ = 1.1; presDb = 6.0; lpF = 4600.0;
        }
        else                                     // warmer, darker (greenback-ish)
        {
            hpF = 80.0;  bumpF = 110.0; bumpQ = 0.8; bumpDb = 3.5f;
            scoopF = 400.0; scoopQ = 1.0; scoopDb = -3.5;
            presF = 2200.0; presQ = 0.9; presDb = 3.5; lpF = 3600.0;
        }

        juce::dsp::IIR::Filter<float> hp, lowBump, scoop, presence, lp;
        hp.coefficients       = Coefs::makeHighPass   (sampleRate, hpF);
        lowBump.coefficients  = Coefs::makePeakFilter (sampleRate, bumpF,  bumpQ,  dB ((float) bumpDb));
        scoop.coefficients    = Coefs::makePeakFilter (sampleRate, scoopF, scoopQ, dB ((float) scoopDb));
        presence.coefficients = Coefs::makePeakFilter (sampleRate, presF,  presQ,  dB ((float) presDb));
        lp.coefficients       = Coefs::makeLowPass    (sampleRate, lpF);

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
    Voicing voicing = Voicing::Modern4x12;
    bool usingCustomIR = false;
    juce::dsp::Convolution convolution;
};

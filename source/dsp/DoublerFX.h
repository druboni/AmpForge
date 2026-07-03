#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
    DoublerFX — a stereo double-tracker (ADT, "artificial double tracking").

    Generates two independently delayed and subtly detuned copies of the (mono)
    guitar and pans them left/right, so one take sounds like two players tracked
    together — wide, thick and grand. The dry signal stays centred; the two
    modulated voices spread it across the stereo field.

    Sits after the cabinet, before the time-based effects.

        Amount  — level of the doubled voices (thickness)
        Width   — how hard the two voices pan apart (stereo spread)
        Detune  — modulation depth: subtle pitch drift between the "two takes"
*/
class DoublerFX
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        const int maxSamps = (int) (0.06 * sampleRate) + 4; // up to 60 ms
        delayL.setMaximumDelayInSamples (maxSamps);
        delayR.setMaximumDelayInSamples (maxSamps);
        delayL.prepare (spec);
        delayR.prepare (spec);

        amountS.reset (sampleRate, 0.02);
        widthS.reset (sampleRate, 0.02);

        phaseL = 0.0f;
        phaseR = juce::MathConstants<float>::pi * 0.5f; // quarter cycle apart
        reset();
    }

    void reset()
    {
        delayL.reset();
        delayR.reset();
    }

    void setEnabled (bool  e) { enabled = e; }
    void setAmount  (float a) { amountS.setTargetValue (juce::jlimit (0.0f, 1.0f, a)); }
    void setWidth   (float w) { widthS.setTargetValue (juce::jlimit (0.0f, 1.0f, w)); }
    void setDetune  (float d) { detune = juce::jlimit (0.0f, 1.0f, d); }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (! enabled)
            return;

        const int numCh = (int) block.getNumChannels();
        const int n     = (int) block.getNumSamples();

        const float baseL = (float) (0.021 * sampleRate); // 21 ms
        const float baseR = (float) (0.027 * sampleRate); // 27 ms
        const float incL  = juce::MathConstants<float>::twoPi * 0.18f / (float) sampleRate;
        const float incR  = juce::MathConstants<float>::twoPi * 0.13f / (float) sampleRate;
        const float twoPi = juce::MathConstants<float>::twoPi;

        for (int i = 0; i < n; ++i)
        {
            const float dry = block.getSample (0, i); // signal is dual-mono here

            const float depth = (0.5f + 3.5f * detune) * (float) (0.001 * sampleRate); // 0.5..4 ms
            const float dL = baseL + depth * std::sin (phaseL);
            const float dR = baseR + depth * std::sin (phaseR);

            phaseL += incL; if (phaseL > twoPi) phaseL -= twoPi;
            phaseR += incR; if (phaseR > twoPi) phaseR -= twoPi;

            delayL.pushSample (0, dry);
            delayR.pushSample (0, dry);
            const float vL = delayL.popSample (0, dL);
            const float vR = delayR.popSample (0, dR);

            const float amount = amountS.getNextValue();
            const float width  = widthS.getNextValue();

            // width=1 -> voices hard-panned; width=0 -> both voices centred.
            const float wetL = vL * (0.5f + 0.5f * width) + vR * (0.5f - 0.5f * width);
            const float wetR = vR * (0.5f + 0.5f * width) + vL * (0.5f - 0.5f * width);

            if (numCh >= 2)
            {
                block.setSample (0, i, dry + amount * wetL);
                block.setSample (1, i, dry + amount * wetR);
            }
            else
            {
                block.setSample (0, i, dry + amount * 0.5f * (vL + vR));
            }
        }
    }

private:
    using Delay = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>;

    double sampleRate = 48000.0;
    Delay  delayL, delayR;

    juce::SmoothedValue<float> amountS { 0.0f }, widthS { 0.0f };
    float  detune = 0.5f;
    float  phaseL = 0.0f, phaseR = 0.0f;
    bool   enabled = false;
};

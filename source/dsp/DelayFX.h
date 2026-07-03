#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

/**
    DelayFX — a mono-per-channel feedback delay / echo with an analog-style
    voicing: each repeat is low-pass filtered in the feedback path so echoes
    get darker as they fade, like a tape/BBD delay.

        Time      20 ms .. 1.5 s
        Feedback  0 .. 0.95   (how many repeats)
        Mix       dry <-> wet

    Sits after the cabinet. All parameters are smoothed so knob moves don't
    click; adds no reported latency (the wet path is purely additive).
*/
class DelayFX
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        const int maxDelaySamples = (int) (sampleRate * 2.0) + 1; // up to 2 s
        delayLine.prepare (spec);
        delayLine.setMaximumDelayInSamples (maxDelaySamples);
        delayLine.reset();

        lpState.assign ((size_t) spec.numChannels, 0.0f);

        timeSmoothed.reset (sampleRate, 0.05); // slower so pitch shifts are gentle
        fbSmoothed.reset   (sampleRate, 0.02);
        mixSmoothed.reset  (sampleRate, 0.02);
    }

    void reset()
    {
        delayLine.reset();
        std::fill (lpState.begin(), lpState.end(), 0.0f);
    }

    void setEnabled  (bool  e)  { enabled = e; }
    void setTimeMs   (float ms) { timeSmoothed.setTargetValue (
                                      (float) (ms * 0.001 * sampleRate)); }
    void setFeedback (float f)  { fbSmoothed.setTargetValue (juce::jlimit (0.0f, 0.95f, f)); }
    void setMix      (float m)  { mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, m)); }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (! enabled)
            return;

        const auto numChannels = block.getNumChannels();
        const auto numSamples  = block.getNumSamples();

        for (size_t n = 0; n < numSamples; ++n)
        {
            const float delaySamples = timeSmoothed.getNextValue();
            const float fb           = fbSmoothed.getNextValue();
            const float mix          = mixSmoothed.getNextValue();
            delayLine.setDelay (delaySamples);

            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                const float in      = block.getSample ((int) ch, (int) n);
                const float delayed = delayLine.popSample ((int) ch);

                // One-pole low-pass on the feedback so repeats darken over time.
                float& s = lpState[ch];
                s += lpCoeff * (delayed - s);

                delayLine.pushSample ((int) ch, in + s * fb);
                block.setSample ((int) ch, (int) n, in * (1.0f - mix) + delayed * mix);
            }
        }
    }

private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine;
    std::vector<float> lpState;

    juce::SmoothedValue<float> timeSmoothed { 0.0f };
    juce::SmoothedValue<float> fbSmoothed   { 0.0f };
    juce::SmoothedValue<float> mixSmoothed  { 0.0f };

    double sampleRate = 48000.0;
    float  lpCoeff    = 0.35f; // feedback tone: lower = darker repeats
    bool   enabled    = false;
};

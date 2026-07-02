#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
    NoiseGate — a simple downward gate placed before the amp to silence hiss
    and hum between notes on high-gain settings (essential once you push a
    JCM800 / Rectifier voicing). Detects on channel 0 (the signal is dual-mono
    by this point) and applies one shared gain to every channel.
*/
class NoiseGate
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        attackCoef  = std::exp (-1.0 / (0.002 * sr));  // ~2 ms open
        releaseCoef = std::exp (-1.0 / (0.080 * sr));  // ~80 ms close
        envCoef     = std::exp (-1.0 / (0.010 * sr));  // detector smoothing
        reset();
    }

    void reset() { env = 0.0f; gain = 0.0f; }

    // 0 = gate off; otherwise maps to a threshold from -80 to -25 dBFS.
    void setAmount (float amount01)
    {
        enabled = amount01 > 0.001f;
        thresholdLin = juce::Decibels::decibelsToGain (juce::jmap (amount01, 0.0f, 1.0f, -80.0f, -25.0f));
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (! enabled)
            return;

        const auto numChannels = block.getNumChannels();
        const int  numSamples  = (int) block.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            const float x = std::abs (block.getSample (0, i));
            env = x > env ? x : (float) (x + (env - x) * envCoef);

            const float target = env > thresholdLin ? 1.0f : 0.0f;
            const double coef = target > gain ? attackCoef : releaseCoef;
            gain = (float) (target + (gain - target) * coef);

            for (size_t ch = 0; ch < numChannels; ++ch)
                block.setSample ((int) ch, i, block.getSample ((int) ch, i) * gain);
        }
    }

private:
    double sampleRate  = 48000.0;
    double attackCoef  = 0.0, releaseCoef = 0.0, envCoef = 0.0;
    float  thresholdLin = 0.0f;
    bool   enabled = false;

    float env = 0.0f, gain = 0.0f;
};

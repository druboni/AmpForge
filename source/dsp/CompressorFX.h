#pragma once

#include <juce_dsp/juce_dsp.h>

/**
    CompressorFX — a simple pedal-style compressor / sustainer at the front of
    the chain (before the distortion and amp). Evens out picking dynamics and
    adds sustain, like a Ross/Dyna Comp into the front of an amp.

        threshold + ratio  (fixed musical attack/release)
      -> makeup gain

    Runs at the base rate; adds no reported latency.
*/
class CompressorFX
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        comp.prepare (spec);
        comp.setAttack (12.0f);   // ms — fast enough to catch pick transients
        comp.setRelease (140.0f); // ms — musical release for sustain
        makeupSmoothed.reset (spec.sampleRate, 0.02);
    }

    void reset()
    {
        comp.reset();
    }

    void setEnabled   (bool  e)  { enabled = e; }
    void setThreshold (float db) { comp.setThreshold (db); }
    void setRatio     (float r)  { comp.setRatio (juce::jmax (1.0f, r)); }
    void setMakeup    (float db) { makeupSmoothed.setTargetValue (
                                       juce::Decibels::decibelsToGain (db)); }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (! enabled)
            return;

        juce::dsp::ProcessContextReplacing<float> ctx (block);
        comp.process (ctx);

        const auto numChannels = block.getNumChannels();
        const auto numSamples  = block.getNumSamples();
        for (size_t n = 0; n < numSamples; ++n)
        {
            const float g = makeupSmoothed.getNextValue();
            for (size_t ch = 0; ch < numChannels; ++ch)
                block.setSample ((int) ch, (int) n,
                                 block.getSample ((int) ch, (int) n) * g);
        }
    }

private:
    juce::dsp::Compressor<float> comp;
    juce::SmoothedValue<float>   makeupSmoothed { 1.0f };
    bool enabled = false;
};

#pragma once

#include <juce_dsp/juce_dsp.h>

/**
    ChorusFX — post-amp modulation for shimmer and stereo width. A thin wrapper
    around juce::dsp::Chorus with a fixed voicing (centre delay + no feedback)
    so only Rate / Depth / Mix are exposed.

    Sits after the cabinet, like a chorus in the amp's FX loop.
*/
class ChorusFX
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        chorus.prepare (spec);
        chorus.setCentreDelay (7.0f); // ms — classic chorus voicing
        chorus.setFeedback (0.0f);    // keep it a clean chorus, not a flanger
    }

    void reset()
    {
        chorus.reset();
    }

    void setEnabled (bool  e)  { enabled = e; }
    void setRate    (float hz) { chorus.setRate (hz); }
    void setDepth   (float d)  { chorus.setDepth (juce::jlimit (0.0f, 1.0f, d)); }
    void setMix     (float m)  { chorus.setMix (juce::jlimit (0.0f, 1.0f, m)); }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (! enabled)
            return;

        juce::dsp::ProcessContextReplacing<float> ctx (block);
        chorus.process (ctx);
    }

private:
    juce::dsp::Chorus<float> chorus;
    bool enabled = false;
};

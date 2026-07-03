#pragma once

#include <juce_dsp/juce_dsp.h>

/**
    ReverbFX — the classic room/hall tail at the end of the chain, wrapping
    juce::dsp::Reverb. Exposes Size (room size), Damping (how fast highs decay)
    and Mix (dry <-> wet). Sits after the cabinet and time effects.
*/
class ReverbFX
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        reverb.prepare (spec);
        reverb.setParameters (params);
    }

    void reset()
    {
        reverb.reset();
    }

    void setEnabled (bool  e) { enabled = e; }
    void setSize    (float v) { params.roomSize = juce::jlimit (0.0f, 1.0f, v); dirty = true; }
    void setDamping (float v) { params.damping  = juce::jlimit (0.0f, 1.0f, v); dirty = true; }
    void setMix     (float v) { const float m = juce::jlimit (0.0f, 1.0f, v);
                                params.wetLevel = m;
                                params.dryLevel = 1.0f - m;
                                dirty = true; }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (! enabled)
            return;

        if (dirty)
        {
            reverb.setParameters (params);
            dirty = false;
        }

        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);
    }

private:
    juce::dsp::Reverb            reverb;
    juce::dsp::Reverb::Parameters params { 0.5f,   // roomSize
                                           0.5f,   // damping
                                           0.25f,  // wetLevel
                                           0.75f,  // dryLevel
                                           1.0f,   // width
                                           0.0f }; // freezeMode
    bool enabled = false;
    bool dirty   = true;
};

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class AmpForgeAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AmpForgeAudioProcessorEditor (AmpForgeAudioProcessor&);
    ~AmpForgeAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    // One labelled rotary knob bound to an APVTS parameter.
    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void addKnob (Knob& knob, const juce::String& paramID, const juce::String& text);

    AmpForgeAudioProcessor& processor;

    Knob drive, bass, mid, treble, presence, cab, master;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpForgeAudioProcessorEditor)
};

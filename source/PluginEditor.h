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
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void addKnob (Knob& knob, const juce::String& paramID, const juce::String& text);

    AmpForgeAudioProcessor& processor;

    // Amp controls.
    Knob drive, bass, mid, treble, presence, master;
    juce::ToggleButton cabButton { "Cabinet" };
    std::unique_ptr<ButtonAttachment> cabAttachment;
    juce::TextButton loadIRButton { "Load IR..." };
    juce::TextButton resetCabButton { "Reset Cab" };

    // NAM amp capture (.nam) controls.
    juce::ToggleButton namButton { "NAM Amp" };
    std::unique_ptr<ButtonAttachment> namAttachment;
    juce::TextButton loadNamButton { "Load NAM..." };
    juce::Label namStatus;
    void updateNamStatus();

    // DS-1 pedal controls.
    Knob dist, pedalTone, pedalLevel;
    juce::ToggleButton ds1Button { "DS-1 On" };
    std::unique_ptr<ButtonAttachment> ds1Attachment;

    // Presets.
    juce::ComboBox presetBox;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpForgeAudioProcessorEditor)
};

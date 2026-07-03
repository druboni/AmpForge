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
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void addKnob (Knob& knob, const juce::String& paramID, const juce::String& text,
                  int textBoxWidth = 84);

    AmpForgeAudioProcessor& processor;

    // Amp controls.
    Knob gate, drive, bass, mid, treble, presence, master;
    juce::ComboBox ampBox;
    std::unique_ptr<ComboBoxAttachment> ampAttachment;
    juce::ToggleButton cabButton { "Cabinet" };
    std::unique_ptr<ButtonAttachment> cabAttachment;
    juce::ComboBox cabBox;
    std::unique_ptr<ComboBoxAttachment> cabModelAttachment;
    juce::TextButton loadIRButton { "Load IR..." };
    juce::TextButton resetCabButton { "Reset Cab" };

    // NAM amp capture (.nam) controls.
    juce::ToggleButton namButton { "NAM Amp" };
    std::unique_ptr<ButtonAttachment> namAttachment;
    juce::TextButton loadNamButton { "Load NAM..." };
    juce::Label namStatus;
    void updateNamStatus();

    // Drive pedals (all pre-amp): Overdrive (SD-1) | DS-1 | DS-2.
    Knob odDrive, odTone, odLevel;
    juce::ToggleButton odButton { "OD" };
    std::unique_ptr<ButtonAttachment> odAttachment;

    Knob dist, pedalTone, pedalLevel;
    juce::ToggleButton ds1Button { "DS-1" };
    std::unique_ptr<ButtonAttachment> ds1Attachment;

    Knob ds2Dist, ds2Tone, ds2Level;
    juce::ToggleButton ds2Button { "DS-2" };
    std::unique_ptr<ButtonAttachment> ds2Attachment;
    juce::ComboBox ds2ModeBox;
    std::unique_ptr<ComboBoxAttachment> ds2ModeAttachment;

    // FX modules (compressor is pre-amp; chorus/delay/reverb are post-amp).
    Knob compThresh, compRatio, compMakeup;
    juce::ToggleButton compButton { "Comp" };
    std::unique_ptr<ButtonAttachment> compAttachment;

    Knob choRate, choDepth, choMix;
    juce::ToggleButton choButton { "Chorus" };
    std::unique_ptr<ButtonAttachment> choAttachment;

    Knob dlyTime, dlyFb, dlyMix;
    juce::ToggleButton dlyButton { "Delay" };
    std::unique_ptr<ButtonAttachment> dlyAttachment;

    Knob revSize, revDamp, revMix;
    juce::ToggleButton revButton { "Reverb" };
    std::unique_ptr<ButtonAttachment> revAttachment;

    Knob dblAmount, dblWidth, dblDetune;
    juce::ToggleButton dblButton { "Double" };
    std::unique_ptr<ButtonAttachment> dblAttachment;

    // Presets.
    juce::ComboBox presetBox;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpForgeAudioProcessorEditor)
};

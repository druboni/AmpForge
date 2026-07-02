#include "PluginEditor.h"

namespace
{
    constexpr int knobSize   = 84;
    constexpr int knobGap    = 10;
    constexpr int sideMargin = 20;
    constexpr int headerH    = 64;
    constexpr int rowLabelH  = 20;
    constexpr int textBoxH   = 16;
}

AmpForgeAudioProcessorEditor::AmpForgeAudioProcessorEditor (AmpForgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    // Amp knobs.
    addKnob (drive,    "drive",    "Drive");
    addKnob (bass,     "bass",     "Bass");
    addKnob (mid,      "mid",      "Mid");
    addKnob (treble,   "treble",   "Treble");
    addKnob (presence, "presence", "Presence");
    addKnob (master,   "master",   "Master");

    // Pedal knobs.
    addKnob (dist,       "ds1_dist",  "Dist");
    addKnob (pedalTone,  "ds1_tone",  "Tone");
    addKnob (pedalLevel, "ds1_level", "Level");

    // Toggles.
    addAndMakeVisible (cabButton);
    cabAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "cab_on", cabButton);

    addAndMakeVisible (ds1Button);
    ds1Button.setColour (juce::ToggleButton::textColourId, juce::Colour (0xffff7b3d));
    ds1Attachment = std::make_unique<ButtonAttachment> (processor.apvts, "ds1_on", ds1Button);

    // Preset menu.
    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("Presets");
    int itemId = 1;
    for (const auto& preset : AmpForgeAudioProcessor::getPresets())
        presetBox.addItem (preset.name, itemId++);

    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0)
            processor.applyPreset (idx);
    };

    setSize (sideMargin * 2 + 6 * knobSize + 5 * knobGap,
             headerH + rowLabelH + knobSize + textBoxH + 90);
}

void AmpForgeAudioProcessorEditor::addKnob (Knob& knob,
                                            const juce::String& paramID,
                                            const juce::String& text)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobSize, textBoxH);
    addAndMakeVisible (knob.slider);

    knob.label.setText (text, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (knob.label);

    knob.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramID, knob.slider);
}

void AmpForgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1b1f));

    auto header = getLocalBounds().removeFromTop (headerH).reduced (sideMargin, 8);
    g.setColour (juce::Colour (0xffe0a030));
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("AmpForge", header, juce::Justification::centredLeft);
    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("guitar amp emulator", header, juce::Justification::bottomLeft);

    // Pedal section backdrop.
    const int pedalTop = headerH + rowLabelH + knobSize + textBoxH + 16;
    auto pedalArea = juce::Rectangle<int> (sideMargin, pedalTop,
                                           getWidth() - sideMargin * 2, 64);
    g.setColour (juce::Colour (0xff26262c));
    g.fillRoundedRectangle (pedalArea.toFloat(), 6.0f);
    g.setColour (juce::Colour (0xffff7b3d));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("DISTORTION  (DS-1 style)", pedalArea.reduced (12, 6),
                juce::Justification::topLeft);
}

void AmpForgeAudioProcessorEditor::resized()
{
    // Preset menu in the header, right side.
    presetBox.setBounds (getWidth() - sideMargin - 150, 14, 150, 26);

    // Amp knob row.
    Knob* ampKnobs[] = { &drive, &bass, &mid, &treble, &presence, &master };
    int x = sideMargin;
    const int labelY = headerH;
    const int knobY  = headerH + rowLabelH;
    for (auto* k : ampKnobs)
    {
        k->label.setBounds  (x, labelY, knobSize, rowLabelH);
        k->slider.setBounds (x, knobY, knobSize, knobSize + textBoxH);
        x += knobSize + knobGap;
    }

    // Cab toggle sits under the master knob column.
    cabButton.setBounds (x - (knobSize + knobGap), knobY + knobSize + textBoxH - 2, knobSize, 22);

    // Pedal row (inside the backdrop drawn in paint()).
    const int pedalTop = headerH + rowLabelH + knobSize + textBoxH + 16;
    ds1Button.setBounds (sideMargin + 12, pedalTop + 30, 80, 24);

    Knob* pedalKnobs[] = { &dist, &pedalTone, &pedalLevel };
    int px = getWidth() - sideMargin - 3 * (knobSize) - 2 * knobGap;
    for (auto* k : pedalKnobs)
    {
        k->label.setBounds  (px, pedalTop - 2, knobSize, 16);
        k->slider.setBounds (px, pedalTop + 8, knobSize, 52);
        px += knobSize + knobGap;
    }
}

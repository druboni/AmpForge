#include "PluginEditor.h"

namespace
{
    constexpr int knobSize   = 90;
    constexpr int knobGap    = 12;
    constexpr int topMargin  = 70;
    constexpr int sideMargin = 20;
}

AmpForgeAudioProcessorEditor::AmpForgeAudioProcessorEditor (AmpForgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    addKnob (drive,    "drive",    "Drive");
    addKnob (bass,     "bass",     "Bass");
    addKnob (mid,      "mid",      "Mid");
    addKnob (treble,   "treble",   "Treble");
    addKnob (presence, "presence", "Presence");
    addKnob (cab,      "cab",      "Cabinet");
    addKnob (master,   "master",   "Master");

    const int numKnobs = 7;
    setSize (sideMargin * 2 + numKnobs * knobSize + (numKnobs - 1) * knobGap,
             topMargin + knobSize + 40);
}

void AmpForgeAudioProcessorEditor::addKnob (Knob& knob,
                                            const juce::String& paramID,
                                            const juce::String& text)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobSize, 18);
    addAndMakeVisible (knob.slider);

    knob.label.setText (text, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (knob.label);

    knob.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramID, knob.slider);
}

void AmpForgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1b1f));

    g.setColour (juce::Colour (0xffe0a030));
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("AmpForge", getLocalBounds().removeFromTop (topMargin).reduced (sideMargin, 10),
                juce::Justification::centredLeft);

    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("guitar amp emulator", getLocalBounds().removeFromTop (topMargin).reduced (sideMargin, 10),
                juce::Justification::bottomLeft);
}

void AmpForgeAudioProcessorEditor::resized()
{
    Knob* knobs[] = { &drive, &bass, &mid, &treble, &presence, &cab, &master };

    int x = sideMargin;
    const int y = topMargin;

    for (auto* k : knobs)
    {
        k->label.setBounds  (x, y - 20, knobSize, 18);
        k->slider.setBounds (x, y, knobSize, knobSize);
        x += knobSize + knobGap;
    }
}

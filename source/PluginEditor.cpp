#include "PluginEditor.h"

namespace
{
    constexpr int knobSize   = 84;
    constexpr int knobGap    = 10;
    constexpr int sideMargin = 20;
    constexpr int textBoxH   = 16;

    constexpr int headerH    = 60;
    constexpr int ampLabelY  = headerH + 8;
    constexpr int ampKnobY   = ampLabelY + 18;
    constexpr int ampKnobH   = knobSize + textBoxH;
    constexpr int stripY     = ampKnobY + ampKnobH + 6;   // cab / IR buttons
    constexpr int stripH     = 26;
    constexpr int namY       = stripY + stripH + 8;        // NAM row
    constexpr int namH       = 26;
    constexpr int pedalTop   = namY + namH + 12;
    constexpr int pedalH     = 96;
}

AmpForgeAudioProcessorEditor::AmpForgeAudioProcessorEditor (AmpForgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    // Amp knobs.
    addKnob (gate,     "gate",     "Gate");
    addKnob (drive,    "drive",    "Drive");
    addKnob (bass,     "bass",     "Bass");
    addKnob (mid,      "mid",      "Mid");
    addKnob (treble,   "treble",   "Treble");
    addKnob (presence, "presence", "Presence");
    addKnob (master,   "master",   "Master");

    // Amp model menu.
    addAndMakeVisible (ampBox);
    ampBox.addItemList ({ "Modern", "Fender Clean", "Plexi", "JCM800", "Rectifier" }, 1);
    ampAttachment = std::make_unique<ComboBoxAttachment> (processor.apvts, "amp_model", ampBox);

    // Cab voicing menu.
    addAndMakeVisible (cabBox);
    cabBox.addItemList ({ "Modern 4x12", "Vintage 4x12" }, 1);
    cabModelAttachment = std::make_unique<ComboBoxAttachment> (processor.apvts, "cab_model", cabBox);

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

    // Cabinet IR buttons.
    addAndMakeVisible (loadIRButton);
    loadIRButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load a cabinet impulse response", juce::File{}, "*.wav;*.aif;*.aiff");

        const auto flags = juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles;

        fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file.existsAsFile())
                processor.loadCabIR (file);
        });
    };

    addAndMakeVisible (resetCabButton);
    resetCabButton.onClick = [this] { processor.resetCab(); };

    // NAM amp capture controls.
    addAndMakeVisible (namButton);
    namButton.setColour (juce::ToggleButton::textColourId, juce::Colour (0xff5ec8ff));
    namAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "nam_on", namButton);

    addAndMakeVisible (loadNamButton);
    loadNamButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load a NAM amp capture (.nam)", juce::File{}, "*.nam");

        const auto flags = juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles;

        fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (! file.existsAsFile())
                return;

            juce::String error;
            if (processor.loadNam (file, error))
            {
                // Turn the NAM amp on automatically once a model loads.
                if (auto* p = processor.apvts.getParameter ("nam_on"))
                    p->setValueNotifyingHost (1.0f);
                updateNamStatus();
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync (
                    juce::AlertWindow::WarningIcon, "Could not load NAM model", error);
            }
        });
    };

    addAndMakeVisible (namStatus);
    namStatus.setColour (juce::Label::textColourId, juce::Colours::grey);
    namStatus.setFont (juce::FontOptions (12.0f));
    updateNamStatus();

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

    setSize (sideMargin * 2 + 7 * knobSize + 6 * knobGap, pedalTop + pedalH + sideMargin);
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

void AmpForgeAudioProcessorEditor::updateNamStatus()
{
    if (processor.namHasModel())
    {
        juce::String txt = "NAM: " + processor.namModelName();

        const double esr = processor.namExpectedSampleRate();
        const double sr  = processor.getCurrentSampleRate();
        if (esr > 0.0 && std::abs (esr - sr) > 1.0)
            txt << "   (model " << juce::String (esr / 1000.0, 1) << "k / DAW "
                << juce::String (sr / 1000.0, 1) << "k \xe2\x80\x94 match rates for best tone)";

        namStatus.setText (txt, juce::dontSendNotification);
    }
    else
    {
        namStatus.setText ("No NAM capture loaded \xe2\x80\x94 load a .nam from tone3000.com", juce::dontSendNotification);
    }
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
    auto pedalArea = juce::Rectangle<int> (sideMargin, pedalTop,
                                           getWidth() - sideMargin * 2, pedalH);
    g.setColour (juce::Colour (0xff26262c));
    g.fillRoundedRectangle (pedalArea.toFloat(), 6.0f);
    g.setColour (juce::Colour (0xffff7b3d));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("DISTORTION  (DS-1 style)", pedalArea.reduced (12, 8),
                juce::Justification::topLeft);
}

void AmpForgeAudioProcessorEditor::resized()
{
    // Header menus, right side: Amp model + Presets.
    presetBox.setBounds (getWidth() - sideMargin - 150, 16, 150, 26);
    ampBox.setBounds (getWidth() - sideMargin - 150 - 8 - 150, 16, 150, 26);

    // Amp knob row.
    Knob* ampKnobs[] = { &gate, &drive, &bass, &mid, &treble, &presence, &master };
    int x = sideMargin;
    for (auto* k : ampKnobs)
    {
        k->label.setBounds  (x, ampLabelY, knobSize, 18);
        k->slider.setBounds (x, ampKnobY, knobSize, ampKnobH);
        x += knobSize + knobGap;
    }

    // Strip under the amp knobs: cabinet toggle + voicing menu + IR buttons.
    cabButton.setBounds (sideMargin, stripY, 90, stripH);
    cabBox.setBounds (sideMargin + 90 + 8, stripY, 130, stripH);
    resetCabButton.setBounds (getWidth() - sideMargin - 90, stripY, 90, stripH);
    loadIRButton.setBounds (getWidth() - sideMargin - 90 - 8 - 90, stripY, 90, stripH);

    // NAM row: toggle + load button + status text.
    namButton.setBounds (sideMargin, namY, 90, namH);
    loadNamButton.setBounds (sideMargin + 90 + 8, namY, 100, namH);
    namStatus.setBounds (sideMargin + 90 + 8 + 100 + 12, namY,
                         getWidth() - (sideMargin * 2 + 90 + 8 + 100 + 12), namH);

    // Pedal row (inside the backdrop drawn in paint()).
    ds1Button.setBounds (sideMargin + 12, pedalTop + pedalH - 34, 90, 24);

    Knob* pedalKnobs[] = { &dist, &pedalTone, &pedalLevel };
    const int pKnobH = pedalH - 34;
    int px = getWidth() - sideMargin - 3 * knobSize - 2 * knobGap;
    for (auto* k : pedalKnobs)
    {
        k->label.setBounds  (px, pedalTop + 6, knobSize, 16);
        k->slider.setBounds (px, pedalTop + 22, knobSize, pKnobH);
        px += knobSize + knobGap;
    }
}

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

    // Shared module metrics for the DRIVE and EFFECTS panels. Each module is a
    // toggle (title) over a row of three small knobs.
    constexpr int modKnob      = 58;
    constexpr int modKnobGap   = 6;
    constexpr int modTextH     = 14;
    constexpr int modGap       = 16;
    constexpr int modW         = 3 * modKnob + 2 * modKnobGap;  // 186

    constexpr int panelHeaderH = 20;
    constexpr int panelToggleH = 22;
    constexpr int panelLabelH  = 14;
    constexpr int panelSliderH = modKnob + modTextH;
    constexpr int panelH       = panelHeaderH + panelToggleH + 4 + panelLabelH + panelSliderH + 12;

    // DRIVE panel (3 modules: Overdrive | DS-1 | DS-2).
    constexpr int driveTop  = namY + namH + 12;
    constexpr int driveRowW = 3 * modW + 2 * modGap;

    // EFFECTS panel (5 modules: Comp | Double | Chorus | Delay | Reverb).
    constexpr int fxTop  = driveTop + panelH + 12;
    constexpr int fxRowW = 5 * modW + 4 * modGap;

    // Y offsets within a panel, given its top.
    constexpr int panelToggleY (int top) { return top + panelHeaderH; }
    constexpr int panelLabelY  (int top) { return panelToggleY (top) + panelToggleH + 4; }
    constexpr int panelSliderY (int top) { return panelLabelY (top) + panelLabelH; }

    // X of module index i within a panel.
    constexpr int moduleX (int i) { return sideMargin + i * (modW + modGap); }
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

    // --- Drive pedals (pre-amp): Overdrive (SD-1) | DS-1 | DS-2 -------------
    addKnob (odDrive, "od_drive", "Drive", modKnob);
    addKnob (odTone,  "od_tone",  "Tone",  modKnob);
    addKnob (odLevel, "od_level", "Level", modKnob);
    addAndMakeVisible (odButton);
    odButton.setColour (juce::ToggleButton::textColourId, juce::Colour (0xffffc24d));
    odAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "od_on", odButton);

    addKnob (dist,       "ds1_dist",  "Dist",  modKnob);
    addKnob (pedalTone,  "ds1_tone",  "Tone",  modKnob);
    addKnob (pedalLevel, "ds1_level", "Level", modKnob);
    addAndMakeVisible (ds1Button);
    ds1Button.setColour (juce::ToggleButton::textColourId, juce::Colour (0xffff7b3d));
    ds1Attachment = std::make_unique<ButtonAttachment> (processor.apvts, "ds1_on", ds1Button);

    addKnob (ds2Dist,  "ds2_dist",  "Dist",  modKnob);
    addKnob (ds2Tone,  "ds2_tone",  "Tone",  modKnob);
    addKnob (ds2Level, "ds2_level", "Level", modKnob);
    addAndMakeVisible (ds2Button);
    ds2Button.setColour (juce::ToggleButton::textColourId, juce::Colour (0xffff5c5c));
    ds2Attachment = std::make_unique<ButtonAttachment> (processor.apvts, "ds2_on", ds2Button);

    addAndMakeVisible (ds2ModeBox);
    ds2ModeBox.addItemList ({ "Mode I", "Mode II (Turbo)" }, 1);
    ds2ModeAttachment = std::make_unique<ComboBoxAttachment> (processor.apvts, "ds2_mode", ds2ModeBox);

    // Cabinet toggle.
    addAndMakeVisible (cabButton);
    cabAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "cab_on", cabButton);

    // --- FX modules: compressor (pre-amp) + chorus / delay / reverb --------
    addKnob (compThresh, "comp_thresh", "Thresh", modKnob);
    addKnob (compRatio,  "comp_ratio",  "Ratio",  modKnob);
    addKnob (compMakeup, "comp_makeup", "Makeup", modKnob);
    addAndMakeVisible (compButton);
    compButton.setColour (juce::ToggleButton::textColourId, juce::Colour (0xff8fe36a));
    compAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "comp_on", compButton);

    addKnob (choRate,  "cho_rate",  "Rate",  modKnob);
    addKnob (choDepth, "cho_depth", "Depth", modKnob);
    addKnob (choMix,   "cho_mix",   "Mix",   modKnob);
    addAndMakeVisible (choButton);
    choButton.setColour (juce::ToggleButton::textColourId, juce::Colour (0xffc78fff));
    choAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "cho_on", choButton);

    addKnob (dlyTime, "dly_time", "Time", modKnob);
    addKnob (dlyFb,   "dly_fb",   "Fbk",  modKnob);
    addKnob (dlyMix,  "dly_mix",  "Mix",  modKnob);
    addAndMakeVisible (dlyButton);
    dlyButton.setColour (juce::ToggleButton::textColourId, juce::Colour (0xff5ec8ff));
    dlyAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "dly_on", dlyButton);

    addKnob (revSize, "rev_size", "Size", modKnob);
    addKnob (revDamp, "rev_damp", "Damp", modKnob);
    addKnob (revMix,  "rev_mix",  "Mix",  modKnob);
    addAndMakeVisible (revButton);
    revButton.setColour (juce::ToggleButton::textColourId, juce::Colour (0xff59b6e8));
    revAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "rev_on", revButton);

    addKnob (dblAmount, "dbl_amount", "Amt",   modKnob);
    addKnob (dblWidth,  "dbl_width",  "Width", modKnob);
    addKnob (dblDetune, "dbl_detune", "Dtune", modKnob);
    addAndMakeVisible (dblButton);
    dblButton.setColour (juce::ToggleButton::textColourId, juce::Colour (0xfff2a65a));
    dblAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "dbl_on", dblButton);

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

    // Standalone-only: audio starts muted for feedback safety; this button
    // goes live. Hidden entirely when hosted in a DAW.
    if (processor.isStandalone())
    {
        addAndMakeVisible (audioEnableButton);
        audioEnableButton.onClick = [this]
        {
            processor.setOutputMuted (! processor.isOutputMuted());
            updateAudioEnableButton();
        };
        updateAudioEnableButton();
    }

    const int ampRowW  = 7 * knobSize + 6 * knobGap;
    const int contentW = juce::jmax (ampRowW, juce::jmax (driveRowW, fxRowW));
    setSize (sideMargin * 2 + contentW, fxTop + panelH + sideMargin);
}

void AmpForgeAudioProcessorEditor::updateAudioEnableButton()
{
    const bool muted = processor.isOutputMuted();
    audioEnableButton.setButtonText (muted ? juce::CharPointer_UTF8 ("\xf0\x9f\x94\x87  MUTED \xe2\x80\x94 Click to Enable Audio")
                                           : juce::CharPointer_UTF8 ("\xf0\x9f\x94\x8a  Audio Live"));
    audioEnableButton.setColour (juce::TextButton::buttonColourId,
                                 muted ? juce::Colour (0xffb23a3a) : juce::Colour (0xff2f7d3a));
}

void AmpForgeAudioProcessorEditor::addKnob (Knob& knob,
                                            const juce::String& paramID,
                                            const juce::String& text,
                                            int textBoxWidth)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxWidth, textBoxH);
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
                << juce::String (sr / 1000.0, 1)
                << juce::String (juce::CharPointer_UTF8 ("k \xe2\x80\x94 match rates for best tone)"));

        namStatus.setText (txt, juce::dontSendNotification);
    }
    else
    {
        namStatus.setText (juce::CharPointer_UTF8 ("No NAM capture loaded \xe2\x80\x94 load a .nam from tone3000.com"), juce::dontSendNotification);
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

    // Draws a panel backdrop with a title and dividers between its modules.
    auto drawPanel = [&g, this] (int top, const juce::String& title,
                                 int numModules, juce::Colour titleColour)
    {
        auto area = juce::Rectangle<int> (sideMargin, top,
                                          getWidth() - sideMargin * 2, panelH);
        g.setColour (juce::Colour (0xff26262c));
        g.fillRoundedRectangle (area.toFloat(), 6.0f);
        g.setColour (titleColour);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (title, area.reduced (12, 6), juce::Justification::topLeft);

        g.setColour (juce::Colour (0xff34343c));
        for (int i = 1; i < numModules; ++i)
        {
            const int dx = moduleX (i) - modGap / 2;
            g.drawVerticalLine (dx, (float) panelToggleY (top), (float) (top + panelH - 8));
        }
    };

    drawPanel (driveTop, juce::CharPointer_UTF8 ("DRIVE / DISTORTION  (SD-1 \xc2\xb7 DS-1 \xc2\xb7 DS-2)"), 3, juce::Colour (0xffff7b3d));
    drawPanel (fxTop,    "EFFECTS",                                          5, juce::Colour (0xff5ec8ff));
}

void AmpForgeAudioProcessorEditor::resized()
{
    // Header menus, right side: Amp model + Presets.
    presetBox.setBounds (getWidth() - sideMargin - 150, 16, 150, 26);
    ampBox.setBounds (getWidth() - sideMargin - 150 - 8 - 150, 16, 150, 26);

    // Standalone audio-enable button: centred in the header between the title
    // and the menus.
    if (processor.isStandalone())
    {
        const int bx = sideMargin + 210;
        const int bw = juce::jmax (240, ampBox.getX() - 12 - bx);
        audioEnableButton.setBounds (bx, 16, juce::jmin (300, bw), 26);
    }

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

    // A module = toggle (title) over a row of three knobs.
    struct Mod { juce::ToggleButton* toggle; Knob* knobs[3]; };
    auto layoutModule = [] (int top, int index, const Mod& m)
    {
        const int mx = moduleX (index);
        m.toggle->setBounds (mx, panelToggleY (top), modW, panelToggleH);

        int kx = mx;
        for (auto* k : m.knobs)
        {
            k->label.setBounds  (kx, panelLabelY (top),  modKnob, panelLabelH);
            k->slider.setBounds (kx, panelSliderY (top), modKnob, panelSliderH);
            kx += modKnob + modKnobGap;
        }
    };

    // DRIVE panel: Overdrive | DS-1 | DS-2.
    layoutModule (driveTop, 0, { &odButton,  { &odDrive, &odTone, &odLevel } });
    layoutModule (driveTop, 1, { &ds1Button, { &dist,    &pedalTone, &pedalLevel } });
    layoutModule (driveTop, 2, { &ds2Button, { &ds2Dist, &ds2Tone,   &ds2Level } });

    // DS-2 shares its header row with the Turbo mode selector, so give the
    // toggle a fixed width and place the mode menu beside it.
    const int ds2x = moduleX (2);
    ds2Button.setBounds (ds2x, panelToggleY (driveTop), 62, panelToggleH);
    ds2ModeBox.setBounds (ds2x + 66, panelToggleY (driveTop), modW - 66, panelToggleH);

    // EFFECTS panel: Comp | Double | Chorus | Delay | Reverb.
    layoutModule (fxTop, 0, { &compButton, { &compThresh, &compRatio, &compMakeup } });
    layoutModule (fxTop, 1, { &dblButton,  { &dblAmount,  &dblWidth,  &dblDetune  } });
    layoutModule (fxTop, 2, { &choButton,  { &choRate,    &choDepth,  &choMix     } });
    layoutModule (fxTop, 3, { &dlyButton,  { &dlyTime,    &dlyFb,     &dlyMix     } });
    layoutModule (fxTop, 4, { &revButton,  { &revSize,    &revDamp,   &revMix     } });
}

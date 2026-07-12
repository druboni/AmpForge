#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ids
{
    constexpr auto drive    = "drive";
    constexpr auto bass     = "bass";
    constexpr auto mid      = "mid";
    constexpr auto treble   = "treble";
    constexpr auto presence = "presence";
    constexpr auto master   = "master";
    constexpr auto cabOn    = "cab_on";
    constexpr auto namOn    = "nam_on";
    constexpr auto ampModel = "amp_model";
    constexpr auto cabModel = "cab_model";
    constexpr auto gate     = "gate";

    constexpr auto ds1On    = "ds1_on";
    constexpr auto ds1Dist  = "ds1_dist";
    constexpr auto ds1Tone  = "ds1_tone";
    constexpr auto ds1Level = "ds1_level";

    // Overdrive pedal (SD-1 style, pre-amp).
    constexpr auto odOn     = "od_on";
    constexpr auto odDrive  = "od_drive";
    constexpr auto odTone   = "od_tone";
    constexpr auto odLevel  = "od_level";

    // DS-2 Turbo Distortion pedal (pre-amp).
    constexpr auto ds2On    = "ds2_on";
    constexpr auto ds2Dist  = "ds2_dist";
    constexpr auto ds2Tone  = "ds2_tone";
    constexpr auto ds2Level = "ds2_level";
    constexpr auto ds2Mode  = "ds2_mode";

    // Compressor (front of chain).
    constexpr auto compOn     = "comp_on";
    constexpr auto compThresh = "comp_thresh";
    constexpr auto compRatio  = "comp_ratio";
    constexpr auto compMakeup = "comp_makeup";

    // Doubler (post-cabinet — stereo double-tracker).
    constexpr auto dblOn     = "dbl_on";
    constexpr auto dblAmount = "dbl_amount";
    constexpr auto dblWidth  = "dbl_width";
    constexpr auto dblDetune = "dbl_detune";

    // Chorus (post-cabinet).
    constexpr auto choOn    = "cho_on";
    constexpr auto choRate  = "cho_rate";
    constexpr auto choDepth = "cho_depth";
    constexpr auto choMix   = "cho_mix";

    // Delay (post-cabinet).
    constexpr auto dlyOn   = "dly_on";
    constexpr auto dlyTime = "dly_time";
    constexpr auto dlyFb   = "dly_fb";
    constexpr auto dlyMix  = "dly_mix";

    // Reverb (end of chain).
    constexpr auto revOn   = "rev_on";
    constexpr auto revSize = "rev_size";
    constexpr auto revDamp = "rev_damp";
    constexpr auto revMix  = "rev_mix";
}

AmpForgeAudioProcessor::AmpForgeAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
AmpForgeAudioProcessor::createParameterLayout()
{
    using FloatP  = juce::AudioParameterFloat;
    using BoolP   = juce::AudioParameterBool;
    using ChoiceP = juce::AudioParameterChoice;
    using Attrs   = juce::AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<ChoiceP> (
        juce::ParameterID { ids::ampModel, 1 }, "Amp",
        juce::StringArray { "Modern", "Fender Clean", "Plexi", "JCM800", "Rectifier" }, 0));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::gate, 1 }, "Gate",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    // --- Compressor (front of chain) ---------------------------------------
    params.push_back (std::make_unique<BoolP> (
        juce::ParameterID { ids::compOn, 1 }, "Comp On", false));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::compThresh, 1 }, "Comp Thresh",
        juce::NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -18.0f, Attrs().withLabel ("dB")));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::compRatio, 1 }, "Comp Ratio",
        juce::NormalisableRange<float> (1.0f, 20.0f, 0.1f), 4.0f, Attrs().withLabel (":1")));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::compMakeup, 1 }, "Comp Makeup",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f, Attrs().withLabel ("dB")));

    // --- DS-1 distortion pedal (pre-amp) -----------------------------------
    params.push_back (std::make_unique<BoolP> (
        juce::ParameterID { ids::ds1On, 1 }, "DS-1 On", false));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::ds1Dist, 1 }, "Dist",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.6f));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::ds1Tone, 1 }, "Pedal Tone",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::ds1Level, 1 }, "Pedal Level",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));

    // --- Overdrive pedal (SD-1 style, pre-amp) -----------------------------
    params.push_back (std::make_unique<BoolP> (
        juce::ParameterID { ids::odOn, 1 }, "OD On", false));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::odDrive, 1 }, "OD Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::odTone, 1 }, "OD Tone",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::odLevel, 1 }, "OD Level",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));

    // --- DS-2 Turbo Distortion pedal (pre-amp) -----------------------------
    params.push_back (std::make_unique<BoolP> (
        juce::ParameterID { ids::ds2On, 1 }, "DS-2 On", false));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::ds2Dist, 1 }, "DS-2 Dist",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.6f));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::ds2Tone, 1 }, "DS-2 Tone",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::ds2Level, 1 }, "DS-2 Level",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));
    params.push_back (std::make_unique<ChoiceP> (
        juce::ParameterID { ids::ds2Mode, 1 }, "DS-2 Mode",
        juce::StringArray { "Mode I", "Mode II (Turbo)" }, 0));

    // --- Amp ----------------------------------------------------------------
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::drive, 1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 40.0f, 0.1f), 12.0f, Attrs().withLabel ("dB")));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::bass, 1 }, "Bass",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::mid, 1 }, "Mid",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::treble, 1 }, "Treble",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::presence, 1 }, "Presence",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::master, 1 }, "Master",
        juce::NormalisableRange<float> (-40.0f, 12.0f, 0.1f), -6.0f, Attrs().withLabel ("dB")));
    params.push_back (std::make_unique<BoolP> (
        juce::ParameterID { ids::cabOn, 1 }, "Cabinet", true));
    params.push_back (std::make_unique<ChoiceP> (
        juce::ParameterID { ids::cabModel, 1 }, "Cab",
        juce::StringArray { "Modern 4x12", "Vintage 4x12" }, 0));
    params.push_back (std::make_unique<BoolP> (
        juce::ParameterID { ids::namOn, 1 }, "NAM Amp", false));

    // --- Doubler (post-cabinet — stereo double-tracker) --------------------
    params.push_back (std::make_unique<BoolP> (
        juce::ParameterID { ids::dblOn, 1 }, "Doubler On", false));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::dblAmount, 1 }, "Doubler Amount",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::dblWidth, 1 }, "Doubler Width",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.85f));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::dblDetune, 1 }, "Doubler Detune",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));

    // --- Chorus (post-cabinet) ---------------------------------------------
    params.push_back (std::make_unique<BoolP> (
        juce::ParameterID { ids::choOn, 1 }, "Chorus On", false));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::choRate, 1 }, "Chorus Rate",
        juce::NormalisableRange<float> (0.05f, 8.0f, 0.01f), 1.0f, Attrs().withLabel ("Hz")));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::choDepth, 1 }, "Chorus Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::choMix, 1 }, "Chorus Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));

    // --- Delay (post-cabinet) ----------------------------------------------
    params.push_back (std::make_unique<BoolP> (
        juce::ParameterID { ids::dlyOn, 1 }, "Delay On", false));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::dlyTime, 1 }, "Delay Time",
        juce::NormalisableRange<float> (20.0f, 1500.0f, 1.0f), 350.0f, Attrs().withLabel ("ms")));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::dlyFb, 1 }, "Delay Feedback",
        juce::NormalisableRange<float> (0.0f, 0.95f, 0.001f), 0.35f));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::dlyMix, 1 }, "Delay Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f));

    // --- Reverb (end of chain) ---------------------------------------------
    params.push_back (std::make_unique<BoolP> (
        juce::ParameterID { ids::revOn, 1 }, "Reverb On", false));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::revSize, 1 }, "Reverb Size",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::revDamp, 1 }, "Reverb Damping",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<FloatP> (
        juce::ParameterID { ids::revMix, 1 }, "Reverb Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f));

    return { params.begin(), params.end() };
}

const std::vector<AmpForgeAudioProcessor::Preset>& AmpForgeAudioProcessor::getPresets()
{
    // amp_model indices: 0 Modern, 1 Fender Clean, 2 Plexi, 3 JCM800, 4 Rectifier.
    // fxOff = every pedal/effect toggle cleared, so selecting a classic preset
    // gives a predictable starting point. Showcase presets below override these.
    static const std::map<juce::String, float> fxOff = {
        { ids::odOn, 0.f }, { ids::ds2On, 0.f },
        { ids::compOn, 0.f }, { ids::choOn, 0.f }, { ids::dlyOn, 0.f }, { ids::revOn, 0.f }
    };

    auto withFxOff = [] (std::map<juce::String, float> v)
    {
        for (const auto& [id, val] : fxOff)
            v.emplace (id, val);
        return v;
    };

    static const std::vector<Preset> presets = {
        { "Fender Clean", withFxOff ({ { ids::ampModel, 1.f }, { ids::ds1On, 0.f }, { ids::gate, 0.f },  { ids::drive, 4.f },  { ids::bass, 0.5f }, { ids::mid, 0.55f }, { ids::treble, 0.6f },  { ids::presence, 0.3f },  { ids::master, -6.f }, { ids::cabOn, 1.f }, { ids::cabModel, 0.f } }) },
        { "Plexi Crunch", withFxOff ({ { ids::ampModel, 2.f }, { ids::ds1On, 0.f }, { ids::gate, 0.f },  { ids::drive, 22.f }, { ids::bass, 0.6f }, { ids::mid, 0.6f },  { ids::treble, 0.6f },  { ids::presence, 0.5f },  { ids::master, -9.f }, { ids::cabOn, 1.f }, { ids::cabModel, 1.f } }) },
        { "JCM800 Lead",  withFxOff ({ { ids::ampModel, 3.f }, { ids::ds1On, 0.f }, { ids::gate, 0.3f }, { ids::drive, 30.f }, { ids::bass, 0.55f },{ ids::mid, 0.65f }, { ids::treble, 0.6f },  { ids::presence, 0.55f }, { ids::master, -11.f },{ ids::cabOn, 1.f }, { ids::cabModel, 0.f } }) },
        { "Rectifier",    withFxOff ({ { ids::ampModel, 4.f }, { ids::ds1On, 0.f }, { ids::gate, 0.4f }, { ids::drive, 34.f }, { ids::bass, 0.65f },{ ids::mid, 0.4f },  { ids::treble, 0.6f },  { ids::presence, 0.5f },  { ids::master, -12.f },{ ids::cabOn, 1.f }, { ids::cabModel, 0.f } }) },
        { "DS-1 into JCM", withFxOff ({ { ids::ampModel, 3.f }, { ids::ds1On, 1.f }, { ids::ds1Dist, 0.7f }, { ids::ds1Tone, 0.6f }, { ids::ds1Level, 0.7f }, { ids::gate, 0.3f }, { ids::drive, 18.f }, { ids::bass, 0.5f }, { ids::mid, 0.6f }, { ids::treble, 0.6f }, { ids::presence, 0.5f }, { ids::master, -10.f }, { ids::cabOn, 1.f }, { ids::cabModel, 0.f } }) },
        { "Ambient Clean", { { ids::ampModel, 1.f }, { ids::ds1On, 0.f }, { ids::gate, 0.f }, { ids::drive, 3.f }, { ids::bass, 0.5f }, { ids::mid, 0.5f }, { ids::treble, 0.6f }, { ids::presence, 0.35f }, { ids::master, -6.f }, { ids::cabOn, 1.f }, { ids::cabModel, 1.f },
                             { ids::compOn, 1.f }, { ids::compThresh, -22.f }, { ids::compRatio, 3.f }, { ids::compMakeup, 6.f },
                             { ids::choOn, 1.f }, { ids::choRate, 0.6f }, { ids::choDepth, 0.35f }, { ids::choMix, 0.4f },
                             { ids::dlyOn, 1.f }, { ids::dlyTime, 420.f }, { ids::dlyFb, 0.35f }, { ids::dlyMix, 0.28f },
                             { ids::revOn, 1.f }, { ids::revSize, 0.7f }, { ids::revDamp, 0.4f }, { ids::revMix, 0.35f } } },
        { "Compressed Lead", { { ids::ampModel, 3.f }, { ids::ds1On, 0.f }, { ids::gate, 0.3f }, { ids::drive, 32.f }, { ids::bass, 0.5f }, { ids::mid, 0.7f }, { ids::treble, 0.6f }, { ids::presence, 0.55f }, { ids::master, -11.f }, { ids::cabOn, 1.f }, { ids::cabModel, 0.f },
                               { ids::compOn, 1.f }, { ids::compThresh, -20.f }, { ids::compRatio, 4.f }, { ids::compMakeup, 5.f },
                               { ids::choOn, 0.f },
                               { ids::dlyOn, 1.f }, { ids::dlyTime, 500.f }, { ids::dlyFb, 0.3f }, { ids::dlyMix, 0.22f },
                               { ids::revOn, 1.f }, { ids::revSize, 0.5f }, { ids::revDamp, 0.5f }, { ids::revMix, 0.2f } } },
        { "SD-1 Boost", { { ids::ampModel, 3.f }, { ids::ds1On, 0.f }, { ids::ds2On, 0.f }, { ids::compOn, 0.f }, { ids::choOn, 0.f }, { ids::dlyOn, 0.f }, { ids::revOn, 0.f }, { ids::gate, 0.2f }, { ids::drive, 26.f }, { ids::bass, 0.55f }, { ids::mid, 0.65f }, { ids::treble, 0.6f }, { ids::presence, 0.5f }, { ids::master, -10.f }, { ids::cabOn, 1.f }, { ids::cabModel, 0.f },
                          { ids::odOn, 1.f }, { ids::odDrive, 0.35f }, { ids::odTone, 0.6f }, { ids::odLevel, 0.8f } } },
        { "DS-2 Turbo Lead", { { ids::ampModel, 4.f }, { ids::ds1On, 0.f }, { ids::odOn, 0.f }, { ids::compOn, 0.f }, { ids::choOn, 0.f }, { ids::gate, 0.4f }, { ids::drive, 24.f }, { ids::bass, 0.6f }, { ids::mid, 0.5f }, { ids::treble, 0.6f }, { ids::presence, 0.55f }, { ids::master, -12.f }, { ids::cabOn, 1.f }, { ids::cabModel, 0.f },
                               { ids::ds2On, 1.f }, { ids::ds2Dist, 0.7f }, { ids::ds2Tone, 0.55f }, { ids::ds2Level, 0.7f }, { ids::ds2Mode, 1.f },
                               { ids::dlyOn, 1.f }, { ids::dlyTime, 450.f }, { ids::dlyFb, 0.3f }, { ids::dlyMix, 0.2f },
                               { ids::revOn, 1.f }, { ids::revSize, 0.5f }, { ids::revDamp, 0.5f }, { ids::revMix, 0.18f } } },
    };
    return presets;
}

void AmpForgeAudioProcessor::applyPreset (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    for (const auto& [id, value] : presets[(size_t) index].values)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void AmpForgeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) getTotalNumOutputChannels();

    gate.prepare (sampleRate);
    compressor.prepare (spec);
    overdrive.prepare (spec);
    pedal.prepare (spec);
    ds2.prepare (spec);
    engine.prepare (spec);
    cabinet.prepare (spec);
    doubler.prepare (spec);
    chorus.prepare (spec);
    delay.prepare (spec);
    reverb.prepare (spec);
    nam.prepare (sampleRate, samplesPerBlock);
    masterSmoothed.reset (sampleRate, 0.02);

    // Every oversampled pre-amp stage runs in series (each adds its own latency).
    setLatencySamples (juce::roundToInt (overdrive.getLatencySamples()
                                         + pedal.getLatencySamples()
                                         + ds2.getLatencySamples()
                                         + engine.getLatencySamples()));
}

bool AmpForgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void AmpForgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // A guitar is a mono source arriving on a single input channel. Sum the
    // inputs to mono and spread across all channels so the amp is centered
    // instead of only coming out of one side.
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    if (numIn > 0 && numOut > 1)
    {
        const int numSamples = buffer.getNumSamples();
        for (int i = 0; i < numSamples; ++i)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < numIn; ++ch)
                mono += buffer.getSample (ch, i);

            for (int ch = 0; ch < numOut; ++ch)
                buffer.setSample (ch, i, mono);
        }
    }

    // Noise gate (runs on the raw guitar before everything else).
    gate.setAmount (apvts.getRawParameterValue (ids::gate)->load());

    // Amp voicing + cabinet voicing.
    engine.setModel (static_cast<AmpEngine::Model> (
        (int) apvts.getRawParameterValue (ids::ampModel)->load()));
    cabinet.setVoicing (apvts.getRawParameterValue (ids::cabModel)->load() < 0.5f
        ? CabinetSim::Voicing::Modern4x12 : CabinetSim::Voicing::Vintage4x12);

    // Compressor parameters.
    compressor.setEnabled   (apvts.getRawParameterValue (ids::compOn)->load() > 0.5f);
    compressor.setThreshold (apvts.getRawParameterValue (ids::compThresh)->load());
    compressor.setRatio     (apvts.getRawParameterValue (ids::compRatio)->load());
    compressor.setMakeup    (apvts.getRawParameterValue (ids::compMakeup)->load());

    // Overdrive (SD-1 style) parameters.
    overdrive.setEnabled (apvts.getRawParameterValue (ids::odOn)->load() > 0.5f);
    overdrive.setDrive   (apvts.getRawParameterValue (ids::odDrive)->load());
    overdrive.setTone    (apvts.getRawParameterValue (ids::odTone)->load());
    overdrive.setLevel   (apvts.getRawParameterValue (ids::odLevel)->load());

    // DS-1 pedal parameters.
    pedal.setEnabled (apvts.getRawParameterValue (ids::ds1On)->load() > 0.5f);
    pedal.setDist    (apvts.getRawParameterValue (ids::ds1Dist)->load());
    pedal.setTone    (apvts.getRawParameterValue (ids::ds1Tone)->load());
    pedal.setLevel   (apvts.getRawParameterValue (ids::ds1Level)->load());

    // DS-2 Turbo Distortion parameters.
    ds2.setEnabled (apvts.getRawParameterValue (ids::ds2On)->load() > 0.5f);
    ds2.setDist    (apvts.getRawParameterValue (ids::ds2Dist)->load());
    ds2.setTone    (apvts.getRawParameterValue (ids::ds2Tone)->load());
    ds2.setLevel   (apvts.getRawParameterValue (ids::ds2Level)->load());
    ds2.setMode    ((int) apvts.getRawParameterValue (ids::ds2Mode)->load());

    // Post-amp effects parameters.
    doubler.setEnabled (apvts.getRawParameterValue (ids::dblOn)->load() > 0.5f);
    doubler.setAmount  (apvts.getRawParameterValue (ids::dblAmount)->load());
    doubler.setWidth   (apvts.getRawParameterValue (ids::dblWidth)->load());
    doubler.setDetune  (apvts.getRawParameterValue (ids::dblDetune)->load());

    chorus.setEnabled (apvts.getRawParameterValue (ids::choOn)->load() > 0.5f);
    chorus.setRate    (apvts.getRawParameterValue (ids::choRate)->load());
    chorus.setDepth   (apvts.getRawParameterValue (ids::choDepth)->load());
    chorus.setMix     (apvts.getRawParameterValue (ids::choMix)->load());

    delay.setEnabled  (apvts.getRawParameterValue (ids::dlyOn)->load() > 0.5f);
    delay.setTimeMs   (apvts.getRawParameterValue (ids::dlyTime)->load());
    delay.setFeedback (apvts.getRawParameterValue (ids::dlyFb)->load());
    delay.setMix      (apvts.getRawParameterValue (ids::dlyMix)->load());

    reverb.setEnabled (apvts.getRawParameterValue (ids::revOn)->load() > 0.5f);
    reverb.setSize    (apvts.getRawParameterValue (ids::revSize)->load());
    reverb.setDamping (apvts.getRawParameterValue (ids::revDamp)->load());
    reverb.setMix     (apvts.getRawParameterValue (ids::revMix)->load());

    // Amp parameters.
    engine.setDriveDb   (apvts.getRawParameterValue (ids::drive)->load());
    engine.setBass      (apvts.getRawParameterValue (ids::bass)->load());
    engine.setMid       (apvts.getRawParameterValue (ids::mid)->load());
    engine.setTreble    (apvts.getRawParameterValue (ids::treble)->load());
    engine.setPresence  (apvts.getRawParameterValue (ids::presence)->load());
    masterSmoothed.setTargetValue (juce::Decibels::decibelsToGain (
        apvts.getRawParameterValue (ids::master)->load()));

    const bool cabBypass = apvts.getRawParameterValue (ids::cabOn)->load() < 0.5f;
    const bool namWanted = apvts.getRawParameterValue (ids::namOn)->load() > 0.5f;

    juce::dsp::AudioBlock<float> block (buffer);

    // Chain: gate -> compressor -> overdrive -> DS-1 -> DS-2 -> [ NAM capture OR
    //        algorithmic amp ] -> cabinet -> doubler -> chorus -> delay -> reverb -> master.
    gate.process (block);
    compressor.process (block);
    overdrive.process (block);
    pedal.process (block);
    ds2.process (block);

    // The NAM capture already models a whole amp, so when it runs it replaces
    // the algorithmic amp rather than stacking with it.
    const bool namRan = namWanted && nam.process (block);
    if (! namRan)
        engine.processBlock (block);

    cabinet.process (block, cabBypass);

    // Post-amp effects (studio / FX-loop style).
    doubler.process (block);
    chorus.process (block);
    delay.process (block);
    reverb.process (block);

    // Master.
    const int numSamples = buffer.getNumSamples();
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = masterSmoothed.getNextValue();
        for (int ch = 0; ch < numOut; ++ch)
            buffer.setSample (ch, i, buffer.getSample (ch, i) * g);
    }

    // Standalone launch safety: output stays silent until the user clicks
    // "Enable Audio" in the editor. Never engages when hosted in a DAW.
    if (outputMuted.load())
        buffer.clear();
}

juce::AudioProcessorEditor* AmpForgeAudioProcessor::createEditor()
{
    return new AmpForgeAudioProcessorEditor (*this);
}

void AmpForgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AmpForgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AmpForgeAudioProcessor();
}

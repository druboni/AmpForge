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

    constexpr auto ds1On    = "ds1_on";
    constexpr auto ds1Dist  = "ds1_dist";
    constexpr auto ds1Tone  = "ds1_tone";
    constexpr auto ds1Level = "ds1_level";
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
    using FloatP = juce::AudioParameterFloat;
    using BoolP  = juce::AudioParameterBool;
    using Attrs  = juce::AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

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

    return { params.begin(), params.end() };
}

const std::vector<AmpForgeAudioProcessor::Preset>& AmpForgeAudioProcessor::getPresets()
{
    static const std::vector<Preset> presets = {
        { "Clean",     { { ids::ds1On, 0.f }, { ids::drive, 4.f },  { ids::bass, 0.5f }, { ids::mid, 0.55f }, { ids::treble, 0.55f }, { ids::presence, 0.35f }, { ids::master, -6.f }, { ids::cabOn, 1.f } } },
        { "Crunch",    { { ids::ds1On, 0.f }, { ids::drive, 18.f }, { ids::bass, 0.55f },{ ids::mid, 0.6f },  { ids::treble, 0.55f }, { ids::presence, 0.45f }, { ids::master, -8.f }, { ids::cabOn, 1.f } } },
        { "Lead",      { { ids::ds1On, 0.f }, { ids::drive, 30.f }, { ids::bass, 0.6f }, { ids::mid, 0.7f },  { ids::treble, 0.5f },  { ids::presence, 0.5f },  { ids::master, -10.f },{ ids::cabOn, 1.f } } },
        { "DS-1 Lead", { { ids::ds1On, 1.f }, { ids::ds1Dist, 0.75f }, { ids::ds1Tone, 0.6f }, { ids::ds1Level, 0.7f }, { ids::drive, 10.f }, { ids::bass, 0.5f }, { ids::mid, 0.5f }, { ids::treble, 0.55f }, { ids::presence, 0.5f }, { ids::master, -9.f }, { ids::cabOn, 1.f } } },
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
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) getTotalNumOutputChannels();

    pedal.prepare (spec);
    engine.prepare (spec);

    setLatencySamples (juce::roundToInt (pedal.getLatencySamples() + engine.getLatencySamples()));
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

    // Pedal parameters.
    pedal.setEnabled (apvts.getRawParameterValue (ids::ds1On)->load() > 0.5f);
    pedal.setDist    (apvts.getRawParameterValue (ids::ds1Dist)->load());
    pedal.setTone    (apvts.getRawParameterValue (ids::ds1Tone)->load());
    pedal.setLevel   (apvts.getRawParameterValue (ids::ds1Level)->load());

    // Amp parameters.
    engine.setDriveDb   (apvts.getRawParameterValue (ids::drive)->load());
    engine.setBass      (apvts.getRawParameterValue (ids::bass)->load());
    engine.setMid       (apvts.getRawParameterValue (ids::mid)->load());
    engine.setTreble    (apvts.getRawParameterValue (ids::treble)->load());
    engine.setPresence  (apvts.getRawParameterValue (ids::presence)->load());
    engine.setMasterDb  (apvts.getRawParameterValue (ids::master)->load());
    engine.setCabBypass (apvts.getRawParameterValue (ids::cabOn)->load() < 0.5f);

    juce::dsp::AudioBlock<float> block (buffer);
    pedal.process (block);   // stompbox first, into the amp
    engine.processBlock (block);
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

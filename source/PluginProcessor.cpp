#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ids
{
    constexpr auto drive    = "drive";
    constexpr auto bass     = "bass";
    constexpr auto mid      = "mid";
    constexpr auto treble   = "treble";
    constexpr auto presence = "presence";
    constexpr auto cab      = "cab";
    constexpr auto master   = "master";
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
    using P = juce::AudioParameterFloat;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<P> (
        juce::ParameterID { ids::drive, 1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 40.0f, 0.1f), 12.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    params.push_back (std::make_unique<P> (
        juce::ParameterID { ids::bass, 1 }, "Bass",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back (std::make_unique<P> (
        juce::ParameterID { ids::mid, 1 }, "Mid",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back (std::make_unique<P> (
        juce::ParameterID { ids::treble, 1 }, "Treble",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back (std::make_unique<P> (
        juce::ParameterID { ids::presence, 1 }, "Presence",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));

    params.push_back (std::make_unique<P> (
        juce::ParameterID { ids::cab, 1 }, "Cabinet",
        juce::NormalisableRange<float> (1000.0f, 12000.0f, 1.0f, 0.3f), 5000.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    params.push_back (std::make_unique<P> (
        juce::ParameterID { ids::master, 1 }, "Master",
        juce::NormalisableRange<float> (-40.0f, 12.0f, 0.1f), -6.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return { params.begin(), params.end() };
}

void AmpForgeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) getTotalNumOutputChannels();

    engine.prepare (spec);
}

bool AmpForgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    // Input and output layouts must match.
    return layouts.getMainInputChannelSet() == out;
}

void AmpForgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Clear any output channels that don't have matching inputs.
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Pull current parameter values into the engine.
    engine.setDriveDb   (apvts.getRawParameterValue (ids::drive)->load());
    engine.setBass      (apvts.getRawParameterValue (ids::bass)->load());
    engine.setMid       (apvts.getRawParameterValue (ids::mid)->load());
    engine.setTreble    (apvts.getRawParameterValue (ids::treble)->load());
    engine.setPresence  (apvts.getRawParameterValue (ids::presence)->load());
    engine.setCabCutoff (apvts.getRawParameterValue (ids::cab)->load());
    engine.setMasterDb  (apvts.getRawParameterValue (ids::master)->load());

    juce::dsp::AudioBlock<float> block (buffer);
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

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AmpForgeAudioProcessor();
}

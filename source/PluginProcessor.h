#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/AmpEngine.h"
#include "dsp/DistortionPedal.h"

class AmpForgeAudioProcessor : public juce::AudioProcessor
{
public:
    AmpForgeAudioProcessor();
    ~AmpForgeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Preset support (driven from the editor's menu).
    struct Preset { juce::String name; std::map<juce::String, float> values; };
    static const std::vector<Preset>& getPresets();
    void applyPreset (int index);

    // Public so the editor can attach controls to it.
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    DistortionPedal pedal;
    AmpEngine       engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpForgeAudioProcessor)
};

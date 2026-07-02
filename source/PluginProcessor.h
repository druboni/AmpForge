#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/AmpEngine.h"
#include "dsp/DistortionPedal.h"
#include "dsp/CabinetSim.h"
#include "dsp/NamProcessor.h"
#include "dsp/NoiseGate.h"

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

    // Cabinet impulse response (driven from the editor's buttons).
    void loadCabIR (const juce::File& f) { cabinet.loadIR (f); }
    void resetCab()                      { cabinet.loadDefaultIR(); }

    // NAM amp captures (.nam from tone3000.com etc). loadNam() is heavy —
    // call it from the message thread.
    bool loadNam (const juce::File& f, juce::String& error) { return nam.loadModel (f, error); }
    void clearNam()                        { nam.clear(); }
    bool namHasModel()                     { return nam.hasModel(); }
    juce::String namModelName()            { return nam.getModelName(); }
    double namExpectedSampleRate()         { return nam.getExpectedSampleRate(); }
    double getCurrentSampleRate() const    { return currentSampleRate; }

    // Public so the editor can attach controls to it.
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    NoiseGate       gate;
    DistortionPedal pedal;
    AmpEngine       engine;
    NamProcessor    nam;
    CabinetSim      cabinet;

    juce::SmoothedValue<float> masterSmoothed { 1.0f };
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpForgeAudioProcessor)
};

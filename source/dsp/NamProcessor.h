#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <filesystem>
#include <memory>
#include <vector>

// NAM core (Neural Amp Modeler). NAM_SAMPLE defaults to double.
#include "dsp.h"
#include "get_dsp.h"

/**
    NamProcessor — hosts a Neural Amp Modeler capture (.nam), e.g. downloaded
    from tone3000.com.

    Loading a model allocates and builds a neural network, so loadModel() must
    be called from a non-audio thread. The audio thread only ever try-locks;
    if a load is mid-swap it simply leaves the block untouched for one buffer.

    The NAM core works in double; we convert to/from the plugin's float buffers.
    Models are mono (one in, one out) — we drive them from channel 0 and copy
    the result to every output channel.
*/
class NamProcessor
{
public:
    void prepare (double sr, int maxBlock)
    {
        sampleRate   = sr;
        maxBlockSize = juce::jmax (1, maxBlock);
        inBuf.assign ((size_t) maxBlockSize, 0.0);
        outBuf.assign ((size_t) maxBlockSize, 0.0);

        const juce::SpinLock::ScopedLockType sl (lock);
        if (model != nullptr)
            model->ResetAndPrewarm (sampleRate, maxBlockSize);
    }

    // Load a .nam file. Returns false (with an error message) on failure.
    // Call from the message thread — this is heavy.
    bool loadModel (const juce::File& file, juce::String& error)
    {
        std::unique_ptr<nam::DSP> loaded;
        try
        {
            loaded = nam::get_dsp (std::filesystem::path (file.getFullPathName().toStdString()));
        }
        catch (const std::exception& e) { error = e.what(); return false; }
        catch (...)                     { error = "Unknown error loading model"; return false; }

        if (loaded == nullptr)
        {
            error = "Not a valid NAM model file";
            return false;
        }

        loaded->ResetAndPrewarm (sampleRate, maxBlockSize);
        const double esr = loaded->GetExpectedSampleRate();

        // Normalize the model's output to a standard loudness so captures don't
        // come in quiet (or blaring). NAM's convention is a -18 dB target.
        float gain = 1.0f;
        if (loaded->HasLoudness())
        {
            constexpr double targetLoudnessDb = -18.0;
            gain = (float) std::pow (10.0, (targetLoudnessDb - loaded->GetLoudness()) / 20.0);
        }

        {
            const juce::SpinLock::ScopedLockType sl (lock);
            model = std::move (loaded);
            modelNameStr = file.getFileNameWithoutExtension();
        }

        outputGain.store (gain);
        expectedSampleRate.store (esr);
        hasModelFlag.store (true);
        return true;
    }

    void clear()
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        model.reset();
        hasModelFlag.store (false);
    }

    bool   hasModel()               const { return hasModelFlag.load(); }
    double getExpectedSampleRate()  const { return expectedSampleRate.load(); }
    juce::String getModelName()           { const juce::SpinLock::ScopedLockType sl (lock); return modelNameStr; }

    // Process in place. Returns true if the model actually ran.
    bool process (juce::dsp::AudioBlock<float>& block)
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || model == nullptr)
            return false;

        const int n = juce::jmin ((int) block.getNumSamples(), maxBlockSize);

        for (int i = 0; i < n; ++i)
            inBuf[(size_t) i] = (double) block.getSample (0, i);

        double* inP  = inBuf.data();
        double* outP = outBuf.data();
        model->process (&inP, &outP, n);

        const float g = outputGain.load();
        const auto numChannels = block.getNumChannels();
        for (size_t ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < n; ++i)
                block.setSample ((int) ch, i, (float) outBuf[(size_t) i] * g);

        return true;
    }

private:
    double sampleRate   = 48000.0;
    int    maxBlockSize = 512;

    std::vector<double> inBuf, outBuf;

    std::unique_ptr<nam::DSP> model;
    juce::SpinLock lock;

    std::atomic<bool>   hasModelFlag { false };
    std::atomic<double> expectedSampleRate { -1.0 };
    std::atomic<float>  outputGain { 1.0f };
    juce::String        modelNameStr;
};

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/DenoiserEngine.h"
#include "Metering.h"

class SuppressorProcessor : public juce::AudioProcessor,
                            private juce::AsyncUpdater
{
public:
    SuppressorProcessor();
    ~SuppressorProcessor() override = default;

    // ------------------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // ------------------------------------------------------------------
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ------------------------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;

    // Bounded, race-free telemetry; never expose mutable DSP state to the editor.
    bool readMeters (suppressor::MeterSnapshot& result) const noexcept { return meters.read (result); }
    float meterSampleRate() const noexcept { return displaySampleRate.load(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void handleAsyncUpdate() override;
    suppressor::EngineParams readTargets (double sampleRate) const;

    suppressor::DenoiserEngine engine;

    suppressor::Metering meters;
    std::atomic<float> displaySampleRate { 44100.0f };
    std::atomic<bool>  latencyDirty { false };
    std::atomic<bool>  humAutoFinished { false };

    bool humLearningNow = false;
    bool bandsLearningNow = false;
    int currentLatency = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuppressorProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

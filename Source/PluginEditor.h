#pragma once

#include "PluginProcessor.h"
#include <affine_ui/affine_ui.h>

namespace SuppressorTheme
{
/** Broadcast finish: hammertone enamel, bakelite knobs and amber-lit meters. */
affine::Theme theme();
}

class ListenButton final : public affine::KeyButton
{
public:
    ListenButton() : affine::KeyButton ("Listen removed") {}
};

class SuppressorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SuppressorEditor (SuppressorProcessor&);
    ~SuppressorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    int getControlParameterIndex (juce::Component&) override;

    static constexpr int width = 880, height = 560;

private:
    void timerCallback() override;
    void print (juce::Graphics&);
    SuppressorProcessor& proc;
    affine::Theme theme;
    affine::LookAndFeel look;
    affine::Faceplate faceplate;
    affine::Knob threshold, strength, release;
    ListenButton listen;
    juce::AudioProcessorValueTreeState::ButtonAttachment listenAttachment;
    affine::NeedleMeter inputMeter, reduction, outputMeter;
    affine::DisplayLabel status, hostSettings;
    juce::Label meterSummary;
    juce::TooltipWindow tooltips { this, 700 };
    suppressor::MeterSnapshot meter;
    uint32_t lastSequence = 0;
    double lastUpdateMs = 0.0;
    bool fresh = false, inputClipped = false, outputClipped = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuppressorEditor)
};

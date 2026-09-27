#pragma once

#include "PluginProcessor.h"
#include <affine_ui/affine_ui.h>

namespace SuppressorTheme
{
/** Graphite anodising with a glacier-cyan emission: the Suppressor finish of the Affine family. */
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

    static constexpr int width = 800, height = 530;

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
    affine::NeedleMeter reduction;
    affine::LadderMeter inputLadder, outputLadder;
    affine::DisplayLabel status, inputPeak, outputPeak, hostSettings;
    juce::Label meterSummary;
    juce::TooltipWindow tooltips { this, 700 };
    suppressor::MeterSnapshot meter;
    uint32_t lastSequence = 0;
    double lastUpdateMs = 0.0;
    bool fresh = false, auditionShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuppressorEditor)
};

#pragma once

#include "PluginProcessor.h"
#include <affine_ui/affine_ui.h>

namespace SuppressorTheme
{
/** Hi-Fi finish: a black glass front with polished end caps, blue-lit meters and silver knobs. */
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

    static constexpr int width = 880, height = 540;

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
    affine::LadderMeter inputBar, outputBar;
    affine::IlluminatedLegends status;
    affine::DisplayLabel inputPeak, outputPeak, hostSettings;
    juce::Label meterSummary;
    juce::TooltipWindow tooltips { this, 700 };
    suppressor::MeterSnapshot meter;
    uint32_t lastSequence = 0;
    double lastUpdateMs = 0.0;
    bool fresh = false, auditionShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuppressorEditor)
};

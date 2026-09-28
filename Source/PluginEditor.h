#pragma once

#include "PluginProcessor.h"
#include <affine_ui/affine_ui.h>

namespace SuppressorTheme
{
/** The family theme with Suppressor's teal screen. */
affine::Theme theme();
}

class ListenButton final : public affine::KeyButton
{
public:
    ListenButton() : affine::KeyButton ("Listen removed") {}
};

/**
    The screen: gain reduction as a scrolling history, with input and output
    peak bars. Only real snapshots are plotted; stretches without audio stay empty.
*/
class ReductionScreen final : public juce::Component
{
public:
    ReductionScreen();

    void setTheme (const affine::Theme&);
    /** Appends one frame. `reductionDb` is ignored when `hasReduction` is false. */
    void push (float reductionDb, bool hasReduction, float inputDb, float outputDb, bool live, bool inputClip,
               bool outputClip, bool removed, bool multiband);
    void paint (juce::Graphics&) override;

private:
    static constexpr int frames = 180; // six seconds at 30 Hz

    void paintHistory (juce::Graphics&, juce::Rectangle<float>);
    void paintLevel (juce::Graphics&, juce::Rectangle<float>, const juce::String& name, float level, bool clipped,
                     juce::Colour);

    affine::Theme theme;
    affine::render::GlowText readoutGlow;
    std::array<float, frames> history {};
    int head = 0, valid = 0;
    float input = -100.0f, output = -100.0f, latest = 0.0f;
    bool live = false, current = false, inputClipped = false, outputClipped = false, removedSignal = false, multi = false;
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

    static constexpr int width = 820, height = 540;

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
    ReductionScreen screen;
    affine::DisplayLabel status, hostSettings;
    juce::Label meterSummary;
    juce::TooltipWindow tooltips { this, 700 };
    suppressor::MeterSnapshot meter;
    uint32_t lastSequence = 0;
    double lastUpdateMs = 0.0;
    bool fresh = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuppressorEditor)
};

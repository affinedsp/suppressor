#pragma once

#include "PluginProcessor.h"
#include "PluginLookAndFeel.h"

class SuppressorDial final : public juce::Slider
{
public:
    SuppressorDial (juce::AudioProcessorValueTreeState&, const juce::String& id,
                    const juce::String& help);
    ~SuppressorDial() override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }
    void enablementChanged() override;
    void beginEntry();
    void cancelInteraction();
    juce::RangedAudioParameter& parameter;

private:
    void finishEntry (bool commit);
    void setWithGesture (double);
    void showMenu();
    juce::TextEditor entry;
    juce::Point<float> lastDrag;
    double dragProportion = 0.0;
    std::unique_ptr<juce::Slider::ScopedDragNotification> drag;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class ListenButton final : public juce::TextButton
{
public:
    ListenButton() : juce::TextButton ("Listen removed") {}
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (isEnabled() && key.isKeyCode (' '))
        {
            setToggleState (! getToggleState(), juce::sendNotificationSync);
            return true;
        }
        return juce::TextButton::keyPressed (key);
    }
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

private:
    void timerCallback() override;
    void drawLevel (juce::Graphics&, int y, const juce::String&, float peak, bool clip);
    SuppressorProcessor& proc;
    SuppressorLookAndFeel theme;
    SuppressorDial threshold, strength, release;
    ListenButton listen;
    juce::AudioProcessorValueTreeState::ButtonAttachment listenAttachment;
    juce::Label status, meterSummary, hostSettings;
    juce::TooltipWindow tooltips { this, 700 };
    suppressor::MeterSnapshot meter;
    uint32_t lastSequence = 0;
    double lastUpdateMs = 0.0;
    bool fresh = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuppressorEditor)
};

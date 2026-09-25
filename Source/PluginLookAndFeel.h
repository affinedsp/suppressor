#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace SuppressorTheme
{
inline const juce::Colour canvas { 0xff0b0e11 }, surface { 0xff12171c },
    surfaceRaised { 0xff171d23 }, divider { 0xff2a333c }, primaryText { 0xffe8edf1 },
    secondaryText { 0xff89939d }, controlOutline { 0xff66717b }, accent { 0xffb8f24a },
    warning { 0xffe7a84b }, error { 0xffff8080 };
juce::Font makeFont (float size, bool semibold = false);
}

class SuppressorLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SuppressorLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float,
                           juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getPopupMenuFont() override;
};

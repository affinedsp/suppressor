#include "PluginLookAndFeel.h"
#include "SuppressorAssets.h"

juce::Font SuppressorTheme::makeFont (float size, bool semibold)
{
    static auto regular = juce::Typeface::createSystemTypefaceFor (
        SuppressorAssets::BarlowCondensedRegular_ttf, SuppressorAssets::BarlowCondensedRegular_ttfSize);
    static auto bold = juce::Typeface::createSystemTypefaceFor (
        SuppressorAssets::BarlowCondensedSemiBold_ttf, SuppressorAssets::BarlowCondensedSemiBold_ttfSize);
    return juce::Font (juce::FontOptions (semibold ? bold : regular).withHeight (size));
}

SuppressorLookAndFeel::SuppressorLookAndFeel()
{
    using namespace SuppressorTheme;
    setColour (juce::Label::textColourId, secondaryText);
    setColour (juce::TextEditor::backgroundColourId, surfaceRaised);
    setColour (juce::TextEditor::textColourId, primaryText);
    setColour (juce::TextEditor::outlineColourId, controlOutline);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.3f));
    setColour (juce::CaretComponent::caretColourId, primaryText);
    setColour (juce::TextButton::textColourOffId, primaryText);
    setColour (juce::TextButton::textColourOnId, warning);
    setColour (juce::PopupMenu::backgroundColourId, surface);
    setColour (juce::PopupMenu::textColourId, primaryText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, surfaceRaised);
    setColour (juce::PopupMenu::highlightedTextColourId, accent);
    setColour (juce::TooltipWindow::backgroundColourId, surfaceRaised);
    setColour (juce::TooltipWindow::textColourId, primaryText);
    setColour (juce::TooltipWindow::outlineColourId, controlOutline);
}

void SuppressorLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                             float position, float start, float end, juce::Slider& s)
{
    using namespace SuppressorTheme;
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (8.0f);
    const auto centre = bounds.getCentre();
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 4.0f;
    g.setColour (surfaceRaised);
    g.fillEllipse (bounds.reduced (7.0f));
    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start, end, true);
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start,
                        start + position * (end - start), true);
    g.setColour (controlOutline);
    g.strokePath (track, juce::PathStrokeType (3.0f));
    g.setColour (s.isEnabled() ? accent : controlOutline);
    g.strokePath (value, juce::PathStrokeType (3.0f));
    const auto angle = start + position * (end - start);
    const auto point = centre + juce::Point<float> (std::sin (angle), -std::cos (angle)) * radius;
    g.fillEllipse (point.x - 3.0f, point.y - 3.0f, 6.0f, 6.0f);
    g.setColour (s.isEnabled() ? primaryText : secondaryText);
    g.setFont (makeFont (21.0f, true));
    g.drawText (s.getTextFromValue (s.getValue()), bounds, juce::Justification::centred);
    if (s.hasKeyboardFocus (true))
    {
        g.setColour (accent);
        g.drawRoundedRectangle (s.getLocalBounds().toFloat().reduced (1.0f), 6.0f, 1.5f);
    }
    else if (s.isMouseOver() && s.isEnabled())
    {
        g.setColour (controlOutline);
        g.drawRoundedRectangle (s.getLocalBounds().toFloat().reduced (1.0f), 6.0f, 1.0f);
    }
}

void SuppressorLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                 const juce::Colour&, bool hover, bool down)
{
    using namespace SuppressorTheme;
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (down ? divider : surfaceRaised);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (b.getToggleState() ? warning : (hover ? primaryText : controlOutline));
    g.drawRoundedRectangle (r, 5.0f, b.hasKeyboardFocus (true) ? 2.0f : 1.0f);
    if (b.hasKeyboardFocus (true)) g.drawRoundedRectangle (r.reduced (4.0f), 3.0f, 1.0f);
}

juce::Font SuppressorLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return SuppressorTheme::makeFont (17.0f, true);
}
juce::Font SuppressorLookAndFeel::getPopupMenuFont() { return SuppressorTheme::makeFont (17.0f); }

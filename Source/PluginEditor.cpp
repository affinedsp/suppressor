#include "PluginEditor.h"
#include <cstdlib>

namespace
{
using namespace SuppressorTheme;
const auto bg = canvas, panel = surface, line = divider, text = primaryText,
           secondary = secondaryText, neutral = controlOutline;

void label (juce::Graphics& g, const juce::String& s, juce::Rectangle<int> r,
            float size, juce::Colour colour, juce::Justification align = juce::Justification::left)
{
    g.setColour (colour);
    g.setFont (SuppressorTheme::makeFont (size, size >= 20.0f));
    g.drawText (s, r, align);
}
float db (float peak) { return juce::Decibels::gainToDecibels (peak, -100.0f); }
juce::String levelText (float peak) { return peak < 0.00001f ? "-inf" : juce::String (db (peak), 1); }
} // namespace

SuppressorDial::SuppressorDial (juce::AudioProcessorValueTreeState& state, const juce::String& id,
                                 const juce::String& help)
    : parameter (*state.getParameter (id)), attachment (state, id, *this)
{
    setComponentID (id);
    setName (parameter.getName (64));
    setTitle (getName());
    setDescription (help);
    setTooltip (help + " Double-click or Return to type. Shift-drag for fine adjustment. "
                       "Alt/Option-click to reset; right-click for the menu.");
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setTextValueSuffix (" " + parameter.getLabel());
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                         juce::MathConstants<float>::pi * 2.75f, true);
    setScrollWheelEnabled (false);
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    setDoubleClickReturnValue (false, 0.0);
    addChildComponent (entry);
    entry.setName (getName() + " exact value");
    entry.setJustification (juce::Justification::centred);
    entry.setFont (SuppressorTheme::makeFont (18.0f));
    entry.onReturnKey = [this] { finishEntry (true); };
    entry.onEscapeKey = [this] { finishEntry (false); };
    entry.onFocusLost = [this] { finishEntry (false); };
}

SuppressorDial::~SuppressorDial() { cancelInteraction(); }
void SuppressorDial::resized()
{
    juce::Slider::resized();
    entry.setBounds (getLocalBounds().withSizeKeepingCentre (100, 32));
}
void SuppressorDial::setWithGesture (double v)
{
    juce::Slider::ScopedDragNotification gesture (*this);
    setValue (v, juce::sendNotificationSync);
}
void SuppressorDial::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled()) return;
    grabKeyboardFocus();
    if (e.mods.isPopupMenu()) { showMenu(); return; }
    if (e.mods.isAltDown())
    {
        setWithGesture (parameter.convertFrom0to1 (parameter.getDefaultValue()));
        return;
    }
    lastDrag = e.position;
    dragProportion = valueToProportionOfLength (getValue());
    drag = std::make_unique<juce::Slider::ScopedDragNotification> (*this);
}
void SuppressorDial::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == nullptr) return;
    const auto delta = e.position - lastDrag;
    const double sensitivity = e.mods.isShiftDown() ? 2400.0 : 240.0;
    // Accumulate sub-step movement so fine drags work on the 0.1 dB grid.
    dragProportion = juce::jlimit (0.0, 1.0, dragProportion + (delta.x - delta.y) / sensitivity);
    setValue (proportionOfLengthToValue (dragProportion), juce::sendNotificationSync);
    lastDrag = e.position;
}
void SuppressorDial::mouseUp (const juce::MouseEvent&) { drag.reset(); }
void SuppressorDial::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! e.mods.isAltDown() && ! e.mods.isPopupMenu()) beginEntry();
}
void SuppressorDial::cancelInteraction()
{
    drag.reset();
    finishEntry (false);
}
void SuppressorDial::enablementChanged()
{
    if (! isEnabled()) cancelInteraction();
    repaint();
}
bool SuppressorDial::keyPressed (const juce::KeyPress& key)
{
    if (! isEnabled()) return false;
    if (key == juce::KeyPress::returnKey) { beginEntry(); return true; }
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey
        || code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey)
    {
        const double step = getComponentID() == "strength" ? 0.01 : 0.1;
        const int direction = code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey ? 1 : -1;
        const auto fine = key.getModifiers().isShiftDown() && getInterval() <= 0.0 ? 0.1 : 1.0;
        setWithGesture (getValue() + direction * step * fine);
        return true;
    }
    if (code == juce::KeyPress::homeKey)
    {
        setWithGesture (parameter.convertFrom0to1 (parameter.getDefaultValue()));
        return true;
    }
    return juce::Slider::keyPressed (key);
}
void SuppressorDial::beginEntry()
{
    if (! isEnabled()) return;
    drag.reset();
    entry.setText (getTextFromValue (getValue()), false);
    entry.setVisible (true);
    entry.grabKeyboardFocus();
    entry.selectAll();
}
void SuppressorDial::finishEntry (bool commit)
{
    if (! entry.isVisible()) return;
    const auto typed = entry.getText().trim();
    entry.setVisible (false);
    if (commit)
    {
        // Accept one finite number plus an optional matching unit. Do not let
        // the parameter's permissive parser turn malformed input into zero.
        auto number = typed;
        const auto unit = parameter.getLabel();
        if (number.endsWithIgnoreCase (unit)) number = number.dropLastCharacters (unit.length()).trimEnd();
        const auto utf8 = number.toRawUTF8();
        char* end = nullptr;
        const auto value = std::strtod (utf8, &end);
        if (number.containsOnly ("0123456789.+-") && end != utf8 && *end == '\0' && std::isfinite (value))
        {
            const double plain = getComponentID() == "strength" ? value / 100.0 : value;
            setWithGesture (plain);
        }
        grabKeyboardFocus();
    }
    repaint();
}
void SuppressorDial::showMenu()
{
    juce::PopupMenu menu;
    const juce::Component::SafePointer<SuppressorDial> safe (this);
    menu.addItem ("Enter value...", [safe] { if (safe != nullptr) safe->beginEntry(); });
    menu.addItem ("Reset to default", [safe]
    {
        if (safe != nullptr)
            safe->setWithGesture (safe->parameter.convertFrom0to1 (safe->parameter.getDefaultValue()));
    });
    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
        if (auto* host = editor->getHostContext())
            if (auto hostMenu = host->getContextMenuForParameter (&parameter))
                menu.addSubMenu ("Host", hostMenu->getEquivalentPopupMenu());
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

SuppressorEditor::SuppressorEditor (SuppressorProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p),
      threshold (p.apvts, "threshold", "Highs below this level are reduced. Raise it until noise between notes is suppressed."),
      strength (p.apvts, "strength", "Higher strength suppresses a wider high-frequency range. Zero is not bypass."),
      release (p.apvts, "release", "How quickly suppression returns after a note. Increase to preserve longer tails."),
      listenAttachment (p.apvts, "deltaAudition", listen)
{
    setLookAndFeel (&theme);
    int order = 1;
    for (auto* dial : { &threshold, &strength, &release })
    {
        addAndMakeVisible (*dial);
        dial->setExplicitFocusOrder (order++);
    }
    listen.setClickingTogglesState (true);
    listen.setComponentID ("deltaAudition");
    listen.setTitle ("Listen removed");
    listen.setTooltip ("Audition input minus processed audio, including filter phase differences; not isolated noise. Turn off to hear the processed signal.");
    listen.setExplicitFocusOrder (4);
    addAndMakeVisible (listen);
    for (auto* l : { &status, &meterSummary, &hostSettings })
    {
        addAndMakeVisible (*l);
        l->setFont (SuppressorTheme::makeFont (16.0f));
        l->setBorderSize (juce::BorderSize<int> (0));
    }
    status.setJustificationType (juce::Justification::centredRight);
    status.setName ("Processing status");
    meterSummary.setName ("Signal meters");
    meterSummary.setTooltip ("Input and output: peak across channels, dBFS. Reduction: deepest gate attenuation, not overall loudness loss. 300 ms peak decay; clips held for one second.");
    hostSettings.setJustificationType (juce::Justification::centredRight);
    hostSettings.setName ("Host settings");
    setSize (640, 460);
    // A reopened editor must see a NEW audio block before calling data live.
    proc.readMeters (meter);
    lastSequence = meter.sequence;
    lastUpdateMs = juce::Time::getMillisecondCounterHiRes() - 1000.0;
    status.setText ("No audio", juce::dontSendNotification);
    timerCallback();
    startTimerHz (30);
}
SuppressorEditor::~SuppressorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}
void SuppressorEditor::resized()
{
    status.setBounds (365, 24, 251, 28);
    meterSummary.setBounds (36, 224, 355, 18);
    threshold.setBounds (59, 277, 122, 116);
    strength.setBounds (259, 277, 122, 116);
    release.setBounds (459, 277, 122, 116);
    listen.setBounds (24, 412, 160, 32);
    hostSettings.setBounds (464, 419, 152, 20);
}
void SuppressorEditor::visibilityChanged()
{
    if (! isShowing())
        for (auto* dial : { &threshold, &strength, &release }) dial->cancelInteraction();
}
int SuppressorEditor::getControlParameterIndex (juce::Component& c)
{
    for (auto* dial : { &threshold, &strength, &release })
        if (&c == dial || dial->isParentOf (&c)) return dial->parameter.getParameterIndex();
    if (&c == &listen || listen.isParentOf (&c))
        return proc.apvts.getParameter ("deltaAudition")->getParameterIndex();
    return -1;
}
void SuppressorEditor::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    suppressor::MeterSnapshot next;
    if (proc.readMeters (next) && next.sequence != lastSequence)
    {
        meter = next;
        lastSequence = next.sequence;
        lastUpdateMs = now;
    }
    fresh = (meter.flags & suppressor::MeterSnapshot::active) != 0 && now - lastUpdateMs < 500.0;
    // A parameter change may precede the next audio block (or happen while
    // stopped). Do not relabel old output/reduction as the new routing mode.
    if ((meter.flags & suppressor::MeterSnapshot::bypassed) == 0)
        fresh = fresh
            && (((meter.flags & suppressor::MeterSnapshot::removed) != 0) == listen.getToggleState())
            && (((meter.flags & suppressor::MeterSnapshot::multiband) != 0)
                == (proc.apvts.getRawParameterValue ("bandMode")->load() > 0.5f));
    if (! isShowing()) return;

    using M = suppressor::MeterSnapshot;
    const bool bypass = fresh && (meter.flags & M::bypassed) != 0;
    const bool learning = fresh && (meter.flags & M::learning) != 0;
    const bool audition = listen.getToggleState() && ! bypass;
    listen.setButtonText (listen.getToggleState() ? "Stop listening" : "Listen removed");
    juce::String state = ! fresh ? "No audio" : bypass ? "Bypassed" : learning ? "Learning bands"
                        : meter.input < 0.00001f ? "No input" : audition ? "Listening to difference"
                        : meter.reduction > 0.5f ? "Suppressing" : "Passing signal";
    status.setText (state, juce::dontSendNotification);
    status.setColour (juce::Label::textColourId, audition ? warning : text);
    const bool multi = proc.apvts.getRawParameterValue ("bandMode")->load() > 0.5f;
    threshold.setEnabled (! multi);
    strength.setEnabled (! multi);

    juce::StringArray changed;
    for (auto* param : proc.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (ranged->paramID != "threshold" && ranged->paramID != "strength"
                && ranged->paramID != "release" && ranged->paramID != "deltaAudition"
                && std::abs (ranged->getValue() - ranged->getDefaultValue()) > 0.00001f)
                changed.add (ranged->getName (64) + ": " + ranged->getCurrentValueAsText());
    hostSettings.setText (changed.isEmpty() ? "" : "Host settings active", juce::dontSendNotification);
    hostSettings.setTooltip ("Legacy settings are preserved. Edit them in your host's parameter view.\n" + changed.joinIntoString ("\n"));
    hostSettings.setDescription (hostSettings.getTooltip());
    meterSummary.setText ("PEAK LEVEL  /  dBFS", juce::dontSendNotification);
    meterSummary.setDescription (! fresh ? "No current audio readings" :
        "Input " + levelText (meter.input) + " dBFS, " + (audition ? "removed " : "output ")
        + levelText (meter.output) + " dBFS. " + (bypass ? "Bypassed." : learning ? "Learning; reduction unavailable."
        : meter.input < 0.00001f ? "No input; reduction unavailable."
        : (multi ? "Maximum band reduction " : "High-band reduction ")
            + (meter.reduction >= 60.0f ? "at least 60" : juce::String (meter.reduction, 1)) + " dB."));
    if (fresh)
    {
        auto description = meterSummary.getDescription();
        if ((meter.flags & M::inputClip) != 0) description += " Input clipped.";
        if ((meter.flags & M::outputClip) != 0) description += " Output clipped.";
        meterSummary.setDescription (description);
    }
    repaint();
}
void SuppressorEditor::drawLevel (juce::Graphics& g, int y, const juce::String& name, float peak, bool clip)
{
    label (g, name, { 36, y, 66, 20 }, 16.0f, secondary);
    const auto track = juce::Rectangle<float> (106.0f, (float) y + 6.0f, 205.0f, 8.0f);
    g.setColour (line); g.fillRoundedRectangle (track, 2.0f);
    if (fresh)
    {
        g.setColour (name == "Input" ? secondary : accent);
        if (name == "Removed") g.setColour (warning);
        g.fillRoundedRectangle (track.withWidth (205.0f * juce::jlimit (0.0f, 1.0f, (db (peak) + 60.0f) / 60.0f)), 2.0f);
    }
    label (g, fresh ? levelText (peak) : "--", { 322, y - 1, 66, 22 }, 19.0f, text, juce::Justification::right);
    if (fresh && clip) label (g, "CLIP", { 319, y + 19, 69, 15 }, 14.0f, error, juce::Justification::right);
}
void SuppressorEditor::paint (juce::Graphics& g)
{
    using M = suppressor::MeterSnapshot;
    const bool bypass = fresh && (meter.flags & M::bypassed) != 0;
    const bool multi = proc.apvts.getRawParameterValue ("bandMode")->load() > 0.5f;
    const bool learning = fresh && (meter.flags & M::learning) != 0;
    const bool audition = listen.getToggleState() && ! bypass;
    const bool hasReduction = fresh && ! learning && (bypass || meter.input >= 0.00001f);
    g.fillAll (bg);
    label (g, "suppressor", { 24, 19, 270, 32 }, 30.0f, text);
    label (g, "GUITAR DI  /  NOISE SUPPRESSOR", { 25, 54, 340, 16 }, 14.0f, secondary);
    g.setColour (panel); g.fillRoundedRectangle (24.0f, 86.0f, 592.0f, 162.0f, 8.0f);
    drawLevel (g, 112, "Input", meter.input, (meter.flags & M::inputClip) != 0);
    drawLevel (g, 164, audition ? "Removed" : "Output", meter.output, (meter.flags & M::outputClip) != 0);
    for (int value : { -60, -36, -12, 0 })
    {
        const int x = 106 + (value + 60) * 205 / 60;
        g.setColour (neutral); g.drawVerticalLine (x, 195.0f, 199.0f);
        label (g, juce::String (value), { x - 14, 202, 28, 16 }, 14.0f, secondary, juce::Justification::centred);
    }
    g.setColour (line); g.drawVerticalLine (406, 106.0f, 230.0f);
    label (g, multi ? "MAX BAND REDUCTION" : "HIGH-BAND REDUCTION", { 423, 108, 178, 18 }, 14.0f, secondary);
    label (g, hasReduction ? (meter.reduction >= 60.0f ? "60+" : juce::String (meter.reduction, 1)) : "--", { 422, 134, 131, 56 }, 52.0f,
           hasReduction && meter.reduction > 0.5f ? accent : text);
    label (g, "dB", { 548, 158, 40, 24 }, 20.0f, secondary);
    const float amount = hasReduction ? juce::jlimit (0.0f, 1.0f, meter.reduction / 60.0f) : 0.0f;
    g.setColour (line); g.fillRect (424.0f, 202.0f, 172.0f, 6.0f);
    g.setColour (accent); g.fillRect (424.0f, 202.0f, 172.0f * amount, 6.0f);
    for (int value : { 0, 20, 40, 60 })
        label (g, juce::String (value), { 416 + value * 172 / 60, 215, 22, 17 }, 14.0f, secondary, juce::Justification::centred);

    label (g, "Threshold", { 24, 259, 192, 22 }, 18.0f, text, juce::Justification::centred);
    label (g, "Strength", { 224, 259, 192, 22 }, 18.0f, text, juce::Justification::centred);
    label (g, "Release", { 424, 259, 192, 22 }, 18.0f, text, juce::Justification::centred);
    label (g, multi ? "One-split only" : "Reduce highs below this level", { 24, 388, 192, 17 }, 14.0f, secondary, juce::Justification::centred);
    const auto cutoff = suppressor::DenoiserEngine::strengthToCutoff (strength.getValue(),
        proc.meterSampleRate());
    label (g, multi ? "One-split only" : "Above " + juce::String (cutoff / 1000.0, 1) + " kHz", { 224, 388, 192, 17 }, 14.0f, secondary, juce::Justification::centred);
    label (g, "How quickly highs close", { 424, 388, 192, 17 }, 14.0f, secondary, juce::Justification::centred);
    label (g, bypass ? "Monitor: dry input" : audition ? "Monitor: input minus processed" : "Monitor: processed signal",
           { 196, 419, 265, 20 }, 15.0f, audition ? warning : secondary);
}

#include "PluginEditor.h"

namespace
{
float db (float peak) { return juce::Decibels::gainToDecibels (peak, -100.0f); }
juce::String levelText (float peak) { return peak < 0.00001f ? "-inf" : juce::String (db (peak), 1); }
juce::String whole (double v) { return juce::String (juce::roundToInt (v)); }

// Layout, in logical pixels. The signal runs left to right across the meter bridge.
const juce::Rectangle<int> statusArea { 548, 26, 214, 30 };
const juce::Rectangle<int> hostSettingsArea { 548, 60, 214, 16 };
const juce::Rectangle<int> meterArea { 218, 90, 364, 210 };
const juce::Rectangle<int> inputColumn { 136, 110, 24, 160 }, outputColumn { 640, 110, 24, 160 };
const juce::Rectangle<int> inputReadout { 116, 276, 64, 20 }, outputReadout { 620, 276, 64, 20 };
const juce::Rectangle<int> summaryArea { 250, 302, 300, 14 };
const juce::Rectangle<float> suppressionFrame { 40.0f, 330.0f, 560.0f, 168.0f };
const juce::Rectangle<float> monitorFrame { 612.0f, 330.0f, 148.0f, 168.0f };
constexpr int knobAxis = 410;
constexpr int knobCentres[] { 150, 320, 490 };
const juce::Point<int> listenCentre { 686, 414 };
} // namespace

affine::Theme SuppressorTheme::theme()
{
    affine::Theme t;
    t.panel.base = juce::Colour (0xff2a3037);
    t.panel.brushing = 0.45f;
    t.knob.pointer = juce::Colour (0xff1c1f22);
    t.palette.accent = juce::Colour (0xff4fd6ff);
    return t;
}

SuppressorEditor::SuppressorEditor (SuppressorProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), theme (SuppressorTheme::theme()), look (theme),
      threshold (p.apvts, "threshold", "Highs below this level are reduced. Raise it until noise between notes is suppressed."),
      strength (p.apvts, "strength", "Higher strength suppresses a wider high-frequency range. Zero is not bypass."),
      release (p.apvts, "release", "How quickly suppression returns after a note. Increase to preserve longer tails."),
      listenAttachment (p.apvts, "deltaAudition", listen)
{
    setLookAndFeel (&look);
    setOpaque (true);

    int order = 1;
    for (auto* dial : { &threshold, &strength, &release })
    {
        dial->setTheme (theme);
        dial->setDiameter (affine::metrics::knobMedium);
        dial->setExplicitFocusOrder (order++);
        addAndMakeVisible (*dial);
    }
    threshold.setScale ({ -80, -60, -40, -20, 0 }, whole);
    strength.setScale ({ 0.0, 0.25, 0.5, 0.75, 1.0 }, [] (double v) { return whole (v * 100.0); });
    release.setScale ({ 2, 4, 8, 15, 30 }, whole);
    strength.setKeyboardStep (0.01);
    release.setKeyboardStep (0.1);

    listen.setClickingTogglesState (true);
    listen.setComponentID ("deltaAudition");
    listen.setTitle ("Listen removed");
    listen.setTooltip ("Audition input minus processed audio, including filter phase differences; not isolated noise. Turn off to hear the processed signal.");
    listen.setExplicitFocusOrder (4);
    listen.setTheme (theme);
    listen.setLampColour (theme.palette.attention);
    listen.setLegend ("Listen", "Removed");
    addAndMakeVisible (listen);

    affine::NeedleMeter::Scale scale;
    // Square-root law: small reductions stay readable, deep gating still fits.
    scale.toPosition = [] (float decibels) { return std::sqrt (juce::jlimit (0.0f, 1.0f, decibels / 60.0f)); };
    scale.majors = { 0, 5, 10, 20, 30, 40, 60 };
    scale.minors = { 1, 2, 3, 4, 6, 7, 8, 9, 15, 25, 35, 50 };
    scale.format = [] (float v) { return whole (v); };
    scale.unit = "dB";
    scale.caption = "High-band reduction";
    reduction.setTheme (theme);
    reduction.setScale (scale);
    reduction.setBacklight (juce::Colour (0xfff2e4c4));
    addAndMakeVisible (reduction);

    for (auto* ladder : { &inputLadder, &outputLadder })
    {
        ladder->setTheme (theme);
        ladder->setRange (-60.0f, 0.0f, 20);
        ladder->setZones (-12.0f, -3.0f);
        addAndMakeVisible (*ladder);
    }

    status.setName ("Processing status");
    status.setTheme (theme);
    status.setJustificationType (juce::Justification::centredLeft);
    for (auto* readout : { &inputPeak, &outputPeak })
    {
        readout->setTheme (theme);
        readout->setShowsLamp (false);
        readout->setJustificationType (juce::Justification::centred);
        readout->setDisplayFont (affine::fonts::readout (13.5f));
        readout->setInterceptsMouseClicks (false, false);
        readout->setAccessible (false);
    }
    hostSettings.setName ("Host settings");
    hostSettings.setTheme (theme);
    hostSettings.setGlassVisible (false);
    hostSettings.setJustificationType (juce::Justification::centredRight);
    hostSettings.setDisplayFont (affine::fonts::label (11.5f, 0.2f));
    hostSettings.setEmission (theme.palette.silkscreen, 1.0f);
    hostSettings.setLampColour (theme.palette.attention);
    meterSummary.setName ("Signal meters");
    meterSummary.setTooltip ("Input and output: peak across channels, dBFS. Reduction: deepest gate attenuation, not overall loudness loss. 300 ms peak decay; clips held for one second.");
    meterSummary.setFont (affine::fonts::label (11.0f, 0.22f));
    meterSummary.setColour (juce::Label::textColourId, theme.palette.silkscreenDim);
    meterSummary.setJustificationType (juce::Justification::centred);
    meterSummary.setBorderSize (juce::BorderSize<int> (0));
    for (juce::Component* c : { static_cast<juce::Component*> (&status), static_cast<juce::Component*> (&meterSummary),
                                static_cast<juce::Component*> (&hostSettings), static_cast<juce::Component*> (&inputPeak),
                                static_cast<juce::Component*> (&outputPeak) })
        addAndMakeVisible (c);

    setSize (width, height);
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
    status.setBounds (statusArea);
    reduction.setBounds (meterArea);
    inputLadder.setBounds (inputColumn);
    outputLadder.setBounds (outputColumn);
    inputPeak.setBounds (inputReadout);
    outputPeak.setBounds (outputReadout);
    meterSummary.setBounds (summaryArea);
    int i = 0;
    for (auto* dial : { &threshold, &strength, &release })
        dial->setBounds (dial->getBoundsForCentre ({ knobCentres[i++], knobAxis }));
    listen.setBounds (juce::Rectangle<int> (96, 88).withCentre (listenCentre));
    hostSettings.setBounds (hostSettingsArea);
}

void SuppressorEditor::visibilityChanged()
{
    if (! isShowing())
        for (auto* dial : { &threshold, &strength, &release })
            dial->cancelInteraction();
}

int SuppressorEditor::getControlParameterIndex (juce::Component& c)
{
    for (auto* dial : { &threshold, &strength, &release })
        if (&c == dial || dial->isParentOf (&c))
            return dial->parameter.getParameterIndex();
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
    if (! isShowing())
        return;

    using M = suppressor::MeterSnapshot;
    const auto& palette = theme.palette;
    const bool bypass = fresh && (meter.flags & M::bypassed) != 0;
    const bool learning = fresh && (meter.flags & M::learning) != 0;
    const bool audition = listen.getToggleState() && ! bypass;
    const bool multi = proc.apvts.getRawParameterValue ("bandMode")->load() > 0.5f;
    listen.setButtonText (listen.getToggleState() ? "Stop listening" : "Listen removed");

    const juce::String state = ! fresh ? "No audio" : bypass ? "Bypassed" : learning ? "Learning bands"
                             : meter.input < 0.00001f ? "No input" : audition ? "Listening to difference"
                             : meter.reduction > 0.5f ? "Suppressing" : "Passing signal";
    status.setText (state, juce::dontSendNotification);
    if (! fresh)
        status.setEmission (palette.accent.withMultipliedBrightness (0.55f), 0.0f);
    else if (bypass)
        status.setEmission (palette.silkscreen, 0.0f);
    else if (audition || learning)
        status.setEmission (palette.attention, 1.0f);
    else
        status.setEmission (palette.accent, meter.input < 0.00001f ? 0.25f : 1.0f);

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

    const bool hasReduction = fresh && ! learning && (bypass || meter.input >= 0.00001f);
    reduction.setCaption (multi ? "Max band reduction" : "High-band reduction");
    reduction.setReading (meter.reduction, hasReduction);
    inputLadder.setLevel (db (meter.input), fresh);
    outputLadder.setLevel (db (meter.output), fresh);
    inputLadder.setClip (fresh && (meter.flags & M::inputClip) != 0);
    outputLadder.setClip (fresh && (meter.flags & M::outputClip) != 0);
    inputPeak.setText (fresh ? levelText (meter.input) : "--", juce::dontSendNotification);
    outputPeak.setText (fresh ? levelText (meter.output) : "--", juce::dontSendNotification);
    const auto peakColour = fresh ? palette.accent : palette.accent.withMultipliedBrightness (0.5f);
    inputPeak.setEmission (peakColour, fresh ? 1.0f : 0.0f);
    outputPeak.setEmission (audition ? palette.attention : peakColour, fresh ? 1.0f : 0.0f);

    if (audition != auditionShown)
    {
        auditionShown = audition;
        repaint (outputColumn.withY (88).withHeight (20).expanded (40, 0));
    }
}

void SuppressorEditor::print (juce::Graphics& g)
{
    using namespace affine::silkscreen;
    const auto& palette = theme.palette;
    wordmark (g, "Suppressor", "Guitar DI noise suppressor", { 40.0f, 22.0f }, palette);
    groove (g, 32.0f, static_cast<float> (width) - 32.0f, 80.0f);
    legend (g, "In", juce::Rectangle<int> (60, 16).withCentre ({ inputColumn.getCentreX(), 98 }).toFloat(), palette,
            juce::Justification::centred);
    frame (g, suppressionFrame, "Suppression", palette);
    frame (g, monitorFrame, "Monitor", palette);

    // Printed dBFS scales on the outer side of each column, and the clip legends.
    g.setFont (affine::fonts::label (9.5f, 0.02f));
    for (const auto* ladder : { &inputLadder, &outputLadder })
    {
        const auto span = ladder->getScaleSpan();
        const bool left = ladder == &inputLadder;
        const auto edge = left ? static_cast<float> (ladder->getX()) - 4.0f : static_cast<float> (ladder->getRight()) + 4.0f;
        for (int value : { 0, -6, -12, -24, -36, -48, -60 })
        {
            const auto y = span.getY() + span.getHeight() * static_cast<float> (-value) / 60.0f;
            g.setColour (palette.silkscreen.withAlpha (0.5f));
            g.fillRect (left ? edge - 4.0f : edge, y - 0.5f, 4.0f, 1.0f);
            g.setColour (palette.silkscreenDim);
            g.drawText (juce::String (value), juce::Rectangle<float> (24.0f, 10.0f).withCentre ({ left ? edge - 18.0f : edge + 18.0f, y }),
                        left ? juce::Justification::centredRight : juce::Justification::centredLeft, false);
        }
        const auto clip = ladder->getClipCentre();
        g.setColour (palette.danger.withAlpha (0.85f));
        g.setFont (affine::fonts::label (9.0f, 0.12f));
        g.drawText ("CLIP", juce::Rectangle<float> (30.0f, 10.0f).withCentre ({ left ? edge - 19.0f : edge + 19.0f, clip.y }),
                    left ? juce::Justification::centredRight : juce::Justification::centredLeft, false);
        g.setFont (affine::fonts::label (9.5f, 0.02f));
    }

    makersMark (g, { 34.0f, static_cast<float> (height) - 10.0f }, palette);
}

void SuppressorEditor::paint (juce::Graphics& g)
{
    faceplate.paint (g, getLocalBounds(), theme, [this] (juce::Graphics& plate) { print (plate); });
    // The output column is labelled with what it is actually monitoring.
    affine::silkscreen::legend (g, auditionShown ? "Removed" : "Out",
                                juce::Rectangle<int> (90, 16).withCentre ({ outputColumn.getCentreX(), 98 }).toFloat(),
                                theme.palette, juce::Justification::centred);
}

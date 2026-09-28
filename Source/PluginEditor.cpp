#include "PluginEditor.h"

namespace
{
float db (float peak) { return juce::Decibels::gainToDecibels (peak, -100.0f); }
juce::String levelText (float peak) { return peak < 0.00001f ? "-inf" : juce::String (db (peak), 1); }
juce::String whole (double v) { return juce::String (juce::roundToInt (v)); }

// Layout, in logical pixels. Polished end caps hold an 788 px glass front.
constexpr float capWidth = 46.0f;
const juce::Rectangle<int> statusArea { 430, 28, 380, 20 };
const juce::Rectangle<int> hostSettingsArea { 596, 58, 214, 16 };
const juce::Rectangle<int> meterArea { 76, 90, 396, 240 };
const juce::Rectangle<int> inputBarArea { 500, 128, 262, 24 }, outputBarArea { 500, 210, 262, 24 };
const juce::Rectangle<int> inputPeakArea { 766, 126, 58, 28 }, outputPeakArea { 766, 208, 58, 28 };
const juce::Rectangle<int> summaryArea { 500, 290, 262, 14 };
constexpr int knobAxis = 440;
constexpr int knobCentres[] { 160, 320, 480 };
const juce::Rectangle<int> listenArea { 610, 378, 150, 120 };

const juce::Colour brandGreen { 0xff54f28c }, legendWhite { 0xffe3ecf2 }, bypassRed { 0xffff5a4a };
} // namespace

affine::Theme SuppressorTheme::theme()
{
    affine::Theme t;
    t.panel.texture = affine::PanelFinish::Texture::glass;
    t.panel.base = juce::Colour (0xff050607);
    t.panel.sheen = 1.0f;

    using Material = affine::KnobFinish::Material;
    t.knob.cap = juce::Colour (0xffd4d7db);
    t.knob.body = juce::Colour (0xffc9ccd0);
    t.knob.capMaterial = Material::spunAluminium;
    t.knob.bodyMaterial = Material::polishedAluminium;
    t.knob.pointer = juce::Colour (0xff17181a);
    t.knob.index = juce::Colour (0xff17181a);

    auto& p = t.palette;
    p.silkscreen = legendWhite;
    p.silkscreenDim = juce::Colour (0xff93a2ad);
    p.accent = juce::Colour (0xff4aa8ff);
    p.attention = juce::Colour (0xffffb238);
    p.danger = juce::Colour (0xffff4a3d);
    p.glass = juce::Colour (0xff030405);
    p.glassTint = juce::Colour (0xff0a0e12);
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
    faceplate.setShowsScrews (false);

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
    listen.setStyle (affine::KeyButton::Style::silverSquare);
    listen.setKeySize (58.0f, 44.0f);
    listen.setLampColour (theme.palette.attention);
    listen.setLegend ("Listen", "Removed");
    addAndMakeVisible (listen);

    affine::NeedleMeter::Face blue;
    blue.backlight = juce::Colour (0xff3a8ef0);
    blue.ink = juce::Colour (0xff08131f);
    blue.mirror = false;
    blue.vignette = 2.4f;
    blue.twinLamps = true;
    blue.bezel = affine::NeedleMeter::Face::Bezel::flush;

    affine::NeedleMeter::Scale gr;
    // Square-root law: small reductions stay readable, deep gating still fits.
    gr.toPosition = [] (float decibels) { return std::sqrt (juce::jlimit (0.0f, 1.0f, decibels / 60.0f)); };
    gr.majors = { 0, 5, 10, 20, 30, 40, 60 };
    gr.minors = { 1, 2, 3, 4, 6, 7, 8, 9, 15, 25, 35, 50 };
    gr.format = [] (float v) { return whole (v); };
    gr.unit = "dB";
    gr.caption = "High-band reduction";
    reduction.setTheme (theme);
    reduction.setScale (gr);
    reduction.setFace (blue);
    addAndMakeVisible (reduction);

    for (auto* bar : { &inputBar, &outputBar })
    {
        bar->setTheme (theme);
        bar->setOrientation (affine::LadderMeter::Orientation::horizontal);
        bar->setRange (-60.0f, 0.0f, 30);
        bar->setZones (-12.0f, -3.0f);
        addAndMakeVisible (*bar);
    }

    status.setName ("Processing status");
    status.setTheme (theme);
    status.setLegends ({ "No signal", "Passing", "Suppressing", "Listen", "Learn", "Bypass" }, [] (const juce::String& text)
    {
        if (text == "Passing signal") return 1;
        if (text == "Suppressing") return 2;
        if (text == "Listening to difference") return 3;
        if (text == "Learning bands") return 4;
        if (text == "Bypassed") return 5;
        return 0;
    });
    for (auto* readout : { &inputPeak, &outputPeak })
    {
        readout->setTheme (theme);
        readout->setShowsLamp (false);
        readout->setGlassVisible (false);
        readout->setJustificationType (juce::Justification::centredRight);
        readout->setDisplayFont (affine::fonts::readout (15.0f));
        readout->setInterceptsMouseClicks (false, false);
        readout->setAccessible (false);
    }
    hostSettings.setName ("Host settings");
    hostSettings.setTheme (theme);
    hostSettings.setGlassVisible (false);
    hostSettings.setJustificationType (juce::Justification::centredRight);
    hostSettings.setDisplayFont (affine::fonts::label (11.5f, 0.2f));
    hostSettings.setEmission (legendWhite, 1.0f);
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
    hostSettings.setBounds (hostSettingsArea);
    reduction.setBounds (meterArea);
    inputBar.setBounds (inputBarArea);
    outputBar.setBounds (outputBarArea);
    inputPeak.setBounds (inputPeakArea);
    outputPeak.setBounds (outputPeakArea);
    meterSummary.setBounds (summaryArea);
    int i = 0;
    for (auto* dial : { &threshold, &strength, &release })
        dial->setBounds (dial->getBoundsForCentre ({ knobCentres[i++], knobAxis }));
    listen.setBounds (listenArea);
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
    status.setLitColour (bypass ? bypassRed : (audition || learning) ? palette.attention
                         : state == "Suppressing" ? brandGreen : legendWhite);

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
    inputBar.setLevel (db (meter.input), fresh);
    outputBar.setLevel (db (meter.output), fresh);
    inputBar.setClip (fresh && (meter.flags & M::inputClip) != 0);
    outputBar.setClip (fresh && (meter.flags & M::outputClip) != 0);
    inputPeak.setText (fresh ? levelText (meter.input) : "--", juce::dontSendNotification);
    outputPeak.setText (fresh ? levelText (meter.output) : "--", juce::dontSendNotification);
    const auto peakColour = fresh ? palette.accent : palette.accent.withMultipliedBrightness (0.5f);
    inputPeak.setEmission (peakColour, fresh ? 1.0f : 0.0f);
    outputPeak.setEmission (audition ? palette.attention : peakColour, fresh ? 1.0f : 0.0f);

    if (audition != auditionShown)
    {
        auditionShown = audition;
        repaint (outputBarArea.withY (outputBarArea.getY() - 22).withHeight (20).withWidth (200));
    }
}

void SuppressorEditor::print (juce::Graphics& g)
{
    using namespace affine::silkscreen;
    const auto& palette = theme.palette;
    const auto w = static_cast<float> (width), h = static_cast<float> (height);

    affine::render::endCap (g, { 0.0f, 0.0f, capWidth, h }, true);
    affine::render::endCap (g, { w - capWidth, 0.0f, capWidth, h }, false);

    litLegend (g, "SUPPRESSOR", { 80.0f, 22.0f, 360.0f, 34.0f }, brandGreen, affine::fonts::wordmark (24.0f, 0.24f),
               juce::Justification::centredLeft, 0.95f);
    litLegend (g, "GUITAR DI NOISE SUPPRESSOR", { 82.0f, 58.0f, 320.0f, 14.0f }, palette.silkscreenDim,
               affine::fonts::label (12.0f, 0.24f), juce::Justification::centredLeft, 0.25f);

    // Printed dBFS scales under both bar meters.
    for (const auto* bar : { &inputBar, &outputBar })
    {
        const auto span = bar->getScaleSpan();
        g.setFont (affine::fonts::label (10.0f, 0.02f));
        for (int value : { -60, -48, -36, -24, -12, -6, 0 })
        {
            const auto x = span.getX() + span.getWidth() * static_cast<float> (value + 60) / 60.0f;
            g.setColour (palette.silkscreenDim.withAlpha (0.8f));
            g.fillRect (x - 0.5f, span.getBottom() + 5.0f, 1.0f, 4.0f);
            g.drawText (juce::String (value), juce::Rectangle<float> (26.0f, 12.0f).withCentre ({ x, span.getBottom() + 16.0f }),
                        juce::Justification::centred, false);
        }
        g.setColour (palette.danger.withAlpha (0.9f));
        g.drawText ("CLIP", juce::Rectangle<float> (30.0f, 12.0f).withCentre ({ bar->getClipCentre().x, static_cast<float> (bar->getY()) - 9.0f }),
                    juce::Justification::centred, false);
    }
    litLegend (g, "INPUT", { 500.0f, 106.0f, 200.0f, 16.0f }, legendWhite, affine::fonts::label (12.5f, 0.24f),
               juce::Justification::centredLeft, 0.3f);

    // A faint lit rule separates the instruments from the controls.
    g.setColour (legendWhite.withAlpha (0.12f));
    g.fillRect (80.0f, 346.0f, w - 160.0f, 1.0f);
    makersMark (g, { 80.0f, h - 14.0f }, palette);
}

void SuppressorEditor::paint (juce::Graphics& g)
{
    faceplate.paint (g, getLocalBounds(), theme, [this] (juce::Graphics& plate) { print (plate); });
    affine::silkscreen::litLegend (g, auditionShown ? "REMOVED" : "OUTPUT",
                                   { 500.0f, static_cast<float> (outputBarArea.getY()) - 22.0f, 200.0f, 16.0f },
                                   auditionShown ? theme.palette.attention : legendWhite,
                                   affine::fonts::label (12.5f, 0.24f), juce::Justification::centredLeft, 0.3f);
}

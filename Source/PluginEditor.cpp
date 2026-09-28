#include "PluginEditor.h"

namespace
{
float db (float peak) { return juce::Decibels::gainToDecibels (peak, -100.0f); }
juce::String levelText (float peak) { return peak < 0.00001f ? "-inf" : juce::String (db (peak), 1); }
juce::String whole (double v) { return juce::String (juce::roundToInt (v)); }

// Layout, in logical pixels. The rack ears frame an 800 px panel.
constexpr float earWidth = 40.0f;
const juce::Rectangle<float> nameplateArea { 64.0f, 22.0f, 250.0f, 42.0f };
const juce::Rectangle<int> statusArea { 600, 22, 216, 36 };
const juce::Rectangle<int> hostSettingsArea { 600, 62, 216, 16 };
const juce::Rectangle<int> inputArea { 60, 122, 198, 164 }, reductionArea { 272, 100, 336, 216 },
                           outputArea { 622, 122, 198, 164 };
const juce::Point<float> inputClip { 159.0f, 304.0f }, outputClip { 721.0f, 304.0f };
const juce::Rectangle<int> summaryArea { 290, 322, 300, 14 };
const juce::Rectangle<float> suppressionFrame { 64.0f, 356.0f, 548.0f, 176.0f };
const juce::Rectangle<float> monitorFrame { 626.0f, 356.0f, 190.0f, 176.0f };
constexpr int knobAxis = 440;
constexpr int knobCentres[] { 158, 338, 518 };
const juce::Rectangle<int> listenArea { 671, 380, 100, 136 };

const juce::Colour passLamp { 0xfff4eedc }, suppressLamp { 0xff8ae39a }, bypassLamp { 0xffff7b6b };

// Peak meters: dBFS with the top of the range opened up, where levels are set.
float peakPosition (float decibels)
{
    return std::pow (juce::jlimit (0.0f, 1.0f, (decibels + 60.0f) / 60.0f), 1.6f);
}
} // namespace

affine::Theme SuppressorTheme::theme()
{
    affine::Theme t;
    t.panel.texture = affine::PanelFinish::Texture::hammertone;
    t.panel.base = juce::Colour (0xffb3b0a6);
    t.panel.grain = 0.035f;
    t.panel.mottle = 0.12f;
    t.panel.sheen = 0.8f;
    t.panel.dimple = 11.0f;

    using Material = affine::KnobFinish::Material;
    t.knob.cap = juce::Colour (0xff141414);
    t.knob.body = juce::Colour (0xff121212);
    t.knob.capMaterial = Material::glossPlastic;
    t.knob.bodyMaterial = Material::glossPlastic;
    t.knob.pointer = juce::Colour (0xfff2efe6);
    t.knob.index = juce::Colour (0xfff2efe6);
    t.knob.capRatio = 0.66f;
    t.knob.gripRatio = 0.86f;
    t.knob.capDome = 0.28f;
    t.knob.ridges = 20;
    t.knob.ridgeDepth = 0.9f;

    auto& p = t.palette;
    p.silkscreen = juce::Colour (0xff1b1a17);
    p.silkscreenDim = juce::Colour (0xff25231f);
    p.accent = juce::Colour (0xff2f9e4f);
    p.attention = juce::Colour (0xffffb238);
    p.danger = juce::Colour (0xffe5483b);
    p.lamp = affine::Palette::Lamp::jewel;
    p.readout = affine::Palette::Readout::backlit;
    p.readoutBacklight = juce::Colour (0xfff1d58c);
    p.readoutInk = juce::Colour (0xff1f1a12);
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
    listen.setStyle (affine::KeyButton::Style::toggle);
    listen.setLegend ("Removed", "Processed");
    addAndMakeVisible (listen);

    affine::NeedleMeter::Face vu;
    vu.backlight = juce::Colour (0xfff2d48a);
    vu.ink = juce::Colour (0xff20190f);
    vu.zone = juce::Colour (0xffc8321f);
    vu.mirror = false;
    vu.bezel = affine::NeedleMeter::Face::Bezel::chrome;

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
    reduction.setFace (vu);
    addAndMakeVisible (reduction);

    affine::NeedleMeter::Scale peak;
    peak.toPosition = peakPosition;
    peak.majors = { -40, -20, -10, -5, 0 };
    peak.minors = { -30, -15, -8, -6, -4, -3, -2, -1 };
    peak.format = [] (float v) { return whole (v); };
    peak.unit = "dBFS";
    auto peakFace = vu;
    peakFace.zoneFrom = peakPosition (-6.0f);
    for (auto* m : { &inputMeter, &outputMeter })
    {
        m->setTheme (theme);
        m->setScale (peak);
        m->setFace (peakFace);
        m->setBallistics (6.0f, 0.82f);
        addAndMakeVisible (*m);
    }
    inputMeter.setCaption ("Input");
    outputMeter.setCaption ("Output");

    status.setName ("Processing status");
    status.setTheme (theme);
    status.setBacklit (true);
    status.setJustificationType (juce::Justification::centred);
    status.setDisplayFont (affine::fonts::label (15.0f, 0.18f));
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
                                static_cast<juce::Component*> (&hostSettings) })
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
    inputMeter.setBounds (inputArea);
    reduction.setBounds (reductionArea);
    outputMeter.setBounds (outputArea);
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
    if (! fresh)
        status.setEmission (passLamp, 0.0f);
    else if (bypass)
        status.setEmission (bypassLamp, 1.0f);
    else if (audition || learning)
        status.setEmission (palette.attention, 1.0f);
    else if (meter.input < 0.00001f)
        status.setEmission (passLamp, 0.45f);
    else
        status.setEmission (meter.reduction > 0.5f ? suppressLamp : passLamp, 1.0f);

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
    inputMeter.setReading (db (meter.input), fresh);
    outputMeter.setReading (db (meter.output), fresh);
    outputMeter.setCaption (audition ? "Removed" : "Output");

    const auto inClip = fresh && (meter.flags & M::inputClip) != 0;
    const auto outClip = fresh && (meter.flags & M::outputClip) != 0;
    if (inClip != inputClipped || outClip != outputClipped)
    {
        inputClipped = inClip;
        outputClipped = outClip;
        for (auto centre : { inputClip, outputClip })
            repaint (juce::Rectangle<int> (40, 40).withCentre (centre.toInt()));
    }
}

void SuppressorEditor::print (juce::Graphics& g)
{
    using namespace affine::silkscreen;
    const auto& palette = theme.palette;
    const auto w = static_cast<float> (width), h = static_cast<float> (height);

    affine::render::rackEar (g, { 0.0f, 0.0f, earWidth, h }, true);
    affine::render::rackEar (g, { w - earWidth, 0.0f, earWidth, h }, false);

    affine::render::nameplate (g, nameplateArea, "SUPPRESSOR", affine::fonts::label (25.0f, 0.42f));
    g.setColour (palette.silkscreenDim);
    g.setFont (affine::fonts::label (12.0f, 0.24f));
    g.drawText ("GUITAR DI NOISE SUPPRESSOR", juce::Rectangle<float> (nameplateArea.getX() + 1.0f, nameplateArea.getBottom() + 6.0f, 300.0f, 14.0f),
                juce::Justification::centredLeft, false);
    groove (g, earWidth + 12.0f, w - earWidth - 12.0f, 92.0f);

    g.setFont (affine::fonts::label (10.5f, 0.22f));
    g.setColour (palette.silkscreen);
    for (auto centre : { inputClip, outputClip })
        g.drawText ("CLIP", juce::Rectangle<float> (40.0f, 12.0f).withCentre (centre.translated (32.0f, 0.0f)),
                    juce::Justification::centredLeft, false);

    frame (g, suppressionFrame, "Suppression", palette);
    frame (g, monitorFrame, "Monitor", palette);
    makersMark (g, { earWidth + 24.0f, h - 12.0f }, palette);
}

void SuppressorEditor::paint (juce::Graphics& g)
{
    faceplate.paint (g, getLocalBounds(), theme, [this] (juce::Graphics& plate) { print (plate); });
    affine::render::jewel (g, inputClip, 11.0f, theme.palette.danger, inputClipped ? 1.0f : 0.0f);
    affine::render::jewel (g, outputClip, 11.0f, theme.palette.danger, outputClipped ? 1.0f : 0.0f);
}

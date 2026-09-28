#include "PluginEditor.h"

namespace
{
float db (float peak) { return juce::Decibels::gainToDecibels (peak, -100.0f); }
juce::String levelText (float peak) { return peak < 0.00001f ? "-inf" : juce::String (db (peak), 1); }
juce::String whole (double v) { return juce::String (juce::roundToInt (v)); }

// Layout, in logical pixels.
const juce::Rectangle<int> statusArea { 470, 28, 310, 20 };
const juce::Rectangle<int> hostSettingsArea { 470, 54, 310, 16 };
const juce::Rectangle<int> screenArea { 36, 86, 748, 238 };
const juce::Rectangle<int> summaryArea { 480, 330, 300, 14 };
constexpr int knobAxis = 434;
constexpr int knobCentres[] { 130, 296, 462 };
const juce::Rectangle<int> listenArea { 600, 368, 170, 130 };

// Reduction reads on a square-root law, as on the family's reduction meters.
float depthFor (float decibels) { return std::sqrt (juce::jlimit (0.0f, 1.0f, decibels / 60.0f)); }
} // namespace

affine::Theme SuppressorTheme::theme()
{
    affine::Theme t;
    t.palette.screen = juce::Colour (0xff35e0c8);
    return t;
}

ReductionScreen::ReductionScreen()
{
    history.fill (std::numeric_limits<float>::quiet_NaN());
    setInterceptsMouseClicks (false, false);
    setAccessible (false);
}

void ReductionScreen::setTheme (const affine::Theme& newTheme)
{
    theme = newTheme;
    repaint();
}

void ReductionScreen::push (float reductionDb, bool hasReduction, float inputDb, float outputDb, bool isLive,
                            bool inputClip, bool outputClip, bool removed, bool multiband)
{
    const auto wasIdle = valid == 0 && ! live;
    auto& slot = history[static_cast<size_t> (head)];
    if (! std::isnan (slot))
        --valid;
    slot = hasReduction ? juce::jmax (0.0f, reductionDb) : std::numeric_limits<float>::quiet_NaN();
    if (hasReduction)
        ++valid;
    head = (head + 1) % frames;

    const auto captionsChanged = removed != removedSignal || multiband != multi;
    latest = reductionDb;
    current = hasReduction;
    live = isLive;
    input = inputDb;
    output = outputDb;
    inputClipped = inputClip;
    outputClipped = outputClip;
    removedSignal = removed;
    multi = multiband;
    if (wasIdle && valid == 0 && ! live && ! captionsChanged)
        return;
    repaint();
}

void ReductionScreen::paint (juce::Graphics& g)
{
    const auto glass = getLocalBounds().toFloat().reduced (5.0f);
    affine::render::screenGlass (g, glass);

    auto inner = glass.reduced (18.0f, 14.0f);
    auto levels = inner.removeFromRight (184.0f);
    inner.removeFromRight (24.0f);
    g.setColour (theme.palette.screen.withAlpha (0.10f));
    g.fillRect (levels.getX() - 12.5f, inner.getY() + 4.0f, 1.0f, inner.getHeight() - 8.0f);
    paintHistory (g, inner);

    levels = levels.withSizeKeepingCentre (levels.getWidth(), 160.0f);
    paintLevel (g, levels.removeFromTop (62.0f), "INPUT", input, inputClipped, theme.palette.screen);
    paintLevel (g, levels.removeFromBottom (62.0f), removedSignal ? "REMOVED" : "OUTPUT", output, outputClipped,
                removedSignal ? theme.palette.attention : theme.palette.screen);
}

void ReductionScreen::paintHistory (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto screenColour = theme.palette.screen;
    const auto dim = screenColour.withAlpha (0.45f);
    auto header = area.removeFromTop (36.0f);
    affine::screen::caption (g, multi ? "MAX BAND REDUCTION" : "HIGH-BAND REDUCTION", header.withHeight (15.0f), screenColour);
    g.setColour (dim);
    g.setFont (affine::fonts::label (10.0f, 0.2f));
    g.drawText ("dB, LAST SIX SECONDS", header.withTrimmedTop (18.0f).withHeight (12.0f), juce::Justification::centredLeft, false);

    auto unit = header.removeFromRight (24.0f);
    g.setColour (current ? screenColour : dim);
    g.setFont (affine::fonts::label (12.0f, 0.1f));
    g.drawText ("dB", unit.withTrimmedBottom (3.0f), juce::Justification::bottomRight, false);
    const auto reading = ! current ? juce::String ("--") : latest >= 60.0f ? juce::String ("60+") : juce::String (latest, 1);
    readoutGlow.draw (g, reading, affine::fonts::readout (32.0f), header.withTrimmedRight (4.0f), juce::Justification::centredRight,
                      current ? affine::screen::lit (screenColour) : screenColour.withAlpha (0.35f), 4.0f,
                      current ? 0.9f : 0.3f);

    area.removeFromTop (10.0f);
    const auto timeAxis = area.removeFromBottom (16.0f);
    const auto gutter = area.removeFromLeft (26.0f);
    const auto plot = area;
    const auto yFor = [&] (float decibels) { return plot.getY() + plot.getHeight() * depthFor (decibels); };
    const auto xFor = [&] (int frame) { return plot.getX() + plot.getWidth() * static_cast<float> (frame) / static_cast<float> (frames - 1); };

    g.setFont (affine::fonts::label (10.0f, 0.02f));
    for (float mark : { 0.0f, 5.0f, 10.0f, 20.0f, 40.0f, 60.0f })
    {
        const auto y = yFor (mark);
        g.setColour (screenColour.withAlpha (mark == 0.0f ? 0.22f : 0.08f));
        g.fillRect (plot.getX(), y - 0.5f, plot.getWidth(), 1.0f);
        g.setColour (dim);
        g.drawText (whole (mark), juce::Rectangle<float> (gutter.getWidth() - 7.0f, 12.0f).withPosition (gutter.getX(), y - 6.0f),
                    juce::Justification::centredRight, false);
    }
    g.setColour (screenColour.withAlpha (0.07f));
    for (int second = 1; second < 6; ++second)
    {
        const auto x = plot.getX() + plot.getWidth() * static_cast<float> (second) / 6.0f;
        for (auto y = plot.getY() + 2.0f; y < plot.getBottom(); y += 4.0f)
            g.fillRect (x - 0.5f, y, 1.0f, 1.5f);
    }
    g.setColour (dim);
    g.drawText ("-6 S", timeAxis.withWidth (40.0f).withX (plot.getX()), juce::Justification::centredLeft, false);
    g.drawText ("-3 S", juce::Rectangle<float> (40.0f, timeAxis.getHeight()).withCentre ({ plot.getCentreX(), timeAxis.getCentreY() }),
                juce::Justification::centred, false);
    g.drawText ("NOW", timeAxis.withLeft (plot.getRight() - 40.0f).withRight (plot.getRight()), juce::Justification::centredRight, false);

    // One filled shape per unbroken run of readings; gaps are frames without a reading.
    juce::Path fill, edge;
    bool inRun = false;
    juce::Point<float> last;
    for (int i = 0; i <= frames; ++i)
    {
        const auto value = i < frames ? history[static_cast<size_t> ((head + i) % frames)] : std::numeric_limits<float>::quiet_NaN();
        if (! std::isnan (value))
        {
            const juce::Point<float> at { xFor (i), yFor (value) };
            if (! inRun)
            {
                fill.startNewSubPath (at.x, plot.getY());
                edge.startNewSubPath (at);
                inRun = true;
            }
            else
            {
                edge.lineTo (at);
            }
            fill.lineTo (at);
            last = at;
        }
        else if (inRun)
        {
            fill.lineTo (last.x, plot.getY());
            fill.closeSubPath();
            inRun = false;
        }
    }
    g.setGradientFill (juce::ColourGradient (screenColour.withAlpha (0.05f), 0.0f, plot.getY(),
                                             screenColour.withAlpha (0.42f), 0.0f, plot.getBottom(), false));
    g.fillPath (fill);
    const juce::PathStrokeType::JointStyle joint = juce::PathStrokeType::curved;
    g.setColour (screenColour.withAlpha (0.16f));
    g.strokePath (edge, juce::PathStrokeType (5.0f, joint, juce::PathStrokeType::rounded));
    g.setColour (screenColour);
    g.strokePath (edge, juce::PathStrokeType (1.6f, joint, juce::PathStrokeType::rounded));

    if (current)
    {
        const juce::Point<float> now { xFor (frames - 1), yFor (latest) };
        affine::render::halo (g, now, 10.0f, screenColour, 0.7f);
        g.setColour (screenColour.interpolatedWith (juce::Colours::white, 0.5f));
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (now));
    }
    else if (valid == 0)
    {
        g.setColour (screenColour.withAlpha (0.4f));
        g.setFont (affine::fonts::label (13.0f, 0.3f));
        g.drawText (live ? "NO READING" : "NO AUDIO", plot, juce::Justification::centred, false);
    }
}

void ReductionScreen::paintLevel (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& name, float level,
                                  bool clipped, juce::Colour colour)
{
    const auto& palette = theme.palette;
    auto top = area.removeFromTop (22.0f);
    affine::screen::caption (g, name, top, colour);
    g.setColour (live ? affine::screen::lit (colour) : colour.withAlpha (0.35f));
    g.setFont (affine::fonts::readout (19.0f));
    g.drawText (! live ? juce::String ("--") : level <= -99.0f ? juce::String ("-inf") : juce::String (level, 1), top,
                juce::Justification::centredRight, false);

    area.removeFromTop (8.0f);
    auto bar = area.removeFromTop (10.0f);
    const auto clip = bar.removeFromRight (16.0f);
    bar.removeFromRight (6.0f);
    affine::screen::segments (g, bar, 40, live ? (level + 60.0f) / 60.0f : 0.0f, [&] (float position, bool lit)
    {
        const auto decibels = -60.0f + 60.0f * position;
        const auto zone = decibels > -3.0f ? palette.attention
                        : decibels > -12.0f ? colour.interpolatedWith (juce::Colours::white, 0.45f) : colour;
        return lit ? zone : zone.withAlpha (0.1f);
    });
    if (clipped)
        affine::render::halo (g, clip.getCentre(), 14.0f, palette.danger, 0.6f);
    g.setColour (clipped ? palette.danger : palette.danger.withAlpha (0.16f));
    g.fillRect (clip);

    area.removeFromTop (4.0f);
    g.setFont (affine::fonts::label (9.5f, 0.02f));
    for (int value : { -48, -36, -24, -12, -6, 0 })
    {
        const auto x = bar.getX() + bar.getWidth() * static_cast<float> (value + 60) / 60.0f;
        g.setColour (colour.withAlpha (0.3f));
        g.fillRect (x - 0.5f, area.getY(), 1.0f, 3.0f);
        g.setColour (colour.withAlpha (0.5f));
        g.drawText (juce::String (value), juce::Rectangle<float> (24.0f, 11.0f).withCentre ({ x, area.getY() + 10.0f }),
                    juce::Justification::centred, false);
    }
    g.setColour (palette.danger.withAlpha (clipped ? 1.0f : 0.6f));
    g.drawText ("CLIP", juce::Rectangle<float> (30.0f, 11.0f).withCentre ({ clip.getCentreX(), area.getY() + 10.0f }),
                juce::Justification::centred, false);
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
    listen.setKeySize (76.0f, 56.0f);
    listen.setLampColour (theme.palette.attention);
    listen.setLegend ("Listen", "Removed");
    juce::Path headphones;
    headphones.addCentredArc (0.5f, 0.62f, 0.4f, 0.52f, 0.0f, -juce::MathConstants<float>::halfPi,
                              juce::MathConstants<float>::halfPi, true);
    headphones.addRoundedRectangle (0.02f, 0.55f, 0.22f, 0.43f, 0.06f);
    headphones.addRoundedRectangle (0.76f, 0.55f, 0.22f, 0.43f, 0.06f);
    listen.setGlyph (headphones);
    addAndMakeVisible (listen);

    screen.setTheme (theme);
    addAndMakeVisible (screen);

    status.setName ("Processing status");
    status.setTheme (theme);
    status.setGlassVisible (false);
    status.setJustificationType (juce::Justification::centredRight);
    status.setDisplayFont (affine::fonts::label (14.0f, 0.24f));
    hostSettings.setName ("Host settings");
    hostSettings.setTheme (theme);
    hostSettings.setGlassVisible (false);
    hostSettings.setJustificationType (juce::Justification::centredRight);
    hostSettings.setDisplayFont (affine::fonts::label (11.5f, 0.2f));
    hostSettings.setEmission (theme.palette.silkscreenDim, 1.0f);
    hostSettings.setLampColour (theme.palette.attention);
    meterSummary.setName ("Signal meters");
    meterSummary.setTooltip ("Input and output: peak across channels, dBFS. Reduction: deepest gate attenuation, not overall loudness loss. 300 ms peak decay; clips held for one second.");
    meterSummary.setFont (affine::fonts::label (11.0f, 0.22f));
    meterSummary.setColour (juce::Label::textColourId, theme.palette.silkscreenDim);
    meterSummary.setJustificationType (juce::Justification::centredRight);
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
    screen.setBounds (screenArea);
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
    status.setEmission (fresh ? palette.silkscreen : palette.silkscreenDim, fresh ? 1.0f : 0.0f);
    status.setLampColour (bypass ? palette.danger : (audition || learning) ? palette.attention
                          : state == "Suppressing" ? palette.screen : palette.accent);

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
    screen.push (meter.reduction, hasReduction, db (meter.input), db (meter.output), fresh,
                 fresh && (meter.flags & M::inputClip) != 0, fresh && (meter.flags & M::outputClip) != 0, audition, multi);
}

void SuppressorEditor::print (juce::Graphics& g)
{
    const auto& palette = theme.palette;
    affine::silkscreen::wordmark (g, "Suppressor", "Guitar DI noise suppressor", { 38.0f, 20.0f }, palette);

    // A hairline separates the Listen key from the three dials.
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.fillRect (570.0f, 380.0f, 1.0f, 112.0f);
    affine::silkscreen::makersMark (g, { 40.0f, static_cast<float> (height) - 16.0f }, palette);
}

void SuppressorEditor::paint (juce::Graphics& g)
{
    faceplate.paint (g, getLocalBounds(), theme, [this] (juce::Graphics& plate) { print (plate); });
}

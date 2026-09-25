#include <doctest/doctest.h>
#include "PluginEditor.h"
#include <array>
#include <set>

namespace
{
void pump (int ms = 50)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
    juce::Timer::callPendingTimersSynchronously();
}
void setParameter (SuppressorProcessor& p, const char* id, float plain)
{
    auto* parameter = p.apvts.getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
}
void prepare (juce::AudioProcessor& p)
{
    p.setRateAndBufferSizeDetails (48000, 256);
    p.prepareToPlay (48000, 256);
}
void feed (juce::AudioProcessor& p, int blocks = 120, bool silence = false, bool bypass = false)
{
    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        for (int n = 0; n < 256; ++n)
        {
            const auto phase = juce::MathConstants<double>::twoPi * (b * 256 + n) / 48000.0;
            const auto sample = silence ? 0.0f : static_cast<float> (0.08 * std::sin (220.0 * phase)
                                                                      + 0.003 * std::sin (11000.0 * phase));
            buffer.setSample (0, n, sample);
            buffer.setSample (1, n, sample * 0.7f);
        }
        if (bypass) p.processBlockBypassed (buffer, midi); else p.processBlock (buffer, midi);
    }
}
juce::MouseEvent event (juce::Component& c, juce::Point<float> pos = { 60, 60 }, int modifiers = juce::ModifierKeys::leftButtonModifier)
{
    const auto now = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(), pos, juce::ModifierKeys (modifiers),
             1, 0, 0, 0, 0, &c, &c, now, { 60, 60 }, now, 1, true };
}
struct Gestures final : juce::AudioProcessorParameter::Listener
{
    void parameterValueChanged (int, float) override { ++changes; }
    void parameterGestureChanged (int, bool start) override { start ? ++starts : ++ends; }
    int starts = 0, ends = 0, changes = 0;
};
juce::Label* namedLabel (juce::Component& editor, const juce::String& name)
{
    for (auto* c : editor.getChildren())
        if (c->getName() == name) return dynamic_cast<juce::Label*> (c);
    return nullptr;
}
void capture (juce::Component& editor, const juce::String& name, float scale = 1.0f)
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("SUPPRESSOR_UI_CAPTURE_DIR", {});
    if (path.isEmpty()) return;
    auto dir = juce::File (path);
    REQUIRE (dir.createDirectory().wasOk());
    const auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true, scale);
    auto file = dir.getChildFile (name + ".png");
    file.deleteFile();
    auto stream = file.createOutputStream();
    REQUIRE (stream != nullptr);
    REQUIRE (juce::PNGImageFormat().writeImageToStream (image, *stream));
    stream->flush();
}
float contrast (juce::Colour a, juce::Colour b)
{
    auto lum = [] (juce::Colour c)
    {
        auto linear = [] (float v) { return v <= 0.04045f ? v / 12.92f : std::pow ((v + 0.055f) / 1.055f, 2.4f); };
        return .2126f * linear (c.getFloatRed()) + .7152f * linear (c.getFloatGreen()) + .0722f * linear (c.getFloatBlue());
    };
    return (std::max (lum (a), lum (b)) + .05f) / (std::min (lum (a), lum (b)) + .05f);
}
} // namespace

TEST_CASE ("theme retains bundled fonts and accessible contrast")
{
    using namespace SuppressorTheme;
    CHECK (makeFont (18).getTypefaceName() == "Barlow Condensed");
    CHECK (makeFont (18).getTypefaceStyle() != makeFont (18, true).getTypefaceStyle());
    for (auto colour : { primaryText, secondaryText, accent, warning, error })
        CHECK (contrast (colour, surface) >= 4.5f);
    CHECK (contrast (controlOutline, surfaceRaised) >= 3.0f);
}

TEST_CASE ("four controls preserve the complete host parameter and saved-state contract")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SuppressorProcessor p;
    SuppressorEditor editor (p);
    editor.addToDesktop (0);
    editor.setVisible (true);
    const juce::StringArray ids { "strength", "threshold", "release", "gateMode", "depth", "hysteresis", "hold",
        "adaptiveRelease", "cue", "sidechain", "lookahead", "humEnable", "humBase", "humHarmonics", "humStrength",
        "humLearn", "bandMode", "bandsLearn", "deltaAudition", "outputGain" };
    const std::array<float, 20> defaults { .9f, -40, 6, 0, 40, 6, 2, 0, 0, 0, 0, 0, 0, 8, 1, 0, 0, 0, 0, 0 };
    REQUIRE (p.getParameters().size() == ids.size());
    CHECK (editor.getWidth() == 640);
    CHECK (editor.getHeight() == 460);
    CHECK_FALSE (editor.isResizable());
    std::set<int> focus;
    int controls = 0;
    for (int i = 0; i < ids.size(); ++i)
    {
        auto* param = p.apvts.getParameter (ids[i]);
        REQUIRE (param != nullptr);
        CHECK (p.getParameters()[i] == param);
        CHECK (param->getVersionHint() == 1);
        CHECK (param->convertFrom0to1 (param->getDefaultValue()) == doctest::Approx (defaults[static_cast<size_t> (i)]));
        auto* component = editor.findChildWithID (ids[i]);
        const bool visible = ids[i] == "strength" || ids[i] == "threshold" || ids[i] == "release" || ids[i] == "deltaAudition";
        CHECK ((component != nullptr) == visible);
        if (component != nullptr)
        {
            ++controls;
            CHECK (editor.getControlParameterIndex (*component) == i);
            CHECK (component->getWidth() >= 32);
            CHECK (component->getHeight() >= 32);
            CHECK (component->getWantsKeyboardFocus());
            CHECK (component->getName().isNotEmpty());
            CHECK (component->getAccessibilityHandler() != nullptr);
            CHECK (focus.insert (component->getExplicitFocusOrder()).second);
        }
        param->setValueNotifyingHost (param->convertTo0to1 (param->convertFrom0to1 (0.27f)));
    }
    CHECK (controls == 4);
    juce::MemoryBlock saved;
    p.getStateInformation (saved);
    SuppressorProcessor recalled;
    recalled.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    for (const auto& id : ids)
        CHECK (recalled.apvts.getParameter (id)->getValue() == doctest::Approx (p.apvts.getParameter (id)->getValue()));
}

TEST_CASE ("dials support balanced drag, fine, exact, cancel, invalid, reset and close gestures")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SuppressorProcessor p;
    auto editor = std::make_unique<SuppressorEditor> (p);
    editor->addToDesktop (0);
    editor->setVisible (true);
    auto* dial = dynamic_cast<SuppressorDial*> (editor->findChildWithID ("strength"));
    REQUIRE (dial != nullptr);
    auto& param = dial->parameter;
    Gestures gestures;
    param.addListener (&gestures);
    CHECK (dial->getComponentAt (dial->getLocalBounds().getCentre()) == dial);
    setParameter (p, "strength", .5f);
    dial->mouseDown (event (*dial));
    dial->mouseDrag (event (*dial, { 60, 36 }));
    dial->mouseUp (event (*dial));
    CHECK (dial->getValue() == doctest::Approx (.6));
    CHECK (gestures.starts == 1);
    CHECK (gestures.ends == 1);
    dial->mouseDown (event (*dial));
    dial->mouseDrag (event (*dial, { 60, 36 }, juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier));
    dial->mouseUp (event (*dial));
    CHECK (dial->getValue() == doctest::Approx (.61));
    CHECK (dial->keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    CHECK (dial->getValue() == doctest::Approx (.62));
    auto* threshold = dynamic_cast<SuppressorDial*> (editor->findChildWithID ("threshold"));
    REQUIRE (threshold != nullptr);
    threshold->mouseDown (event (*threshold));
    for (int pixel = 1; pixel <= 6; ++pixel)
        threshold->mouseDrag (event (*threshold, { 60, 60.0f - static_cast<float> (pixel) },
            juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier));
    threshold->mouseUp (event (*threshold));
    CHECK (threshold->getValue() == doctest::Approx (-39.8));

    auto enter = [&] (const juce::String& value, bool commit)
    {
        dial->keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
        auto* entry = dynamic_cast<juce::TextEditor*> (dial->getChildComponent (0));
        REQUIRE (entry != nullptr);
        REQUIRE (entry->isVisible());
        CHECK (editor->getControlParameterIndex (*entry) == param.getParameterIndex());
        entry->setText (value);
        if (commit) entry->onReturnKey(); else entry->onEscapeKey();
        CHECK_FALSE (entry->isVisible());
    };
    enter ("35 %", true);
    CHECK (param.convertFrom0to1 (param.getValue()) == doctest::Approx (.35));
    enter ("80 %", false);
    CHECK (dial->getValue() == doctest::Approx (.35));
    for (const auto& invalid : { "junk", "nan", "inf", "--12", "12 garbage", "0x10", "1e2", "" })
    {
        enter (invalid, true);
        CHECK (dial->getValue() == doctest::Approx (.35));
    }
    enter ("999 %", true);
    CHECK (dial->getValue() == 1.0);
    dial->mouseDown (event (*dial, { 60, 60 }, juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier));
    CHECK (dial->getValue() == doctest::Approx (.9));
    setParameter (p, "strength", .42f);
    CHECK (dial->getValue() == doctest::Approx (.42));
    capture (*editor, "suppressor-exact-entry-before");
    dial->beginEntry();
    capture (*editor, "suppressor-exact-entry");
    CHECK (gestures.starts == gestures.ends);
    auto* listen = dynamic_cast<ListenButton*> (editor->findChildWithID ("deltaAudition"));
    REQUIRE (listen != nullptr);
    Gestures buttonGestures;
    auto* listenParam = p.apvts.getParameter ("deltaAudition");
    listenParam->addListener (&buttonGestures);
    CHECK (listen->keyPressed (juce::KeyPress (' ')));
    CHECK (listenParam->getValue() == doctest::Approx (1.0));
    CHECK (buttonGestures.starts == 1);
    CHECK (buttonGestures.ends == 1);
    listenParam->removeListener (&buttonGestures);
    dial->mouseDown (event (*dial));
    editor->setVisible (false);
    CHECK (gestures.starts == gestures.ends);
    editor->setVisible (true);
    dial->mouseDown (event (*dial));
    CHECK (gestures.starts == gestures.ends + 1);
    editor.reset();
    CHECK (gestures.starts == gestures.ends);
    param.removeListener (&gestures);
}

TEST_CASE ("processor metering follows actual output without altering DSP")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SuppressorProcessor p;
    prepare (p);
    suppressor::DenoiserEngine reference;
    reference.prepare (48000, 2, 120);
    suppressor::EngineParams targets;
    targets.strength01 = p.apvts.getRawParameterValue ("strength")->load();
    reference.setTargets (targets);
    reference.reset();
    juce::AudioBuffer<float> audio (2, 256), expected (2, 256);
    juce::MidiBuffer midi;
    float* io[] { expected.getWritePointer (0), expected.getWritePointer (1) };
    for (int block = 0; block < 50; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < 256; ++n)
                audio.setSample (ch, n, static_cast<float> (.03 * std::sin ((block * 256 + n) * .12)));
        expected.makeCopyOf (audio);
        reference.setTargets (targets);
        reference.processBlock (io, nullptr, 0, 256, 2);
        p.processBlock (audio, midi);
        for (int ch = 0; ch < 2; ++ch)
            CHECK (std::equal (audio.getReadPointer (ch), audio.getReadPointer (ch) + 256, expected.getReadPointer (ch)));
    }
    for (bool delta : { false, true })
    {
        p.releaseResources();
        prepare (p);
        setParameter (p, "deltaAudition", delta ? 1.0f : 0.0f);
        setParameter (p, "outputGain", 6);
        audio.clear();
        audio.setSample (1, 0, .8f);
        p.processBlock (audio, midi);
        suppressor::MeterSnapshot s;
        REQUIRE (p.readMeters (s));
        CHECK (s.input == doctest::Approx (.8f));
        CHECK (s.output == doctest::Approx (audio.getMagnitude (0, 256)));
        CHECK (((s.flags & suppressor::MeterSnapshot::removed) != 0) == delta);
    }
    p.releaseResources();
    suppressor::MeterSnapshot s;
    REQUIRE (p.readMeters (s));
    CHECK (s.flags == 0);
}

TEST_CASE ("live meter states, stale data, legacy modes and monitor labels are explicit")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SuppressorProcessor p;
    prepare (p);
    auto editor = std::make_unique<SuppressorEditor> (p);
    editor->addToDesktop (0);
    editor->setVisible (true);
    pump();
    juce::Component::unfocusAllComponents();
    auto* status = namedLabel (*editor, "Processing status");
    auto* summary = namedLabel (*editor, "Signal meters");
    REQUIRE (status != nullptr);
    REQUIRE (summary != nullptr);
    CHECK (status->getText() == "No audio");
    capture (*editor, "suppressor-default");
    feed (p); pump();
    CHECK (status->getText() == "Suppressing");
    CHECK (summary->getDescription().contains ("High-band reduction"));
    capture (*editor, "suppressor-ui");
    for (float scale : { 1.0f, 1.25f, 1.5f, 1.75f, 2.0f })
    {
        editor->setScaleFactor (scale);
        CHECK (editor->getWidth() == 640);
        CHECK (editor->getLocalBounds().toFloat().transformedBy (editor->getTransform()).getWidth()
               == doctest::Approx (640 * scale));
        capture (*editor, "suppressor-scale-" + juce::String (juce::roundToInt (scale * 100)), scale);
    }
    editor->setScaleFactor (1);
    setParameter (p, "deltaAudition", 1); pump();
    CHECK (status->getText() == "No audio"); // old output is not relabelled as removed audio
    feed (p); pump();
    CHECK (status->getText() == "Listening to difference");
    CHECK (summary->getDescription().contains ("removed"));
    capture (*editor, "suppressor-delta-audition");
    feed (p, 120, false, true); pump();
    CHECK (status->getText() == "Bypassed");
    capture (*editor, "suppressor-bypassed");
    setParameter (p, "deltaAudition", 0);
    feed (p, 700, true); pump();
    CHECK (status->getText() == "No input");
    capture (*editor, "suppressor-silence");
    setParameter (p, "bandMode", 1); setParameter (p, "bandsLearn", 1);
    feed (p); pump();
    CHECK (status->getText() == "Learning bands");
    CHECK (summary->getDescription().contains ("unavailable"));
    CHECK_FALSE (editor->findChildWithID ("threshold")->isEnabled());
    CHECK_FALSE (editor->findChildWithID ("strength")->isEnabled());
    CHECK (namedLabel (*editor, "Host settings")->getText() == "Host settings active");
    capture (*editor, "suppressor-learning");
    setParameter (p, "bandsLearn", 0); feed (p); pump();
    CHECK (summary->getDescription().contains ("Maximum band reduction"));
    pump (600);
    CHECK (status->getText() == "No audio");
    CHECK (summary->getDescription() == "No current audio readings");
    editor.reset();
    editor = std::make_unique<SuppressorEditor> (p);
    editor->addToDesktop (0);
    editor->setVisible (true); pump();
    CHECK (namedLabel (*editor, "Processing status")->getText() == "No audio");
}

TEST_CASE ("sidechain levels are excluded and clipping is visible and accessible")
{
    juce::ScopedJuceInitialiser_GUI gui;
    SuppressorProcessor p;
    auto layout = p.getBusesLayout();
    layout.inputBuses.set (1, juce::AudioChannelSet::stereo());
    REQUIRE (p.setBusesLayout (layout));
    prepare (p);
    SuppressorEditor editor (p);
    editor.addToDesktop (0);
    editor.setVisible (true);
    juce::AudioBuffer<float> buffer (4, 256);
    juce::MidiBuffer midi;
    buffer.clear();
    buffer.setSample (0, 0, .25f);
    buffer.setSample (2, 0, 4.0f);
    p.processBlock (buffer, midi);
    suppressor::MeterSnapshot meters;
    REQUIRE (p.readMeters (meters));
    CHECK (meters.input == doctest::Approx (.25f));
    CHECK ((meters.flags & suppressor::MeterSnapshot::inputClip) == 0);
    buffer.clear();
    buffer.setSample (1, 0, 1.25f);
    p.processBlockBypassed (buffer, midi);
    pump();
    auto* summary = namedLabel (editor, "Signal meters");
    REQUIRE (summary != nullptr);
    CHECK (summary->getDescription().contains ("Input clipped"));
    CHECK (summary->getDescription().contains ("Output clipped"));
    capture (editor, "suppressor-clipping");
    for (float value : { 0.0f, 1.0f })
    {
        for (const char* id : { "threshold", "strength", "release" })
            p.apvts.getParameter (id)->setValueNotifyingHost (value);
        pump();
        capture (editor, value > .5f ? "suppressor-maximum" : "suppressor-minimum");
    }
}

TEST_CASE ("VST3 native editor lifecycle and host parameter state recall")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("SUPPRESSOR_VST3_PATH", {});
    if (path.isEmpty()) return;
    juce::ScopedJuceInitialiser_GUI gui;
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> types;
    format.findAllTypesForFile (types, path);
    REQUIRE (types.size() == 1);
    juce::String errorMessage;
    auto p = format.createInstanceFromDescription (*types[0], 48000, 256, errorMessage);
    INFO (errorMessage);
    REQUIRE (p != nullptr);
    prepare (*p);
    for (int reopen = 0; reopen < 3; ++reopen)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (p->createEditorIfNeeded());
        REQUIRE (editor != nullptr);
        editor->addToDesktop (0);
        editor->setVisible (true);
        pump();
        feed (*p); pump();
        CHECK (editor->getWidth() == 640);
        CHECK (editor->getHeight() == 460);
        // This is the host's native embedding view, not the plugin's JUCE
        // component tree. Exercise the real wrapper rather than casting it.
        REQUIRE (p->getParameters().size() == 21); // includes VST3 wrapper bypass
        auto* parameter = p->getParameters()[1];
        CHECK (parameter->getName (64) == "Threshold");
        parameter->setValueNotifyingHost (.6f);
        feed (*p); pump();
        juce::MemoryBlock state;
        p->getStateInformation (state);
        const auto saved = parameter->getValue();
        parameter->setValueNotifyingHost (.8f);
        feed (*p); pump();
        CHECK (parameter->getValue() == doctest::Approx (.8f));
        p->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        feed (*p); pump();
        CHECK (parameter->getValue() == doctest::Approx (saved));
        editor->setVisible (false);
        pump();
        editor->setVisible (true);
        pump();
    }
    p->releaseResources();
}

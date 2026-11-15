#include "PluginEditor.h"
#include "dsp/ExampleHit.h"
#include "ui/ModeColours.h"
#include "ui/RevealPrompt.h"
#include "ui/Tips.h"

namespace
{
using lucky::Knob;
using lucky::Mode;
namespace modes = lucky::modes;
namespace format = lucky::format;

constexpr int topBarHeight = lucky::theme::topBarHeight;
constexpr int brandWidth = 230;
constexpr int menuWidth = 56;
constexpr int stripHeight = 172;
constexpr int tipsHeight = 38;
constexpr float liveShare = 0.6f;
const juce::String showTipsKey { "showTips" };
const juce::String showReductionKey { "showGainReduction" };
const juce::String showLabKey { "showLab" };
const juce::String askBeforeReductionKey { "askBeforeShowingGainReduction" };
/** VCA Auto sits one switch position past 1.2 s on the release dial. */
constexpr float autoReleasePositionMs = 2400.0f;

float logPosition (float from, float to, float value)
{
    return std::log (std::max (value, from * 0.5f) / from) / std::log (to / from);
}

float logValue (float from, float to, float position)
{
    return from * std::pow (to / from, position);
}

/** One log scale shared by every mode, so a mode's reach can be compared with the others on the same dial. */
struct Scale
{
    float from, to;
    juce::String startLabel, endLabel;

    float position (float value) const { return logPosition (from, to, value); }
    float value (float position) const { return logValue (from, to, position); }
};

const Scale& attackScale()
{
    static const Scale scale { modes::minAttackMs, modes::maxAttackMs, "10 " + format::micro + "s", "100 ms" };
    return scale;
}

const Scale& releaseScale()
{
    static const Scale scale { modes::minReleaseMs, modes::maxReleaseMs, "5 ms", "5 s" };
    return scale;
}

const Scale& ratioScale()
{
    static const Scale scale { modes::minRatio, modes::maxRatio, "1:1", "20:1" };
    return scale;
}

/** A knob on a shared scale, usable between low and high. */
Knob::Spec scaleSpec (const juce::String& label, const Scale& scale, float low, float high, float defaultValue,
                      std::function<juce::String (float)> text)
{
    Knob::Spec spec;
    spec.label = label;
    spec.toPosition = [&scale] (float v) { return scale.position (v); };
    spec.fromPosition = [&scale] (float p) { return scale.value (p); };
    spec.usableFrom = scale.position (low);
    spec.usableTo = scale.position (high);
    spec.startLabel = scale.startLabel;
    spec.endLabel = scale.endLabel;
    spec.defaultValue = defaultValue;
    spec.text = std::move (text);
    return spec;
}

/** A knob on a shared scale that clicks between the given values. */
template <size_t n>
Knob::Spec switchedSpec (const juce::String& label, const Scale& scale, const std::array<float, n>& steps, float defaultValue,
                         std::function<juce::String (float)> text)
{
    auto spec = scaleSpec (label, scale, steps.front(), steps.back(), defaultValue, std::move (text));
    spec.stepValues.assign (steps.begin(), steps.end());
    return spec;
}

/** A knob on a shared scale showing a value it doesn't let you change. */
Knob::Spec shownSpec (const juce::String& label, const Scale& scale, Knob::Look look, float position, const juce::String& text,
                      const juce::String& caption)
{
    Knob::Spec spec;
    spec.label = label;
    spec.look = look;
    spec.startLabel = scale.startLabel;
    spec.endLabel = scale.endLabel;
    spec.toPosition = [position] (float) { return position; };
    spec.text = [text] (float) { return text; };
    spec.caption = [caption] (float) { return caption; };
    return spec;
}

/** Separates two parts of a dial caption, as in "Dial 5 · Tight". */
const juce::String dot { juce::CharPointer_UTF8 (" \xc2\xb7 ") };

juce::String thresholdCaption (float db)
{
    return "= threshold " + format::decibels (db);
}

Knob::Spec amountSpec (Mode mode)
{
    const auto d = modes::defaults (mode).thresholdDb;
    Knob::Spec spec;
    spec.defaultValue = d;
    switch (mode)
    {
        case Mode::clean:
        case Mode::vca:
        {
            const auto low = mode == Mode::clean ? modes::minThresholdDb : modes::classicMinThresholdDb;
            spec.label = "Threshold";
            spec.toPosition = [low] (float v) { return (v - low) / -low; };
            spec.fromPosition = [low] (float p) { return low - low * p; };
            spec.text = [] (float v) { return format::decibels (v); };
            break;
        }
        case Mode::fet:
            spec.label = "Input";
            spec.toPosition = [] (float v) { return v / modes::classicMinThresholdDb; };
            spec.fromPosition = [] (float p) { return p * modes::classicMinThresholdDb; };
            spec.text = [] (float v) { return "+" + juce::String (juce::roundToInt (-v)) + " dB"; };
            spec.caption = thresholdCaption;
            break;
        case Mode::opto:
            spec.label = "Peak Reduction";
            spec.toPosition = [] (float v) { return v / modes::classicMinThresholdDb; };
            spec.fromPosition = [] (float p) { return p * modes::classicMinThresholdDb; };
            spec.text = [] (float v) { return juce::String (juce::roundToInt (-v / modes::optoDbPerPeakReduction)); };
            spec.caption = thresholdCaption;
            break;
    }
    return spec;
}

Knob::Spec ratioSpec (Mode mode)
{
    switch (mode)
    {
        case Mode::clean: return scaleSpec ("Ratio", ratioScale(), modes::minRatio, modes::maxRatio, 4.0f, format::ratio);
        case Mode::fet: return switchedSpec ("Ratio", ratioScale(), modes::fetRatios, 4.0f, format::ratio);
        case Mode::vca: return switchedSpec ("Ratio", ratioScale(), modes::vcaRatios, 4.0f, format::ratio);
        case Mode::opto: break;
    }
    return shownSpec ("Ratio", ratioScale(), Knob::Look::fixed, ratioScale().position (modes::optoRatio), format::approx + "3:1", "Fixed" + dot + "Soft");
}

Knob::Spec attackSpec (Mode mode)
{
    switch (mode)
    {
        case Mode::clean:
        {
            auto spec = scaleSpec ("Attack", attackScale(), modes::minAttackMs, modes::maxAttackMs, 10.0f, format::time);
            spec.heldCaption = lucky::tips::attackWord;
            return spec;
        }
        case Mode::fet:
        {
            auto spec = scaleSpec ("Attack", attackScale(), modes::fetFastestAttackMs, modes::fetSlowestAttackMs,
                                   modes::defaults (Mode::fet).attackMs, format::time);
            spec.caption = [] (float ms) { return "Dial " + juce::String (modes::fetAttackDial (ms), 1); };
            spec.heldCaption = [] (float ms) { return "Dial " + juce::String (modes::fetAttackDial (ms), 1) + dot + lucky::tips::attackWord (ms); };
            return spec;
        }
        case Mode::vca:
        {
            auto spec = switchedSpec ("Attack", attackScale(), modes::vcaAttacksMs, 10.0f, format::time);
            spec.heldCaption = lucky::tips::attackWord;
            return spec;
        }
        case Mode::opto: break;
    }
    return shownSpec ("Attack", attackScale(), Knob::Look::fixed, attackScale().position (modes::optoAttackMs), "10 ms", "Fixed");
}

/** Release; on VCA a value below zero means Auto, and on Opto the value is the release in seconds it follows. */
Knob::Spec releaseSpec (Mode mode)
{
    switch (mode)
    {
        case Mode::clean:
        {
            auto spec = scaleSpec ("Release", releaseScale(), modes::minReleaseMs, modes::maxReleaseMs, 150.0f, format::time);
            spec.heldCaption = lucky::tips::releaseWord;
            return spec;
        }
        case Mode::fet:
        {
            auto spec = scaleSpec ("Release", releaseScale(), modes::fetFastestReleaseMs, modes::fetSlowestReleaseMs,
                                   modes::defaults (Mode::fet).releaseMs, format::time);
            spec.caption = [] (float ms) { return "Dial " + juce::String (modes::fetReleaseDial (ms), 1); };
            spec.heldCaption = [] (float ms) { return "Dial " + juce::String (modes::fetReleaseDial (ms), 1) + dot + lucky::tips::releaseWord (ms); };
            return spec;
        }
        case Mode::vca:
        {
            auto spec = switchedSpec ("Release", releaseScale(), std::array<float, 5> { 100.0f, 300.0f, 600.0f, 1200.0f, -1.0f }, 300.0f,
                                      [] (float v) { return v < 0.0f ? juce::String ("Auto") : format::time (v); });
            spec.toPosition = [] (float v) { return releaseScale().position (v < 0.0f ? autoReleasePositionMs : v); };
            spec.usableFrom = releaseScale().position (100.0f);
            spec.usableTo = releaseScale().position (autoReleasePositionMs);
            return spec;
        }
        case Mode::opto: break;
    }

    auto spec = shownSpec ("Release", releaseScale(), Knob::Look::follows, 0.0f, {}, "Follows the music");
    spec.toPosition = [] (float seconds) { return releaseScale().position (seconds * 1000.0f); };
    spec.text = [] (float seconds) { return format::time (seconds * 1000.0f); };
    return spec;
}

/** The last knob as a Mix crossfade (0 to 100% compressed) or, in Blend, the level the compressed signal is added
    at while the dry one stays at 0 dB. Its label is replaced by the MIX | BLEND tabs. */
Knob::Spec blendSpec (const juce::RangedAudioParameter& blend)
{
    Knob::Spec spec;
    spec.toPosition = [&blend] (float db) { return blend.convertTo0to1 (db); };
    spec.fromPosition = [&blend] (float p) { return blend.convertFrom0to1 (p); };
    spec.defaultValue = -12.0f;
    spec.startLabel = format::minus + juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e"));
    spec.endLabel = "+6 dB";
    spec.text = [] (float db) { return db <= LuckyCompProcessor::blendSilentDb ? format::minus + juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e")) + " dB" : format::decibels (db); };
    spec.caption = [] (float) { return juce::String ("Dry stays at 0 dB"); };
    return spec;
}

Knob::Spec mixSpec()
{
    Knob::Spec spec;
    spec.toPosition = [] (float v) { return v / 100.0f; };
    spec.fromPosition = [] (float p) { return p * 100.0f; };
    spec.defaultValue = 100.0f;
    spec.startLabel = "Dry";
    spec.endLabel = "Comp";
    spec.text = [] (float v) { return juce::String (juce::roundToInt (v)) + "%"; };
    spec.caption = [] (float v) { return v >= 99.5f ? juce::String() : v <= 0.5f ? juce::String ("No compression") : juce::String ("Parallel"); };
    return spec;
}

/** How far either side of the 1× tick (as a fraction of the dial) a drag snaps to exactly 1×: about 6 px of drag. */
constexpr float likeHardwareDetent = 0.03f;

/** Colour from Off straight to 1× (typical hardware, ticked just after Off) and on to 20×, exaggerated so it can
    be heard. Clean has none. */
Knob::Spec colourSpec (Mode mode)
{
    Knob::Spec spec;
    spec.label = "Colour";
    if (mode == Mode::clean)
    {
        spec.look = Knob::Look::unavailable;
        return spec;
    }

    const juce::String times (juce::CharPointer_UTF8 ("\xc3\x97"));
    spec.toPosition = [] (float v) { return lucky::Colour::positionOf (v / 100.0f); };
    spec.fromPosition = [] (float p) { return lucky::Colour::amountAt (p) * 100.0f; };
    spec.defaultValue = 100.0f;
    spec.tickAt = lucky::Colour::offPosition;
    spec.tickDetent = likeHardwareDetent;
    spec.startLabel = "Off";
    spec.endLabel = juce::String (juce::roundToInt (lucky::Colour::maxAmount)) + times;
    spec.text = [times] (float v)
    {
        if (v < 0.5f)
            return juce::String ("Off");
        return (v >= 995.0f ? juce::String (juce::roundToInt (v / 100.0f)) : juce::String (v / 100.0f, 1)) + times;
    };
    spec.caption = [] (float v)
    {
        if (juce::approximatelyEqual (v, 100.0f))
            return juce::String ("Like the hardware");
        return v > 100.0f ? juce::String ("Exaggerated") : juce::String();
    };
    return spec;
}

} // namespace

LuckyCompView::ModeTab::ModeTab (Mode m) : juce::Button (modes::name (m)), mode (m)
{
    setWantsKeyboardFocus (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void LuckyCompView::ModeTab::paintButton (juce::Graphics& g, bool isMouseOver, bool)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto selected = getToggleState();
    if (! selected && isMouseOver)
    {
        g.setColour (lucky::theme::text.withAlpha (0.04f));
        g.fillRect (bounds);
    }

    const auto font = lucky::theme::labelFont (13.0f);
    const auto label = getButtonText().toUpperCase();
    const auto textWidth = juce::GlyphArrangement::getStringWidth (font, label);
    auto content = bounds.withSizeKeepingCentre (textWidth + 15.0f, bounds.getHeight());
    g.setColour (comparing ? lucky::theme::greyedOut : lucky::theme::modeColour (mode));
    g.fillEllipse (content.removeFromLeft (7.0f).withSizeKeepingCentre (7.0f, 7.0f));
    g.setColour (selected || isMouseOver ? lucky::theme::text : lucky::theme::textDim);
    g.setFont (font);
    g.drawText (label, content.withTrimmedLeft (8.0f), juce::Justification::centredLeft);

    if (selected)
    {
        g.setColour (comparing ? lucky::theme::greyedOut : lucky::theme::modeColour (mode));
        g.fillRect (bounds.withTop (bounds.getBottom() - 2.0f));
    }
}

LuckyCompView::LuckyCompView (LuckyCompProcessor& processor)
    : owner (processor),
      liveView (processor.getEngine(), [&processor] { return processor.getSampleRate(); })
{
    for (int m = 0; m < lucky::numModes; ++m)
    {
        auto& tab = tabs[static_cast<size_t> (m)];
        tab = std::make_unique<ModeTab> (static_cast<Mode> (m));
        tab->onClick = [this, m]
        {
            grabKeyboardFocus();
            owner.switchMode (static_cast<Mode> (m));
            setComparing (false);
        };
        addAndMakeVisible (*tab);
    }

    lucky::setUpMenuButton (menuButton);
    menuButton.onClick = [this] { showMenu(); };
    addAndMakeVisible (menuButton);

    addAndMakeVisible (liveView);
    addAndMakeVisible (hitView);

    for (int c = 0; c < numControls; ++c)
    {
        auto& knob = knobs[static_cast<size_t> (c)];
        const auto control = static_cast<Control> (c);
        knob.onGestureStart = [this, control]
        {
            grabKeyboardFocus();
            auto& id = gestureIds[static_cast<size_t> (control)];
            id = parameterIdFor (control);
            owner.getParameters().getParameter (id)->beginChangeGesture();
            if (control == colour)
                hitView.holdColour (true);
            if (control == release)
                owner.getParameters().getParameter (LuckyCompProcessor::autoReleaseId)->beginChangeGesture();
        };
        knob.onGestureEnd = [this, control]
        {
            auto& id = gestureIds[static_cast<size_t> (control)];
            if (id.isNotEmpty())
                owner.getParameters().getParameter (id)->endChangeGesture();
            id.clear();
            if (control == colour)
                hitView.holdColour (false);
            if (control == release)
                owner.getParameters().getParameter (LuckyCompProcessor::autoReleaseId)->endChangeGesture();
        };
        knob.onChange = [this, control] (float value)
        {
            setKnobValue (control, value);
            if (control == colour)
                hitView.revealColour();
        };
        addAndMakeVisible (knob);
    }
    knobs[colour].setTooltip ("Colour: the tone of the classic hardware. 1" + juce::String (juce::CharPointer_UTF8 ("\xc3\x97"))
                              + " is like the hardware, which is subtle; turn it up to hear it.");
    mixTabs.setTooltips ({ "Mix: crossfade between the original and the compressed sound. Lower it to bring the hits' punch back "
                           "while keeping the quiet parts lifted (parallel compression).",
                           "Blend: the original stays at 0 dB and the knob adds the compressed sound underneath, as in New York "
                           "style parallel compression." });
    mixTabs.onChoose = [this] (int index)
    {
        grabKeyboardFocus();
        setParameter (LuckyCompProcessor::mixModeId, static_cast<float> (index));
        sync();
    };
    addAndMakeVisible (mixTabs);

    compareHint.setTooltip ("Hear your sound without compression, at the same loudness, then switch back.");
    compareHint.onClick = [this]
    {
        grabKeyboardFocus();
        toggleComparing();
    };
    addChildComponent (compareHint);
    compareNotice.onTurnOff = [this]
    {
        grabKeyboardFocus();
        setComparing (false);
    };
    addChildComponent (compareNotice);

    matchSwitch.setTooltip ("Turns the compressed sound up to match the original's loudness, so you hear the shape, not the level");
    matchSwitch.onToggle = [this] (bool on)
    {
        grabKeyboardFocus();
        setParameter (LuckyCompProcessor::matchId, on ? 1.0f : 0.0f);
    };
    addAndMakeVisible (matchSwitch);

    tipsShown = settings->getBool (showTipsKey, true);
    compareHint.setVisible (tipsShown);
    labShown = settings->getBool (showLabKey, true);
    hitView.setVisible (labShown);
    liveView.setReductionShown (settings->getBool (showReductionKey, false));
    liveView.onEyeClicked = [this] { eyeClicked(); };
    hitView.onPlayHit = [this]
    {
        grabKeyboardFocus();
        owner.playTestHit();
        hitView.flashHit();
    };
    setWantsKeyboardFocus (true);
    sync();
    startTimerHz (30);
}

void LuckyCompView::setParameter (const juce::String& id, float value)
{
    auto* parameter = owner.getParameters().getParameter (id);
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    parameter->endChangeGesture();
}

void LuckyCompView::setComparing (bool on)
{
    setParameter (LuckyCompProcessor::bypassId, on ? 1.0f : 0.0f);
    sync();
}

void LuckyCompView::setKnobValue (Control control, float value)
{
    const auto set = [this] (const juce::String& id, float v)
    {
        auto* parameter = owner.getParameters().getParameter (id);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (v));
    };

    if (control == release)
    {
        set (LuckyCompProcessor::autoReleaseId, value < 0.0f ? 1.0f : 0.0f);
        if (value >= 0.0f)
            set (LuckyCompProcessor::releaseId, value);
    }
    else if (control != numControls)
    {
        set (parameterIdFor (control), value);
    }
    sync();
}

juce::String LuckyCompView::parameterIdFor (Control control) const
{
    switch (control)
    {
        case amount: return LuckyCompProcessor::thresholdId;
        case ratio: return LuckyCompProcessor::ratioId;
        case attack: return LuckyCompProcessor::attackId;
        case release: return LuckyCompProcessor::releaseId;
        case colour: return LuckyCompProcessor::colourId;
        case mix: return owner.isBlendMode() ? LuckyCompProcessor::blendId : LuckyCompProcessor::mixId;
        case numControls: break;
    }
    return {};
}

float LuckyCompView::releaseSeconds() const
{
    const auto& engine = owner.getEngine();
    return engine.isHearingAudio() ? engine.getReleaseSeconds() : hitView.getReleaseSeconds();
}

void LuckyCompView::applyMode (Mode mode)
{
    shownMode = mode;
    for (auto& tab : tabs)
        tab->setToggleState (tab->mode == mode, juce::dontSendNotification);

    knobs[amount].setSpec (amountSpec (mode));
    knobs[ratio].setSpec (ratioSpec (mode));
    knobs[attack].setSpec (attackSpec (mode));
    knobs[release].setSpec (releaseSpec (mode));
    knobs[colour].setSpec (colourSpec (mode));
    const auto clean = mode == Mode::clean;
    knobs[attack].setTooltip (clean ? "FET reaches 20 " + format::micro + "s to 0.8 ms, VCA 0.1 to 30 ms, Opto is fixed at 10 ms. "
                                      "Switch modes to see each one's range on this dial."
                                    : juce::String());
    knobs[release].setTooltip (clean ? juce::String ("FET reaches 50 ms to 1.1 s, VCA 0.1 to 1.2 s or Auto, and Opto follows the music. "
                                                     "Switch modes to see each one's range on this dial.")
                                     : juce::String());
    modeTip = lucky::tips::mode (mode);
    applyAccent();
}

juce::Colour LuckyCompView::accent() const
{
    return comparing ? lucky::theme::greyedOut : lucky::theme::modeColour (shownMode.value_or (Mode::clean));
}

void LuckyCompView::applyAccent()
{
    const auto colourNow = accent();
    const auto shared = comparing ? lucky::theme::greyedOut : lucky::theme::text;
    for (auto& knob : knobs)
        knob.setColour (colourNow);
    knobs[mix].setColour (shared);
    mixTabs.setColour (shared);
    liveView.setColour (colourNow);
    hitView.setColour (colourNow);
    matchSwitch.setColour (colourNow);
    for (auto& tab : tabs)
    {
        tab->comparing = comparing;
        tab->repaint();
    }
    compareNotice.setVisible (comparing);
    repaint();
}

void LuckyCompView::sync()
{
    const auto mode = owner.getMode();
    if (shownMode != mode)
        applyMode (mode);

    const auto controls = modes::constrain (mode, owner.getControls());
    const auto detector = owner.getDetectorSettings();
    hitView.update (detector, lucky::example_hit::windowSeconds, owner.getBlend(), owner.getColour());
    knobs[colour].setValue (owner.getParameters().getRawParameterValue (LuckyCompProcessor::colourId)->load());
    const auto blendMode = owner.isBlendMode();
    if (shownBlendMode != blendMode)
    {
        shownBlendMode = blendMode;
        knobs[mix].setSpec (blendMode ? blendSpec (*owner.getParameters().getParameter (LuckyCompProcessor::blendId)) : mixSpec());
        mixTabs.setChosen (blendMode ? 1 : 0);
    }
    knobs[mix].setValue (owner.getParameters().getRawParameterValue (blendMode ? LuckyCompProcessor::blendId : LuckyCompProcessor::mixId)->load());

    knobs[amount].setValue (controls.thresholdDb);
    knobs[ratio].setValue (controls.ratio);
    knobs[attack].setValue (controls.attackMs);
    if (mode == Mode::opto)
        knobs[release].setValue (releaseSeconds());
    else if (mode == Mode::vca && controls.autoRelease)
        knobs[release].setValue (-1.0f);
    else
        knobs[release].setValue (controls.releaseMs);

    auto releaseSpecNow = knobs[release].getSpec();
    if (mode == Mode::vca)
    {
        const auto autoCaption = controls.autoRelease ? format::approx + " " + format::time (releaseSeconds() * 1000.0f) + " now" : juce::String();
        releaseSpecNow.caption = [autoCaption] (float) { return autoCaption; };
        releaseSpecNow.heldCaption = [autoCaption] (float ms) { return autoCaption.isNotEmpty() ? autoCaption : lucky::tips::releaseWord (ms); };
        knobs[release].setSpec (releaseSpecNow);
    }

    const auto& parameters = owner.getParameters();
    if (const auto comparingNow = parameters.getRawParameterValue (LuckyCompProcessor::bypassId)->load() > 0.5f; comparing != comparingNow)
    {
        comparing = comparingNow;
        applyAccent();
    }
    const auto matching = parameters.getRawParameterValue (LuckyCompProcessor::matchId)->load() > 0.5f;
    matchSwitch.setOn (matching);
    compareNotice.setBody (matching ? "Your sound without compression, at the same loudness." : "Your sound without compression.");
    liveView.setThreshold (detector.thresholdDb);
    liveView.repaint();
}

void LuckyCompView::timerCallback()
{
    if (focusTriesLeft > 0 && isShowing())
    {
        grabKeyboardFocus();
        focusTriesLeft = hasKeyboardFocus (false) ? 0 : focusTriesLeft - 1;
    }
    sync();
}

juce::Rectangle<int> LuckyCompView::stripArea() const
{
    return getLocalBounds().removeFromBottom (stripHeight);
}

juce::Rectangle<int> LuckyCompView::tipsArea() const
{
    if (! tipsShown)
        return {};
    return getLocalBounds().withTrimmedTop (topBarHeight).removeFromTop (tipsHeight);
}

void LuckyCompView::resized()
{
    auto bounds = getLocalBounds();
    auto top = bounds.removeFromTop (topBarHeight);
    menuButton.setBounds (top.removeFromRight (menuWidth));
    top.removeFromRight (1);
    top.removeFromLeft (brandWidth + 1);
    const auto tabWidth = top.getWidth() / lucky::numModes;
    for (int m = lucky::numModes; --m >= 0;)
        tabs[static_cast<size_t> (m)]->setBounds (m == 0 ? top : top.removeFromRight (tabWidth));

    auto strip = bounds.removeFromBottom (stripHeight);
    if (tipsShown)
        bounds.removeFromTop (tipsHeight);

    const auto liveWidth = labShown ? juce::roundToInt (static_cast<float> (bounds.getWidth()) * liveShare) : bounds.getWidth();
    liveView.setBounds (bounds.removeFromLeft (liveWidth));
    bounds.removeFromLeft (1);
    hitView.setBounds (bounds);

    const auto matchWidth = matchSwitch.getIdealWidth();
    matchSwitch.setBounds (liveView.getRight() - 18 - lucky::LiveView::eyeWidth - lucky::LiveView::readoutWidth - 16 - matchWidth,
                           liveView.getY() + 10, matchWidth, 22);
    matchSwitch.toFront (false);

    if (tipsShown)
        compareHint.setBounds (tipsArea().reduced (16, 0).removeFromRight (compareHint.getIdealWidth()).withSizeKeepingCentre (compareHint.getIdealWidth(), 26));
    compareNotice.setCentrePosition (liveView.getBounds().getCentre().translated (0, 12));
    compareNotice.toFront (false);

    strip.removeFromTop (14);
    const auto cellWidth = strip.getWidth() / numControls;
    for (auto& knob : knobs)
        knob.setBounds (strip.removeFromLeft (cellWidth).withHeight (Knob::labelHeight + Knob::dialSize + Knob::captionHeight + 6));
    mixTabs.setBounds (knobs[mix].getBounds().withHeight (Knob::labelHeight + 4).withSizeKeepingCentre (130, Knob::labelHeight + 4));
}

void LuckyCompView::paint (juce::Graphics& g)
{
    using namespace lucky;
    g.fillAll (theme::background);

    const auto top = getLocalBounds().removeFromTop (topBarHeight);
    g.setColour (theme::topBar);
    g.fillRect (top);
    g.setColour (theme::background);
    g.fillRect (top.getX() + brandWidth, top.getY(), 1, top.getHeight());
    g.fillRect (top.getRight() - menuWidth - 1, top.getY(), 1, top.getHeight());

    const auto colourOfMode = accent();
    const auto logo = juce::Rectangle<float> (16.0f, (float) (topBarHeight - 28) * 0.5f, 28.0f, 28.0f);
    ripple::paint (g, logo, colourOfMode);
    g.setColour (theme::text);
    g.setFont (theme::labelFont (17.0f).withStyle ("Bold"));
    g.drawText ("LUCKY COMP", juce::Rectangle<int> (60, 0, brandWidth - 64, topBarHeight), juce::Justification::centredLeft);

    g.setColour (theme::strip);
    g.fillRect (liveView.getRight(), liveView.getY(), 1, liveView.getHeight());

    if (tipsShown)
    {
        const auto area = tipsArea();
        g.setColour (theme::strip);
        g.fillRect (area);
        g.setColour (theme::outline);
        g.fillRect (area.withTop (area.getBottom() - 1));
        auto text = area.reduced (22, 10).withTrimmedRight (compareHint.getIdealWidth() + 12);
        g.setFont (juce::FontOptions (14.0f));
        g.setColour (theme::textDim);
        g.drawFittedText (modeTip, text, juce::Justification::centredLeft, 1, 0.9f);
    }

    auto strip = stripArea();
    g.setColour (theme::strip);
    g.fillRect (strip);
    g.setColour (theme::outline);
    g.fillRect (strip.withHeight (1));

}

bool LuckyCompView::keyPressed (const juce::KeyPress& key)
{
    if (key.getTextCharacter() == 's' || key.getTextCharacter() == 'S')
    {
        owner.playTestHit();
        hitView.flashHit();
        return true;
    }
    if (key.getTextCharacter() == 'c' || key.getTextCharacter() == 'C')
    {
        toggleComparing();
        return true;
    }
    return false;
}

void LuckyCompView::mouseDown (const juce::MouseEvent&)
{
    grabKeyboardFocus();
}

void LuckyCompView::showMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader (juce::String ("Lucky Comp v") + JucePlugin_VersionString);
    if (addWindowSizeMenu)
        addWindowSizeMenu (menu);
    juce::PopupMenu::Item compareItem ("Compare with uncompressed");
    compareItem.setTicked (comparing).setAction ([this] { toggleComparing(); });
    compareItem.shortcutKeyDescription = "C";
    menu.addItem (compareItem);
    menu.addSeparator();
    menu.addItem ("Show mode info", true, tipsShown, [this]
                  {
                      tipsShown = ! tipsShown;
                      settings->setBool (showTipsKey, tipsShown);
                      compareHint.setVisible (tipsShown);
                      resized();
                      repaint();
                  });
    menu.addItem ("Show Lab", true, labShown, [this]
                  {
                      labShown = ! labShown;
                      settings->setBool (showLabKey, labShown);
                      hitView.setVisible (labShown);
                      resized();
                      repaint();
                  });
    menu.addItem ("Ask before showing GR", true, settings->getBool (askBeforeReductionKey, true),
                  [this] { settings->setBool (askBeforeReductionKey, ! settings->getBool (askBeforeReductionKey, true)); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton));
}

void LuckyCompView::setReductionShown (bool shown)
{
    liveView.setReductionShown (shown);
    settings->setBool (showReductionKey, shown);
}

void LuckyCompView::eyeClicked()
{
    if (liveView.isReductionShown() || ! settings->getBool (askBeforeReductionKey, true))
    {
        setReductionShown (! liveView.isReductionShown());
        return;
    }

    const juce::Component::SafePointer<LuckyCompView> safe (this);
    auto prompt = std::make_unique<lucky::RevealPrompt> (
        lucky::theme::modeColour (owner.getMode()),
        [safe] (bool show, bool dontAskAgain)
        {
            if (safe == nullptr)
                return;
            if (dontAskAgain)
                safe->settings->setBool (askBeforeReductionKey, false);
            if (show)
                safe->setReductionShown (true);
        },
        [safe]
        {
            if (safe != nullptr)
                safe->grabKeyboardFocus();
        });
    juce::CallOutBox::launchAsynchronously (std::move (prompt), getReductionButtonArea(), this);
}

LuckyCompEditor::LuckyCompEditor (LuckyCompProcessor& processorToEdit)
    : ScaledEditor (processorToEdit, LuckyCompView::baseWidth, LuckyCompView::baseHeight), view (processorToEdit)
{
    view.addWindowSizeMenu = [this] (juce::PopupMenu& menu) { ScaledEditor::addWindowSizeMenu (menu); };
    setView (view);
}

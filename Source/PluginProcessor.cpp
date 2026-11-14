#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const juce::Identifier memoryType { "ModeMemory" };
const juce::Identifier modeProperty { "mode" };
/** The level-match gain (dB) for the saved settings, so a reopened session starts at the right level. */
const juce::Identifier matchGainProperty { "matchGainDb" };
/** The compressed signal's own level-match gain (dB), applied before Mix or Blend. */
const juce::Identifier wetMatchGainProperty { "wetMatchGainDb" };
/** The uncompressed signal's gain (dB) while comparing, below 0 when peaks held the make-up down. */
const juce::Identifier uncompressedMatchGainProperty { "uncompressedMatchGainDb" };

/** A saved level-match gain in dB, or nothing if it's missing or not a finite number; limited to what the matcher can set. */
std::optional<float> readMatchGainDb (const juce::var& saved)
{
    if (saved.isVoid())
        return std::nullopt;
    const auto db = static_cast<float> (saved);
    if (! std::isfinite (db))
        return std::nullopt;
    return std::clamp (db, lucky::LevelMatcher::minGainDb, lucky::LevelMatcher::maxGainDb);
}

/** A log-scaled range, so a knob spends as much travel on 0.1-1 ms as on 10-100 ms. */
juce::NormalisableRange<float> logRange (float start, float end)
{
    return { start, end,
             [] (float s, float e, float proportion) { return s * std::pow (e / s, proportion); },
             [] (float s, float e, float value) { return std::log (value / s) / std::log (e / s); } };
}

juce::String formatMs (float ms, int)
{
    if (ms < 1.0f)
        return juce::String (juce::roundToInt (ms * 1000.0f)) + " us";
    if (ms < 10.0f)
        return juce::String (ms, 1) + " ms";
    if (ms < 1000.0f)
        return juce::String (juce::roundToInt (ms)) + " ms";
    return juce::String (ms / 1000.0f, 2) + " s";
}
} // namespace

LuckyCompProcessor::LuckyCompProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("Lucky Comp level match"),
      parameters (*this, nullptr, "LuckyComp", createLayout())
{
    for (int m = 0; m < lucky::numModes; ++m)
        memory[static_cast<size_t> (m)] = lucky::modes::defaults (static_cast<lucky::Mode> (m));

    modeParameter = parameters.getRawParameterValue (modeId);
    thresholdParameter = parameters.getRawParameterValue (thresholdId);
    ratioParameter = parameters.getRawParameterValue (ratioId);
    attackParameter = parameters.getRawParameterValue (attackId);
    releaseParameter = parameters.getRawParameterValue (releaseId);
    autoReleaseParameter = parameters.getRawParameterValue (autoReleaseId);
    bypassParameter = parameters.getRawParameterValue (bypassId);
    matchParameter = parameters.getRawParameterValue (matchId);
    mixParameter = parameters.getRawParameterValue (mixId);
    mixModeParameter = parameters.getRawParameterValue (mixModeId);
    blendParameter = parameters.getRawParameterValue (blendId);
    colourParameter = parameters.getRawParameterValue (colourId);
    startThread (juce::Thread::Priority::low);
}

LuckyCompProcessor::~LuckyCompProcessor()
{
    stopThread (2000);
}

void LuckyCompProcessor::run()
{
    while (! threadShouldExit())
    {
        if (! isNonRealtime())
            updateLevelMatch();
        wait (15);
    }
}

void LuckyCompProcessor::updateLevelMatch()
{
    const juce::SpinLock::ScopedTryLockType lock (matchLock);
    if (! lock.isLocked())
        return;

    const auto settings = getDetectorSettings();
    const auto blend = getBlend();
    const auto& matcher = engine.getMatcher();
    const auto sound = matcher.getSoundRecorded();
    const auto full = sound >= matcher.getHistoryLength();
    const auto refresh = rememberedMatch ? full
                                         : ! full && sound - matchedSound >= static_cast<long long> (matchRefreshSeconds * matcher.getSampleRate());
    if (matchedSettings == settings && matchedBlend == blend && ! refresh)
        return;

    if (const auto makeup = matcher.predictMakeup (settings, blend))
    {
        engine.setWetMakeupTarget (makeup->wet);
        engine.setMakeupTarget (makeup->trim);
        engine.setUncompressedTarget (makeup->uncompressed);
        matchedSettings = settings;
        matchedBlend = blend;
        matchedSound = sound;
        rememberedMatch = false;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout LuckyCompProcessor::createLayout()
{
    using namespace lucky::modes;
    const auto defaults = lucky::modes::defaults (lucky::Mode::clean);
    const auto dB = juce::AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (
        [] (float v, int) { return juce::String (v, 1) + " dB"; });
    const auto ms = juce::AudioParameterFloatAttributes().withStringFromValueFunction (formatMs);
    const auto ratio = juce::AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return juce::String (v, v < 10.0f ? 1 : 0) + ":1"; });

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { modeId, 1 }, "Mode",
                                                              juce::StringArray { "Clean", "FET", "Opto", "VCA" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { thresholdId, 1 }, "Threshold",
                                                             juce::NormalisableRange<float> (minThresholdDb, maxThresholdDb), defaults.thresholdDb, dB));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ratioId, 1 }, "Ratio", logRange (minRatio, maxRatio), defaults.ratio, ratio));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { attackId, 1 }, "Attack", logRange (minAttackMs, maxAttackMs), defaults.attackMs, ms));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { releaseId, 1 }, "Release", logRange (minReleaseMs, maxReleaseMs), defaults.releaseMs, ms));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { autoReleaseId, 1 }, "Auto release", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { bypassId, 1 }, "Compare with uncompressed", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { matchId, 1 }, "Level match", true));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { mixId, 1 }, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f,
                                                             juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (
                                                                 [] (float v, int) { return juce::String (juce::roundToInt (v)) + "%"; })));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { mixModeId, 1 }, "Mix mode", mixModes, 0));
    const juce::NormalisableRange<float> colourRange (0.0f, lucky::Colour::maxAmount * 100.0f,
                                                     [] (float, float, float p) { return lucky::Colour::amountAt (p) * 100.0f; },
                                                     [] (float, float, float v) { return lucky::Colour::positionOf (v / 100.0f); });
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { colourId, 1 }, "Colour", colourRange, 100.0f,
                                                             juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (
                                                                 [] (float v, int) { return juce::String (v / 100.0f, 2) + "x"; })));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { blendId, 1 }, "Blend",
                                                             juce::NormalisableRange<float> (blendSilentDb, 6.0f, 0.1f, 2.5f), -12.0f,
                                                             juce::AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (
                                                                 [] (float v, int) { return v <= blendSilentDb ? juce::String ("-inf dB") : juce::String (v, 1) + " dB"; })));
    return layout;
}

void LuckyCompProcessor::prepareToPlay (double sampleRate, int)
{
    const juce::SpinLock::ScopedLockType lock (matchLock);
    engine.setSettings (getDetectorSettings());
    engine.setBlend (getBlend());
    engine.setColour (getColour());
    engine.prepare (sampleRate);
    matchedSound = 0;
    rememberedMatch = matchedSettings.has_value();
}

bool LuckyCompProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

lucky::Mode LuckyCompProcessor::getMode() const noexcept
{
    return static_cast<lucky::Mode> (juce::jlimit (0, lucky::numModes - 1, juce::roundToInt (modeParameter->load())));
}

lucky::Controls LuckyCompProcessor::getControls() const noexcept
{
    return { thresholdParameter->load(), ratioParameter->load(), attackParameter->load(), releaseParameter->load(),
             autoReleaseParameter->load() > 0.5f };
}

void LuckyCompProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    engine.setSettings (getDetectorSettings());
    engine.setBypass (bypassParameter->load() > 0.5f);
    engine.setMatch (matchParameter->load() > 0.5f);
    engine.setBlend (getBlend());
    engine.setColour (getColour());
    engine.process (buffer);

    if (isNonRealtime())
        updateLevelMatch();
}

void LuckyCompProcessor::setControls (const lucky::Controls& controls)
{
    const auto set = [this] (const juce::String& id, float value)
    {
        auto* parameter = parameters.getParameter (id);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        parameter->endChangeGesture();
    };
    set (thresholdId, controls.thresholdDb);
    set (ratioId, controls.ratio);
    set (attackId, controls.attackMs);
    set (releaseId, controls.releaseMs);
    set (autoReleaseId, controls.autoRelease ? 1.0f : 0.0f);
}

void LuckyCompProcessor::switchMode (lucky::Mode mode)
{
    const auto current = getMode();
    if (mode == current)
        return;

    memory[static_cast<size_t> (current)] = lucky::modes::constrain (current, getControls());
    colourMemory[static_cast<size_t> (current)] = colourParameter->load();
    auto* parameter = parameters.getParameter (modeId);
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (mode)));
    parameter->endChangeGesture();
    setControls (memory[static_cast<size_t> (mode)]);
    auto* colour = parameters.getParameter (colourId);
    colour->beginChangeGesture();
    colour->setValueNotifyingHost (colour->convertTo0to1 (colourMemory[static_cast<size_t> (mode)]));
    colour->endChangeGesture();
}

void LuckyCompProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.removeChild (state.getChildWithName (memoryType), nullptr);

    memory[static_cast<size_t> (getMode())] = lucky::modes::constrain (getMode(), getControls());
    colourMemory[static_cast<size_t> (getMode())] = colourParameter->load();
    for (int m = 0; m < lucky::numModes; ++m)
    {
        const auto& c = memory[static_cast<size_t> (m)];
        juce::ValueTree entry (memoryType);
        entry.setProperty (modeProperty, m, nullptr);
        entry.setProperty (thresholdId, c.thresholdDb, nullptr);
        entry.setProperty (ratioId, c.ratio, nullptr);
        entry.setProperty (attackId, c.attackMs, nullptr);
        entry.setProperty (releaseId, c.releaseMs, nullptr);
        entry.setProperty (autoReleaseId, c.autoRelease, nullptr);
        entry.setProperty (colourId, colourMemory[static_cast<size_t> (m)], nullptr);
        state.appendChild (entry, nullptr);
    }

    state.removeProperty (matchGainProperty, nullptr);
    state.removeProperty (wetMatchGainProperty, nullptr);
    {
        const juce::SpinLock::ScopedLockType lock (matchLock);
        if (matchedSettings == getDetectorSettings() && matchedBlend == getBlend())
        {
            state.setProperty (matchGainProperty, juce::Decibels::gainToDecibels (engine.getMakeupTarget()), nullptr);
            state.setProperty (wetMatchGainProperty, juce::Decibels::gainToDecibels (engine.getWetMakeupTarget()), nullptr);
            state.setProperty (uncompressedMatchGainProperty, juce::Decibels::gainToDecibels (engine.getUncompressedTarget()), nullptr);
        }
    }

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LuckyCompProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    for (const auto& entry : state)
    {
        if (! entry.hasType (memoryType))
            continue;
        const auto m = static_cast<int> (entry.getProperty (modeProperty, -1));
        if (m < 0 || m >= lucky::numModes)
            continue;
        const auto fallback = lucky::modes::defaults (static_cast<lucky::Mode> (m));
        memory[static_cast<size_t> (m)] = { static_cast<float> (entry.getProperty (thresholdId, fallback.thresholdDb)),
                                            static_cast<float> (entry.getProperty (ratioId, fallback.ratio)),
                                            static_cast<float> (entry.getProperty (attackId, fallback.attackMs)),
                                            static_cast<float> (entry.getProperty (releaseId, fallback.releaseMs)),
                                            static_cast<bool> (entry.getProperty (autoReleaseId, fallback.autoRelease)) };
        colourMemory[static_cast<size_t> (m)] = static_cast<float> (entry.getProperty (colourId, 100.0f));
    }

    for (int i = state.getNumChildren(); --i >= 0;)
        if (state.getChild (i).hasType (memoryType))
            state.removeChild (i, nullptr);
    const auto savedMatch = readMatchGainDb (state.getProperty (matchGainProperty));
    const auto savedWetMatch = readMatchGainDb (state.getProperty (wetMatchGainProperty));
    const auto savedUncompressed = readMatchGainDb (state.getProperty (uncompressedMatchGainProperty)).value_or (0.0f);
    state.removeProperty (matchGainProperty, nullptr);
    state.removeProperty (wetMatchGainProperty, nullptr);
    state.removeProperty (uncompressedMatchGainProperty, nullptr);
    parameters.replaceState (state);

    if (! savedMatch || ! savedWetMatch)
        return;
    const juce::SpinLock::ScopedLockType lock (matchLock);
    matchedSettings = getDetectorSettings();
    matchedBlend = getBlend();
    matchedSound = 0;
    rememberedMatch = true;
    engine.setWetMakeupTarget (juce::Decibels::decibelsToGain (*savedWetMatch), true);
    engine.setMakeupTarget (juce::Decibels::decibelsToGain (*savedMatch), true);
    engine.setUncompressedTarget (juce::Decibels::decibelsToGain (savedUncompressed), true);
}

void LuckyCompProcessor::playTestHit()
{
    const auto rate = getSampleRate() > 0.0 ? getSampleRate() : lucky::example_hit::sampleRate;
    engine.playTestHit (lucky::example_hit::renderHit (getDetectorSettings(), rate, matchParameter->load() > 0.5f, getBlend(), getColour()));
}

juce::AudioProcessorEditor* LuckyCompProcessor::createEditor()
{
    return new LuckyCompEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LuckyCompProcessor();
}

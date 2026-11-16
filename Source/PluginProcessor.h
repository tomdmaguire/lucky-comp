#pragma once

#include "dsp/CompressorEngine.h"
#include "dsp/Modes.h"
#include <juce_audio_processors/juce_audio_processors.h>

class LuckyCompressorProcessor final : public juce::AudioProcessor,
                                 private juce::Thread
{
public:
    /** How much new sound, at the start, triggers a fresh level-match prediction (until the history is full). */
    static constexpr double matchRefreshSeconds = 0.25;
    /** Level match predicts at most this often (of wall time, or of audio when rendering offline) while settings
        move, since each prediction replays 3 s of audio. */
    static constexpr double matchIntervalSeconds = 0.05;

    static inline const juce::String modeId { "mode" };
    static inline const juce::String thresholdId { "threshold" };
    static inline const juce::String ratioId { "ratio" };
    static inline const juce::String attackId { "attack" };
    static inline const juce::String releaseId { "release" };
    static inline const juce::String autoReleaseId { "autoRelease" };
    static inline const juce::String bypassId { "bypass" };
    static inline const juce::String matchId { "match" };
    static inline const juce::String mixId { "mix" };
    static inline const juce::String mixModeId { "mixMode" };
    static inline const juce::String blendId { "blend" };
    static inline const juce::String colourId { "colour" };
    static inline const juce::StringArray mixModes { "Mix", "Blend" };
    /** Blend at or below this is silent (−∞), as in Lucky Reverb. */
    static constexpr float blendSilentDb = -60.0f;

    LuckyCompressorProcessor();
    ~LuckyCompressorProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** Message thread: switches mode, saving the current controls for the old mode and loading the new mode's. */
    void switchMode (lucky::Mode);

    lucky::Mode getMode() const noexcept;

    /** The controls as the host parameters hold them now. */
    lucky::Controls getControls() const noexcept;

    /** The detector settings the current mode and controls resolve to. */
    lucky::DetectorSettings getDetectorSettings() const noexcept { return lucky::modes::resolve (getMode(), getControls()); }

    /** The tone the Colour knob adds in the current mode. */
    lucky::ColourSettings getColour() const noexcept
    {
        return { getMode(), colourParameter->load() / 100.0f, getDetectorSettings().thresholdDb };
    }

    /** True when the dry signal stays at 0 dB and Blend adds the compressed one; false for a Mix crossfade. */
    bool isBlendMode() const noexcept { return mixModeParameter->load() > 0.5f; }

    /** How the compressed signal is combined with the dry one, from the Mix or Blend control. */
    lucky::Blend getBlend() const noexcept
    {
        return isBlendMode() ? lucky::Blend::added (blendParameter->load(), blendSilentDb) : lucky::Blend::mix (mixParameter->load());
    }

    /** Re-predicts the level-match gain if the settings changed, or while the history is still filling with sound,
        and returns true if it did. A remembered gain (from the saved session, or from before the host restarted
        audio) is kept until the history is full, rather than replaced by guesses from the first fraction of a second.
        Runs on a background thread, and on the audio thread when rendering offline, at most every
        matchIntervalSeconds and only while Level Match is on; tests call it directly. */
    bool updateLevelMatch();

    /** Turns the background level-match thread's predictions on or off (on by default). Tests and tools turn it off
        so their direct updateLevelMatch() calls never race with it. */
    void setBackgroundMatching (bool shouldMatch) noexcept { backgroundMatching.store (shouldMatch); }

    /** True while Level Match is on. */
    bool isMatching() const noexcept { return matchParameter->load() > 0.5f; }

    /** Message thread: renders the Lab's test hit with the current settings and plays it. */
    void playTestHit();

    const lucky::CompressorEngine& getEngine() const noexcept { return engine; }
    juce::AudioProcessorValueTreeState& getParameters() noexcept { return parameters; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void run() override;
    void setControls (const lucky::Controls&);

    juce::AudioProcessorValueTreeState parameters;
    lucky::CompressorEngine engine;
    std::array<lucky::Controls, lucky::numModes> memory;
    /** Each mode's Colour (%), since each is its own classic's tone; 1× until changed. */
    std::array<float, lucky::numModes> colourMemory { 100.0f, 100.0f, 100.0f, 100.0f };
    std::atomic<float>* modeParameter = nullptr;
    std::atomic<float>* thresholdParameter = nullptr;
    std::atomic<float>* ratioParameter = nullptr;
    std::atomic<float>* attackParameter = nullptr;
    std::atomic<float>* releaseParameter = nullptr;
    std::atomic<float>* autoReleaseParameter = nullptr;
    std::atomic<float>* bypassParameter = nullptr;
    std::atomic<float>* matchParameter = nullptr;
    std::atomic<float>* mixParameter = nullptr;
    std::atomic<float>* mixModeParameter = nullptr;
    std::atomic<float>* blendParameter = nullptr;
    std::atomic<float>* colourParameter = nullptr;
    juce::SpinLock matchLock;
    std::atomic<bool> backgroundMatching { true };
    std::optional<lucky::DetectorSettings> matchedSettings;
    long long matchedSound = 0;
    lucky::Blend matchedBlend;
    bool rememberedMatch = false;
    int samplesSinceMatch = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuckyCompressorProcessor)
};

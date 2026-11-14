#pragma once

#include "Colour.h"
#include "Compressor.h"
#include "ExampleHit.h"
#include "LevelMatcher.h"
#include <array>
#include <atomic>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>

namespace lucky
{

/** Runs the compressor on the audio: make-up gain, what you hear (compressed or bypassed) and the meters the
    editor draws. Audio thread only, except for the getters and setMakeupTarget, which any thread may call. */
class CompressorEngine
{
public:
    /** Samples summarised by each meter entry. */
    static constexpr int meterBlockSize = 128;
    /** Meter entries kept: about 11 s at 48 kHz. */
    static constexpr int meterCapacity = 4096;
    static constexpr double listenFadeSeconds = 0.02;
    static constexpr double makeupRampSeconds = 0.08;

    /** One meter entry: peaks of the input and of the output before make-up (compressed and blended with the
        dry signal by Mix or Blend), and the deepest gain reduction, all in dB. */
    struct MeterEntry
    {
        float inputDb = -120.0f, outputDb = -120.0f, reductionDb = 0.0f;
    };

    void prepare (double sampleRate);
    void reset() noexcept;

    void setSettings (const DetectorSettings&) noexcept;
    void setBypass (bool) noexcept;
    void setMatch (bool) noexcept;

    /** How the compressed signal is combined with the dry one (Mix or Blend). */
    void setBlend (Blend) noexcept;

    /** The tone added to the compressed signal. */
    void setColour (const ColourSettings&) noexcept;

    /** Any thread: the make-up gain level match should ramp to (linear), or jump to at once (a restored session).
        It survives prepare and reset: it belongs to the settings and the music, not to the audio device. */
    void setMakeupTarget (float gain, bool jump = false) noexcept
    {
        makeupTarget.store (gain, std::memory_order_relaxed);
        if (jump)
            makeupJump.store (true, std::memory_order_release);
    }
    float getMakeupTarget() const noexcept { return makeupTarget.load (std::memory_order_relaxed); }

    /** Any thread: with level match on, the compressed signal's own make-up, applied before Mix or Blend so the
        two sides are about as loud as each other and the knob sounds even across its range (as on a parallel
        compressor with its own output gain). setMakeupTarget then trims the mix. Ramps and jumps the same way. */
    void setWetMakeupTarget (float gain, bool jump = false) noexcept
    {
        wetMakeupTarget.store (gain, std::memory_order_relaxed);
        if (jump)
            makeupJump.store (true, std::memory_order_release);
    }
    float getWetMakeupTarget() const noexcept { return wetMakeupTarget.load (std::memory_order_relaxed); }

    /** Any thread: with level match on, the gain for the uncompressed signal heard while comparing, below 1 when
        the compressed one couldn't be matched without peaking too high. Ramps and jumps like the make-up; at 1 the
        uncompressed signal is bit-identical to the input. */
    void setUncompressedTarget (float gain, bool jump = false) noexcept
    {
        uncompressedTarget.store (gain, std::memory_order_relaxed);
        if (jump)
            makeupJump.store (true, std::memory_order_release);
    }
    float getUncompressedTarget() const noexcept { return uncompressedTarget.load (std::memory_order_relaxed); }

    /** Message thread: plays a rendered test hit on top of the output, compressed or (while bypassed) raw. It
        never touches the input, so it doesn't affect your audio's compression, meters or level match. The hit is
        written to the slot that isn't playing, so pressing again mid-hit never changes what is being heard. */
    void playTestHit (const example_hit::HitRender&);

    /** The recent input that level match predicts from. */
    const LevelMatcher& getMatcher() const noexcept { return matcher; }

    /** Processes a mono or stereo buffer in place. */
    void process (juce::AudioBuffer<float>&) noexcept;

    /** Index one past the newest meter entry; entries before it (up to meterCapacity back) can be read. */
    int getMeterPosition() const noexcept { return meterPosition.load (std::memory_order_acquire); }

    /** A meter entry by absolute index (wraps). */
    MeterEntry getMeterEntry (int index) const noexcept;

    /** Latest gain reduction in dB. */
    float getReduction() const noexcept { return reductionNow.load (std::memory_order_relaxed); }

    /** The detector's current release in seconds (moves with the music for two-stage shapes). */
    float getReleaseSeconds() const noexcept { return releaseNow.load (std::memory_order_relaxed); }

    /** Make-up gain being applied by level match, in dB. */
    float getMakeupDb() const noexcept { return makeupDbNow.load (std::memory_order_relaxed); }

    /** True if non-silent audio has arrived in the last half second. */
    bool isHearingAudio() const noexcept { return hearingAudio.load (std::memory_order_relaxed); }

private:
    void pushMeter() noexcept;

    Compressor compressor;
    Colour colour;
    LevelMatcher matcher;
    DetectorSettings settings;
    double sampleRate = 48000.0;
    bool bypass = false, match = true;
    float rawWeight = 0.0f, weightStep = 0.001f;
    float makeup = 1.0f, makeupRate = 0.0f;
    Blend blend, blendTarget;
    float blendStep = 0.001f;
    std::atomic<float> makeupTarget { 1.0f }, wetMakeupTarget { 1.0f }, uncompressedTarget { 1.0f };
    float wetMakeup = 1.0f, uncompressed = 1.0f;
    std::atomic<bool> makeupJump { false };
    /** Two slots, so a new hit is written to the one not playing. hitLock guards the slots' contents, requestedSlot
        and playingSlot against playTestHit and prepare; the audio thread only try-locks it to pick up a request. */
    std::array<example_hit::HitRender, 2> hits;
    juce::SpinLock hitLock;
    int requestedSlot = -1;
    int playingSlot = 0;
    size_t hitPosition = 0;

    std::array<std::atomic<float>, meterCapacity> meterInput {}, meterOutput {}, meterReduction {};
    std::atomic<int> meterPosition { 0 };
    int meterFill = 0;
    float peakIn = 0.0f, peakOut = 0.0f, peakReduction = 0.0f;
    int quietSamples = 0;

    std::atomic<float> reductionNow { 0.0f }, releaseNow { 0.15f }, makeupDbNow { 0.0f };
    std::atomic<bool> hearingAudio { false };
};

} // namespace lucky

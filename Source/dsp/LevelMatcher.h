#pragma once

#include "Modes.h"
#include <atomic>
#include <lucky_dsp/lucky_dsp.h>
#include <memory>
#include <optional>

namespace lucky
{

/** Predicts the make-up gain that makes the compressed signal as loud as the input.

    The audio thread records the last few seconds of input (K-weighted energy and detector level). On request,
    any thread replays that history through a fresh detector with the given settings and compares the energies,
    so a settings change is matched in milliseconds instead of after seconds of listening. The result is used as a
    fixed gain until the settings change again, so it never undoes the compressor's own levelling.

    The history is played through the detector warmUpPasses times before the measured pass, so detectors with memory
    (Opto's release) are warmed up as they would be in the music. The compressed energy is estimated as the input's
    K-weighted energy times the gain squared, which holds as long as the gain moves slowly next to the filter; tests
    check it against a real render (within 0.5 dB). */
class LevelMatcher
{
public:
    static constexpr double historySeconds = 3.0;
    static constexpr double minimumSoundSeconds = 0.1;
    static constexpr float silenceDb = -70.0f;
    static constexpr float maxGainDb = 30.0f;
    /** Times the history is played through the detector before the measured pass, to warm up memory (Opto). */
    static constexpr int warmUpPasses = 2;
    /** Blend can make the output louder than the input, so matching may also turn down, this far at most. */
    static constexpr float minGainDb = -30.0f;
    /** Level match never pushes a peak above this (or above the input's own loudest peak, if that's higher). The
        attack lets the front of each hit through before it compresses, so a loudness match alone can put peaks
        far above the input's, worst on the first hit after a silence. */
    static constexpr float peakCeilingDb = -1.0f;

    void prepare (double sampleRate);
    void reset() noexcept;

    /** Audio thread: records one stereo frame of input. */
    void record (float left, float right) noexcept;

    /** Any thread: the gain that would make the recorded audio, compressed with these settings and combined with
        the dry signal by the blend, as loud as it is uncompressed (up or down); nothing while too little sound has been recorded. */
    std::optional<float> predict (const DetectorSettings&, Blend = {}) const;

    /** Level match with Mix or Blend: the compressed signal's own make-up (applied before it meets the dry signal),
        and the trim that then brings the combination back to the input's loudness. */
    struct Makeup
    {
        float wet = 1.0f, trim = 1.0f;
        /** For comparing: the uncompressed signal's gain. Below 1 only when the trim was held down to keep peaks
            under the ceiling, by as much as it was held down, so both are still as loud as each other. */
        float uncompressed = 1.0f;
    };

    /** Any thread: both make-up gains from a single replay of the history; nothing while too little sound has been
        recorded. The trim is turned down if the loudest peak would otherwise go above peakCeilingDb (the replay's
        first pass starts with the detector at rest, so this covers the first hit after a silence too), and the
        uncompressed signal is then turned down by the same amount. */
    std::optional<Makeup> predictMakeup (const DetectorSettings&, Blend) const;

    /** Samples of sound (not silence) recorded since the last reset, capped at the history length. */
    long long getSoundRecorded() const noexcept { return soundRecorded.load (std::memory_order_relaxed); }

    /** Samples the history holds. */
    int getHistoryLength() const noexcept { return capacity; }

    double getSampleRate() const noexcept { return sampleRate; }

private:
    /** The history's K-weighted energy, uncompressed and weighted by the compressor's gain g and g², from one replay,
        and its loudest sample before and after compression (the latter over every pass, from a detector at rest on). */
    struct Sums
    {
        double energy = 0.0, gain = 0.0, gainSquared = 0.0;
        float inputPeak = 0.0f, compressedPeak = 0.0f;

        /** Energy of dry + wet·g, relative to the uncompressed energy. */
        double ratio (Blend) const;
    };

    std::optional<Sums> replay (const DetectorSettings&) const;
    static float limit (double gain);

    double sampleRate = 48000.0;
    std::array<KWeighting, 2> filters {};
    std::unique_ptr<std::atomic<float>[]> levels, energies;
    int capacity = 0;
    float silentLevel = 3.0e-4f;
    std::atomic<long long> written { 0 }, soundRecorded { 0 };
};

} // namespace lucky

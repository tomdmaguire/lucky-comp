#pragma once

#include "Colour.h"
#include "Modes.h"
#include <vector>

namespace lucky
{

/** One drum hit run through the detector, for the Lab. */
struct HitTrace
{
    /** Mono input and output (before make-up, combined with the dry signal by Mix or Blend) of the shown window. */
    std::vector<float> input, output;
    /** The compressed (and coloured) signal alone, before it is combined with the dry one. */
    std::vector<float> wet;
    /** Gain reduction in dB for each sample of the window. */
    std::vector<float> reduction;
    double sampleRate = 48000.0;
    /** Time before the hit starts within the window. */
    double leadSeconds = 0.0;
    double windowSeconds = 0.4;
    /** The detector's release time when the hit's reduction lets go. */
    float releaseSeconds = 0.0f;
};

namespace example_hit
{
inline constexpr double sampleRate = 48000.0;
inline constexpr double leadSeconds = 0.006;
/** Like a backbeat at 120 bpm; longer than the snare, so its tail is never cut. */
inline constexpr double hitSpacingSeconds = 1.0;
inline constexpr int hitsBefore = 3;

/** The window shown in every mode: the whole hit plus a little silence, so the hit is the same size whichever mode
    is chosen and a slow release shows as a line still climbing at the right edge. */
inline constexpr double windowSeconds = 0.6;

/** Loudest the test hit may be once compressed and matched, so auditioning it can't clip. */
inline constexpr float hitCeiling = 0.89f;

/** Peak level of the test hit (about −6 dBFS): loud enough to reach the default thresholds, quiet enough to
    audition over a mix. */
inline constexpr float hitPeak = 0.5f;

/** A real snare (Resources/TestSnare.wav: an uncompressed DW 13" snare from freesound, CC0, with a small room added),
    at the given rate and peaking at hitPeak. The crack makes attack audible and the room makes release audible. */
std::vector<float> makeHit (double rate = sampleRate);

/** Runs the hit (after hitsBefore identical hits, so the detector is in a realistic state) through the settings. */
HitTrace simulate (const DetectorSettings&, double windowSeconds, double rate = sampleRate, Blend = {}, const ColourSettings& = {});

/** The test hit ready to play: as it is, and compressed exactly as the Lab shows it. With match, it is level
    matched the way your audio is: the compressed hit is brought to the raw hit's loudness before Mix or Blend, the
    result is trimmed to the raw hit's loudness, then turned down if needed to stay under hitCeiling. */
struct HitRender
{
    std::vector<float> raw, compressed;
};

HitRender renderHit (const DetectorSettings&, double rate, bool match, Blend = {}, const ColourSettings& = {});
} // namespace example_hit

} // namespace lucky

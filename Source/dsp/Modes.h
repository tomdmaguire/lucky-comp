#pragma once

#include <array>
#include <cmath>
#include <span>

namespace lucky
{

/** The four behaviours: a transparent compressor and three classic designs. */
enum class Mode : int
{
    clean = 0,
    fet,
    opto,
    vca,
};

inline constexpr int numModes = 4;

/** How the gain reduction lets go. */
enum class ReleaseShape
{
    fixed,
    /** Two-stage, slow stage set by recent compression (Opto). */
    opto,
    /** Two-stage, faster cousin of opto (VCA Auto). */
    automatic,
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"

/** The values the user sets, in real units, before a mode constrains them. */
struct Controls
{
    float thresholdDb = -24.0f;
    float ratio = 4.0f;
    float attackMs = 10.0f;
    float releaseMs = 150.0f;
    bool autoRelease = false;

    bool operator== (const Controls&) const = default;
};

/** What the detector actually runs with. */
struct DetectorSettings
{
    float thresholdDb = -24.0f;
    float ratio = 4.0f;
    float kneeDb = 6.0f;
    float attackMs = 10.0f;
    float releaseMs = 150.0f;
    ReleaseShape release = ReleaseShape::fixed;

    bool operator== (const DetectorSettings&) const = default;
};

/** How the compressed signal is combined with the dry one: output = in × (dry + wet × gain). */
struct Blend
{
    float dry = 0.0f, wet = 1.0f;

    bool operator== (const Blend&) const = default;

    /** Mix: a straight crossfade, 0 to 100% compressed. Linear, as dry and compressed are in phase. */
    static Blend mix (float percent) { return { 1.0f - percent / 100.0f, percent / 100.0f }; }

    /** Blend: dry stays at 0 dB and the compressed signal is added at a level (silent at or below silentDb). */
    static Blend added (float db, float silentDb) { return { 1.0f, db <= silentDb ? 0.0f : std::pow (10.0f, db / 20.0f) }; }
};

#pragma clang diagnostic pop

namespace modes
{
inline constexpr float minThresholdDb = -60.0f, maxThresholdDb = 0.0f;
inline constexpr float minRatio = 1.0f, maxRatio = 20.0f;
inline constexpr float minAttackMs = 0.01f, maxAttackMs = 100.0f;
inline constexpr float minReleaseMs = 5.0f, maxReleaseMs = 5000.0f;

inline constexpr float classicMinThresholdDb = -40.0f;
inline constexpr float cleanMaxReleaseMs = maxReleaseMs;

inline constexpr float fetFastestAttackMs = 0.02f, fetSlowestAttackMs = 0.8f;
inline constexpr float fetFastestReleaseMs = 50.0f, fetSlowestReleaseMs = 1100.0f;
inline constexpr std::array<float, 4> fetRatios { 4.0f, 8.0f, 12.0f, 20.0f };

/** The LA-2A's published figures: about 3:1 in Compress, 10 ms attack. */
inline constexpr float optoRatio = 3.0f, optoAttackMs = 10.0f;
/** Peak Reduction 0-100 maps onto a threshold of 0 to -40 dB. */
inline constexpr float optoDbPerPeakReduction = 0.4f;

inline constexpr std::array<float, 3> vcaRatios { 2.0f, 4.0f, 10.0f };
inline constexpr std::array<float, 6> vcaAttacksMs { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f };
inline constexpr std::array<float, 4> vcaReleasesMs { 100.0f, 300.0f, 600.0f, 1200.0f };

/** A mode's display name, e.g. "Clean". */
const char* name (Mode);

/** The values a mode starts with on first visit. */
Controls defaults (Mode);

/** The detector settings a mode runs for the given controls: clamped, snapped and with the mode's knee and release. */
DetectorSettings resolve (Mode, const Controls&);

/** The controls as the mode allows them, e.g. FET attack clamped to its range and VCA attack snapped to a step. */
Controls constrain (Mode, const Controls&);

/** FET dial (1-7, 7 fastest) to attack in ms, logarithmic like the hardware's scale. */
float fetAttackMs (float dial);

/** FET dial (1-7, 7 fastest) to release in ms. */
float fetReleaseMs (float dial);

/** Attack in ms to the FET dial that gives it (clamped to 1-7). */
float fetAttackDial (float ms);

/** Release in ms to the FET dial that gives it (clamped to 1-7). */
float fetReleaseDial (float ms);

/** The step nearest to the value on a log scale. */
float snapLog (float value, std::span<const float> steps);
} // namespace modes

} // namespace lucky

#include "Modes.h"
#include <algorithm>
#include <cmath>

namespace lucky::modes
{
namespace
{
float logInterpolate (float from, float to, float amount)
{
    return from * std::pow (to / from, amount);
}

float logPosition (float from, float to, float value)
{
    return std::log (value / from) / std::log (to / from);
}
} // namespace

const char* name (Mode mode)
{
    switch (mode)
    {
        case Mode::clean: return "Clean";
        case Mode::fet: return "FET";
        case Mode::opto: return "Opto";
        case Mode::vca: return "VCA";
    }
    return "Clean";
}

Controls defaults (Mode mode)
{
    switch (mode)
    {
        case Mode::clean: return { maxThresholdDb, 4.0f, 10.0f, 150.0f, false };
        case Mode::fet: return { maxThresholdDb, 4.0f, fetAttackMs (3.0f), fetReleaseMs (5.0f), false };
        case Mode::opto: return { maxThresholdDb, optoRatio, optoAttackMs, 500.0f, false };
        case Mode::vca: return { maxThresholdDb, 4.0f, 10.0f, 300.0f, false };
    }
    return {};
}

float fetAttackMs (float dial)
{
    return logInterpolate (fetSlowestAttackMs, fetFastestAttackMs, (std::clamp (dial, 1.0f, 7.0f) - 1.0f) / 6.0f);
}

float fetReleaseMs (float dial)
{
    return logInterpolate (fetSlowestReleaseMs, fetFastestReleaseMs, (std::clamp (dial, 1.0f, 7.0f) - 1.0f) / 6.0f);
}

float fetAttackDial (float ms)
{
    return std::clamp (1.0f + 6.0f * logPosition (fetSlowestAttackMs, fetFastestAttackMs, ms), 1.0f, 7.0f);
}

float fetReleaseDial (float ms)
{
    return std::clamp (1.0f + 6.0f * logPosition (fetSlowestReleaseMs, fetFastestReleaseMs, ms), 1.0f, 7.0f);
}

float snapLog (float value, std::span<const float> steps)
{
    const auto target = std::log (std::max (value, 1.0e-6f));
    return *std::min_element (steps.begin(), steps.end(),
                              [target] (float a, float b) { return std::abs (std::log (a) - target) < std::abs (std::log (b) - target); });
}

Controls constrain (Mode mode, const Controls& controls)
{
    auto c = controls;
    c.thresholdDb = std::clamp (c.thresholdDb, minThresholdDb, maxThresholdDb);
    c.ratio = std::clamp (c.ratio, minRatio, maxRatio);
    c.attackMs = std::clamp (c.attackMs, minAttackMs, maxAttackMs);
    c.releaseMs = std::clamp (c.releaseMs, minReleaseMs, maxReleaseMs);

    switch (mode)
    {
        case Mode::clean:
            c.releaseMs = std::min (c.releaseMs, cleanMaxReleaseMs);
            c.autoRelease = false;
            break;
        case Mode::fet:
            c.thresholdDb = std::max (c.thresholdDb, classicMinThresholdDb);
            c.ratio = snapLog (c.ratio, fetRatios);
            c.attackMs = std::clamp (c.attackMs, fetFastestAttackMs, fetSlowestAttackMs);
            c.releaseMs = std::clamp (c.releaseMs, fetFastestReleaseMs, fetSlowestReleaseMs);
            c.autoRelease = false;
            break;
        case Mode::opto:
            c.thresholdDb = std::max (c.thresholdDb, classicMinThresholdDb);
            c.ratio = optoRatio;
            c.attackMs = optoAttackMs;
            c.autoRelease = false;
            break;
        case Mode::vca:
            c.thresholdDb = std::max (c.thresholdDb, classicMinThresholdDb);
            c.ratio = snapLog (c.ratio, vcaRatios);
            c.attackMs = snapLog (c.attackMs, vcaAttacksMs);
            c.releaseMs = snapLog (c.releaseMs, vcaReleasesMs);
            break;
    }
    return c;
}

DetectorSettings resolve (Mode mode, const Controls& controls)
{
    const auto c = constrain (mode, controls);
    DetectorSettings settings { c.thresholdDb, c.ratio, 6.0f, c.attackMs, c.releaseMs, ReleaseShape::fixed };

    switch (mode)
    {
        case Mode::clean: settings.kneeDb = 6.0f; break;
        case Mode::fet: settings.kneeDb = 2.0f; break;
        case Mode::opto:
            settings.kneeDb = 14.0f;
            settings.release = ReleaseShape::opto;
            break;
        case Mode::vca:
            settings.kneeDb = 3.0f;
            if (c.autoRelease)
                settings.release = ReleaseShape::automatic;
            break;
    }
    return settings;
}

} // namespace lucky::modes

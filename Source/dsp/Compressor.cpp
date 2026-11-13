#include "Compressor.h"
#include <algorithm>
#include <cmath>

namespace lucky
{
namespace
{
constexpr int slowUpdateInterval = 32;
constexpr float silenceDb = -120.0f;
} // namespace

void Compressor::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    setSettings (settings);
    reset();
}

void Compressor::reset() noexcept
{
    fast = slow = memory = 0.0f;
    samplesUntilUpdate = 0;
    updateSlowStage();
}

float Compressor::coefficient (float milliseconds) const noexcept
{
    return static_cast<float> (std::exp (-1.0 / (std::max (milliseconds, 0.001f) * 0.001 * sampleRate)));
}

float Compressor::fastStageMs (ReleaseShape shape) noexcept
{
    return shape == ReleaseShape::opto ? 60.0f : 100.0f;
}

void Compressor::setSettings (const DetectorSettings& newSettings) noexcept
{
    settings = newSettings;
    attackCoefficient = coefficient (settings.attackMs);
    releaseCoefficient = coefficient (settings.releaseMs);
    fastCoefficient = coefficient (fastStageMs (settings.release));
    const auto memorySeconds = settings.release == ReleaseShape::opto ? 3.0 : 1.5;
    memoryRate = static_cast<float> (1.0 - std::exp (-1.0 / (memorySeconds * sampleRate)));
    updateSlowStage();
}

void Compressor::updateSlowStage() noexcept
{
    slowSeconds = settings.release == ReleaseShape::opto ? std::min (5.0f, 0.4f + 0.3f * memory)
                                                          : std::min (1.5f, 0.2f + 0.1f * memory);
    slowCoefficient = coefficient (slowSeconds * 1000.0f);
}

float Compressor::staticReduction (float levelDb) const noexcept
{
    const auto over = levelDb - settings.thresholdDb;
    const auto slope = 1.0f - 1.0f / settings.ratio;
    const auto knee = settings.kneeDb;

    if (2.0f * over <= -knee)
        return 0.0f;
    if (2.0f * over >= knee)
        return over * slope;

    const auto into = over + knee * 0.5f;
    return slope * into * into / (2.0f * knee);
}

float Compressor::process (float absoluteLevel) noexcept
{
    const auto levelDb = absoluteLevel > 1.0e-6f ? 20.0f * std::log10 (absoluteLevel) : silenceDb;
    const auto target = staticReduction (levelDb);

    if (settings.release == ReleaseShape::fixed)
    {
        fast = target + (target > fast ? attackCoefficient : releaseCoefficient) * (fast - target);
        return fast;
    }

    if (--samplesUntilUpdate <= 0)
    {
        samplesUntilUpdate = slowUpdateInterval;
        updateSlowStage();
    }

    fast = target + (target > fast ? attackCoefficient : fastCoefficient) * (fast - target);
    slow = target + (target > slow ? attackCoefficient : slowCoefficient) * (slow - target);
    const auto reduction = 0.5f * (fast + slow);
    memory += (reduction - memory) * memoryRate;
    return reduction;
}

float Compressor::getReleaseSeconds() const noexcept
{
    return settings.release == ReleaseShape::fixed ? settings.releaseMs * 0.001f : slowSeconds;
}

} // namespace lucky

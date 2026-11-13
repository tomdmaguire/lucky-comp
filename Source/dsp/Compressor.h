#pragma once

#include "Modes.h"

namespace lucky
{

/** The detector and gain computer: turns a level into gain reduction, smoothed the way the mode behaves.
    Holds no audio; the engine and the Lab's simulation both drive it one sample at a time. */
class Compressor
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void setSettings (const DetectorSettings&) noexcept;
    const DetectorSettings& getSettings() const noexcept { return settings; }

    /** Takes the larger absolute sample of the channels and returns the gain reduction in dB (0 or more). */
    float process (float absoluteLevel) noexcept;

    /** Gain reduction the gain computer asks for at a steady level, before any smoothing. */
    float staticReduction (float levelDb) const noexcept;

    /** The slow stage's current release time in seconds for two-stage shapes, else the fixed release. */
    float getReleaseSeconds() const noexcept;

    /** Release of the fast stage: about half the reduction lets go this quickly. */
    static float fastStageMs (ReleaseShape) noexcept;

private:
    void updateSlowStage() noexcept;
    float coefficient (float milliseconds) const noexcept;

    DetectorSettings settings;
    double sampleRate = 48000.0;
    float attackCoefficient = 0.0f, releaseCoefficient = 0.0f, fastCoefficient = 0.0f, slowCoefficient = 0.0f;
    float memoryRate = 0.0f;
    float fast = 0.0f, slow = 0.0f, memory = 0.0f, slowSeconds = 0.4f;
    int samplesUntilUpdate = 0;
};

} // namespace lucky

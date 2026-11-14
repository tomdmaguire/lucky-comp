#include "CompressorEngine.h"
#include <algorithm>
#include <cmath>

namespace lucky
{
namespace
{
constexpr float silentLevel = 3.0e-4f; // about -70 dBFS

float toDb (float gain) noexcept
{
    return gain > 1.0e-6f ? 20.0f * std::log10 (gain) : -120.0f;
}

/** A NaN or infinite sample from upstream would stick in every filter and envelope; it is heard as silence instead. */
float finiteOrSilent (float sample) noexcept
{
    return std::isfinite (sample) ? sample : 0.0f;
}

float approach (float value, float target, float step) noexcept
{
    return value < target ? std::min (target, value + step) : std::max (target, value - step);
}
} // namespace

void CompressorEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    compressor.prepare (sampleRate);
    compressor.setSettings (settings);
    colour.prepare (sampleRate);
    matcher.prepare (sampleRate);
    const auto hitLength = static_cast<size_t> (example_hit::hitSpacingSeconds * sampleRate);
    {
        const juce::SpinLock::ScopedLockType lock (hitLock);
        for (auto& k : hits)
        {
            k.raw.assign (hitLength, 0.0f);
            k.compressed.assign (hitLength, 0.0f);
        }
        requestedSlot = -1;
    }
    hitPosition = hitLength;
    weightStep = blendStep = static_cast<float> (1.0 / (listenFadeSeconds * sampleRate));
    makeupRate = static_cast<float> (1.0 - std::exp (-1.0 / (makeupRampSeconds * sampleRate)));
    reset();
}

void CompressorEngine::reset() noexcept
{
    compressor.reset();
    colour.reset();
    matcher.reset();
    makeup = match ? makeupTarget.load (std::memory_order_relaxed) : 1.0f;
    wetMakeup = match ? wetMakeupTarget.load (std::memory_order_relaxed) : 1.0f;
    uncompressed = match ? uncompressedTarget.load (std::memory_order_relaxed) : 1.0f;
    rawWeight = bypass ? 1.0f : 0.0f;
    blend = blendTarget;
    meterFill = 0;
    peakIn = peakOut = peakReduction = 0.0f;
    quietSamples = 0;
}

void CompressorEngine::setSettings (const DetectorSettings& newSettings) noexcept
{
    if (newSettings == settings)
        return;

    settings = newSettings;
    compressor.setSettings (settings);
}

void CompressorEngine::setBypass (bool shouldBypass) noexcept { bypass = shouldBypass; }
void CompressorEngine::setMatch (bool shouldMatch) noexcept { match = shouldMatch; }
void CompressorEngine::setBlend (Blend newBlend) noexcept { blendTarget = newBlend; }
void CompressorEngine::setColour (const ColourSettings& newColour) noexcept { colour.setSettings (newColour); }

void CompressorEngine::playTestHit (const example_hit::HitRender& render)
{
    const juce::SpinLock::ScopedLockType lock (hitLock);
    const auto slot = 1 - playingSlot;
    auto& target = hits[static_cast<size_t> (slot)];
    const auto length = std::min ({ target.raw.size(), render.raw.size(), render.compressed.size() });
    std::copy_n (render.raw.begin(), length, target.raw.begin());
    std::copy_n (render.compressed.begin(), length, target.compressed.begin());
    std::fill (target.raw.begin() + static_cast<std::ptrdiff_t> (length), target.raw.end(), 0.0f);
    std::fill (target.compressed.begin() + static_cast<std::ptrdiff_t> (length), target.compressed.end(), 0.0f);
    requestedSlot = slot;
}

CompressorEngine::MeterEntry CompressorEngine::getMeterEntry (int index) const noexcept
{
    const auto i = static_cast<size_t> (((index % meterCapacity) + meterCapacity) % meterCapacity);
    return { meterInput[i].load (std::memory_order_relaxed), meterOutput[i].load (std::memory_order_relaxed),
             meterReduction[i].load (std::memory_order_relaxed) };
}

void CompressorEngine::pushMeter() noexcept
{
    const auto position = meterPosition.load (std::memory_order_relaxed);
    const auto i = static_cast<size_t> (position % meterCapacity);
    meterInput[i].store (toDb (peakIn), std::memory_order_relaxed);
    meterOutput[i].store (toDb (peakOut), std::memory_order_relaxed);
    meterReduction[i].store (peakReduction, std::memory_order_relaxed);
    meterPosition.store (position + 1, std::memory_order_release);
    meterFill = 0;
    peakIn = peakOut = peakReduction = 0.0f;
}

void CompressorEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    if (numChannels == 0)
        return;

    auto* left = buffer.getWritePointer (0);
    auto* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    const auto rawTarget = bypass ? 1.0f : 0.0f;
    const auto makeupGoal = match ? makeupTarget.load (std::memory_order_relaxed) : 1.0f;
    const auto wetMakeupGoal = match ? wetMakeupTarget.load (std::memory_order_relaxed) : 1.0f;
    const auto uncompressedGoal = match ? uncompressedTarget.load (std::memory_order_relaxed) : 1.0f;
    if (makeupJump.exchange (false, std::memory_order_acquire))
    {
        makeup = makeupGoal;
        wetMakeup = wetMakeupGoal;
        uncompressed = uncompressedGoal;
    }
    if (const juce::SpinLock::ScopedTryLockType lock (hitLock); lock.isLocked() && requestedSlot >= 0)
    {
        playingSlot = std::exchange (requestedSlot, -1);
        hitPosition = 0;
    }
    const auto& hit = hits[static_cast<size_t> (playingSlot)];
    float reduction = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto inL = finiteOrSilent (left[i]);
        const auto inR = right != nullptr ? finiteOrSilent (right[i]) : inL;
        matcher.record (inL, inR);
        const auto level = std::max (std::abs (inL), std::abs (inR));

        reduction = compressor.process (level);
        blend.dry = approach (blend.dry, blendTarget.dry, blendStep);
        blend.wet = approach (blend.wet, blendTarget.wet, blendStep * 2.0f);
        const auto gain = std::pow (10.0f, -reduction * 0.05f);
        const auto wetL = colour.process (inL * gain, reduction, 0), wetR = colour.process (inR * gain, reduction, 1);
        makeup += (makeupGoal - makeup) * makeupRate;
        wetMakeup += (wetMakeupGoal - wetMakeup) * makeupRate;
        const auto wetGain = blend.wet * wetMakeup;
        const auto compL = blend.dry * inL + wetGain * wetL, compR = blend.dry * inR + wetGain * wetR;

        uncompressed += (uncompressedGoal - uncompressed) * makeupRate;
        if (std::abs (uncompressedGoal - uncompressed) < 1.0e-6f)
            uncompressed = uncompressedGoal;
        rawWeight = approach (rawWeight, rawTarget, weightStep);
        const auto rawGain = rawWeight * uncompressed;
        const auto compressedWeight = (1.0f - rawWeight) * makeup;
        auto hitSample = 0.0f;
        if (hitPosition < hit.raw.size())
        {
            hitSample = rawWeight * hit.raw[hitPosition] + (1.0f - rawWeight) * hit.compressed[hitPosition];
            ++hitPosition;
        }

        left[i] = rawGain * inL + compressedWeight * compL + hitSample;
        if (right != nullptr)
            right[i] = rawGain * inR + compressedWeight * compR + hitSample;

        peakIn = std::max (peakIn, level);
        peakOut = std::max (peakOut, std::max (std::abs (blend.dry * inL + blend.wet * wetL), std::abs (blend.dry * inR + blend.wet * wetR)));
        peakReduction = std::max (peakReduction, reduction);
        if (++meterFill == meterBlockSize)
            pushMeter();

        quietSamples = level > silentLevel ? 0 : quietSamples + 1;
    }

    reductionNow.store (reduction, std::memory_order_relaxed);
    releaseNow.store (compressor.getReleaseSeconds(), std::memory_order_relaxed);
    makeupDbNow.store (toDb (makeup), std::memory_order_relaxed);
    hearingAudio.store (quietSamples < static_cast<int> (sampleRate * 0.5), std::memory_order_relaxed);
}

} // namespace lucky

#include "LevelMatcher.h"
#include "Compressor.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace lucky
{

void LevelMatcher::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    for (auto& filter : filters)
        filter.prepare (sampleRate);

    capacity = std::max (1, static_cast<int> (std::lround (historySeconds * sampleRate)));
    levels = std::make_unique<std::atomic<float>[]> (static_cast<size_t> (capacity));
    energies = std::make_unique<std::atomic<float>[]> (static_cast<size_t> (capacity));
    silentLevel = std::pow (10.0f, silenceDb / 20.0f);
    reset();
}

void LevelMatcher::reset() noexcept
{
    for (auto& filter : filters)
        filter.reset();
    written.store (0, std::memory_order_relaxed);
    soundRecorded.store (0, std::memory_order_relaxed);
}

void LevelMatcher::record (float left, float right) noexcept
{
    if (capacity == 0)
        return;

    const auto l = filters[0].process (left), r = filters[1].process (right);
    const auto position = written.load (std::memory_order_relaxed);
    const auto index = static_cast<size_t> (position % capacity);
    const auto level = std::max (std::abs (left), std::abs (right));
    levels[index].store (level, std::memory_order_relaxed);
    energies[index].store (static_cast<float> (l * l + r * r), std::memory_order_relaxed);
    written.store (position + 1, std::memory_order_release);

    if (level > silentLevel)
    {
        const auto sound = soundRecorded.load (std::memory_order_relaxed);
        if (sound < capacity)
            soundRecorded.store (sound + 1, std::memory_order_relaxed);
    }
}

std::optional<LevelMatcher::Sums> LevelMatcher::replay (const DetectorSettings& settings) const
{
    if (getSoundRecorded() < static_cast<long long> (minimumSoundSeconds * sampleRate))
        return std::nullopt;

    const auto end = written.load (std::memory_order_acquire);
    const auto length = std::min (end, static_cast<long long> (capacity));

    Compressor compressor;
    compressor.prepare (sampleRate);
    compressor.setSettings (settings);

    const auto toGain = [] (float reduction) { return std::exp (-static_cast<double> (reduction) * (std::numbers::ln10 / 20.0)); };

    Sums sums;
    for (int pass = 0; pass < warmUpPasses; ++pass)
        for (auto position = end - length; position < end; ++position)
        {
            const auto level = levels[static_cast<size_t> (position % capacity)].load (std::memory_order_relaxed);
            const auto reduction = compressor.process (level);
            if (level > sums.compressedPeak)
                sums.compressedPeak = std::max (sums.compressedPeak, static_cast<float> (level * toGain (reduction)));
        }

    for (auto position = end - length; position < end; ++position)
    {
        const auto index = static_cast<size_t> (position % capacity);
        const auto level = levels[index].load (std::memory_order_relaxed);
        const auto reduction = compressor.process (level);
        const auto energy = static_cast<double> (energies[index].load (std::memory_order_relaxed));
        const auto gain = toGain (reduction);
        sums.energy += energy;
        sums.gain += energy * gain;
        sums.gainSquared += energy * gain * gain;
        sums.inputPeak = std::max (sums.inputPeak, level);
        sums.compressedPeak = std::max (sums.compressedPeak, static_cast<float> (level * gain));
    }

    const auto silentEnergy = std::pow (10.0, silenceDb / 10.0) * 2.0 * static_cast<double> (length);
    if (sums.energy < silentEnergy)
        return std::nullopt;
    return sums;
}

double LevelMatcher::Sums::ratio (Blend blend) const
{
    const auto dry = static_cast<double> (blend.dry), wet = static_cast<double> (blend.wet);
    return (dry * dry * energy + 2.0 * dry * wet * gain + wet * wet * gainSquared) / energy;
}

float LevelMatcher::limit (double gain)
{
    return std::clamp (static_cast<float> (gain), std::pow (10.0f, minGainDb / 20.0f), std::pow (10.0f, maxGainDb / 20.0f));
}

std::optional<float> LevelMatcher::predict (const DetectorSettings& settings, Blend blend) const
{
    const auto sums = replay (settings);
    if (! sums)
        return std::nullopt;
    const auto ratio = sums->ratio (blend);
    if (ratio <= 0.0)
        return std::nullopt;
    return limit (std::sqrt (1.0 / ratio));
}

std::optional<LevelMatcher::Makeup> LevelMatcher::predictMakeup (const DetectorSettings& settings, Blend blend) const
{
    const auto sums = replay (settings);
    if (! sums || sums->gainSquared <= 0.0)
        return std::nullopt;
    const auto wet = limit (std::sqrt (sums->energy / sums->gainSquared));
    const auto ratio = sums->ratio ({ blend.dry, blend.wet * wet });
    if (ratio <= 0.0)
        return std::nullopt;

    const auto trim = limit (std::sqrt (1.0 / ratio));
    const auto ceiling = std::max (std::pow (10.0, peakCeilingDb / 20.0), static_cast<double> (sums->inputPeak));
    const auto peak = static_cast<double> (blend.dry) * sums->inputPeak + static_cast<double> (blend.wet) * wet * sums->compressedPeak;
    if (trim * peak <= ceiling)
        return Makeup { wet, trim, 1.0f };

    const auto held = limit (ceiling / peak);
    return Makeup { wet, held, limit (held / trim) };
}

} // namespace lucky

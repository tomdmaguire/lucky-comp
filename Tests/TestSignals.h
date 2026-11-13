#pragma once

#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <numbers>

namespace lucky::test
{

/** A two-bar drum loop at 96 BPM with kicks, snares, ghost notes and hats, peaking at -1 dBFS. */
inline juce::AudioBuffer<float> drumLoop (double sampleRate, int repeats = 1)
{
    const auto step = 60.0 / 96.0 / 4.0;
    const auto loopLength = static_cast<int> (step * 32.0 * sampleRate);
    juce::AudioBuffer<float> loop (2, loopLength);
    loop.clear();
    juce::Random random (7);

    const auto add = [&] (double time, double seconds, float gain, float pan, auto&& voice)
    {
        const auto start = static_cast<int> (time * sampleRate);
        const auto length = static_cast<int> (seconds * sampleRate);
        for (int i = 0; i < length && start + i < loopLength; ++i)
        {
            const auto v = voice (static_cast<double> (i) / sampleRate) * gain;
            loop.addSample (0, start + i, v * (1.0f - pan));
            loop.addSample (1, start + i, v * (1.0f + pan));
        }
    };
    const auto noise = [&] { return random.nextFloat() * 2.0f - 1.0f; };

    for (const auto s : { 0, 7, 10, 16, 26, 27 })
    {
        auto phase = 0.0;
        add (s * step, 0.6, 1.0f, 0.0f, [&] (double t)
             {
                 phase += 2.0 * std::numbers::pi * (48.0 + 120.0 * std::exp (-t * 28.0)) / sampleRate;
                 return static_cast<float> (std::sin (phase) * std::exp (-t * 7.0)) + (t < 0.004 ? noise() * 0.6f * static_cast<float> (1.0 - t / 0.004) : 0.0f);
             });
    }

    for (const auto [s, gain] : { std::pair { 4, 0.85f }, { 12, 0.85f }, { 20, 0.85f }, { 28, 0.85f }, { 6, 0.22f }, { 15, 0.18f }, { 23, 0.2f }, { 31, 0.28f } })
    {
        auto phase = 0.0;
        auto low = 0.0f;
        add (s * step, 0.45, gain, 0.05f, [&] (double t)
             {
                 phase += 2.0 * std::numbers::pi * 190.0 / sampleRate;
                 const auto n = noise();
                 low += (n - low) * 0.3f;
                 return static_cast<float> (0.55 * std::sin (phase) * std::exp (-t * 25.0) + 0.9 * (n - low) * std::exp (-t * 13.0));
             });
    }

    for (int s = 0; s < 32; s += 2)
    {
        auto low = 0.0f;
        add (s * step, 0.12, s % 4 == 0 ? 0.26f : 0.16f, -0.25f, [&] (double t)
             {
                 const auto n = noise();
                 low += (n - low) * 0.6f;
                 return static_cast<float> ((n - low) * std::exp (-t * 55.0));
             });
    }

    loop.applyGain (0.89f / loop.getMagnitude (0, loopLength));

    juce::AudioBuffer<float> result (2, loopLength * repeats);
    for (int r = 0; r < repeats; ++r)
        for (int ch = 0; ch < 2; ++ch)
            result.copyFrom (ch, r * loopLength, loop, ch, 0, loopLength);
    return result;
}

/** A constant-amplitude stereo sine. */
inline juce::AudioBuffer<float> sine (double sampleRate, double seconds, float amplitude, double hz = 1000.0)
{
    juce::AudioBuffer<float> buffer (2, static_cast<int> (seconds * sampleRate));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto v = amplitude * static_cast<float> (std::sin (2.0 * std::numbers::pi * hz * i / sampleRate));
        buffer.setSample (0, i, v);
        buffer.setSample (1, i, v);
    }
    return buffer;
}

/** Mean-square energy of a range of a buffer, both channels. */
inline double energy (const juce::AudioBuffer<float>& buffer, int start, int length)
{
    auto sum = 0.0;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = start; i < start + length; ++i)
            sum += static_cast<double> (buffer.getSample (ch, i)) * buffer.getSample (ch, i);
    return sum / (length * buffer.getNumChannels());
}

} // namespace lucky::test

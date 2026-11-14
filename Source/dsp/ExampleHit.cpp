#include "ExampleHit.h"
#include "Compressor.h"
#include "LuckyCompSamples.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <numbers>
#include <string_view>

namespace lucky::example_hit
{
namespace
{
struct Sound
{
    std::vector<float> samples;
    double rate = 0.0;
};

/** The Lab draws at 48 kHz while S plays at the host's rate, so at least two rates stay resampled. */
constexpr size_t maxCachedRates = 4;

/** Reads a mono PCM WAV (16 or 24 bit) from memory; empty if the format isn't one of those. */
Sound decodeWav (const char* data, size_t size)
{
    const auto read16 = [data] (size_t at) { return static_cast<uint32_t> (static_cast<uint8_t> (data[at]) | static_cast<uint8_t> (data[at + 1]) << 8); };
    const auto read32 = [&] (size_t at) { return read16 (at) | read16 (at + 2) << 16; };

    Sound sound;
    if (size < 12 || std::string_view (data, 4) != "RIFF" || std::string_view (data + 8, 4) != "WAVE")
        return sound;

    auto channels = 0u, bits = 0u;
    for (size_t at = 12; at + 8 <= size;)
    {
        const std::string_view id (data + at, 4);
        const auto length = read32 (at + 4);
        const auto body = at + 8;
        if (id == "fmt " && length >= 16 && read16 (body) == 1)
        {
            channels = read16 (body + 2);
            sound.rate = read32 (body + 4);
            bits = read16 (body + 14);
        }
        else if (id == "data" && channels == 1 && (bits == 16 || bits == 24))
        {
            const auto bytes = bits / 8;
            const auto count = std::min<size_t> (length, size - body) / bytes;
            sound.samples.resize (count);
            for (size_t i = 0; i < count; ++i)
            {
                const auto sample = body + i * bytes;
                const auto top = bits == 16 ? read16 (sample) << 16 : (read16 (sample + 1) << 16) | static_cast<uint32_t> (static_cast<uint8_t> (data[sample])) << 8;
                const auto value = static_cast<int32_t> (top);
                sound.samples[i] = static_cast<float> (value / 2147483648.0);
            }
            return sound;
        }
        at = body + length + (length & 1);
    }
    return {};
}

/** Band-limited resampling with a Blackman-windowed sinc, filtering below the lower of the two Nyquists. */
std::vector<float> resample (const std::vector<float>& in, double from, double to)
{
    if (std::abs (from - to) < 1.0e-6)
        return in;

    constexpr auto zeroCrossings = 16;
    const auto step = from / to;
    const auto cutoff = std::min (1.0, to / from) * 0.97;
    const auto halfWidth = static_cast<int> (std::ceil (zeroCrossings / cutoff));
    std::vector<float> out (static_cast<size_t> (std::ceil (static_cast<double> (in.size()) / step)));

    for (size_t m = 0; m < out.size(); ++m)
    {
        const auto t = static_cast<double> (m) * step;
        const auto centre = static_cast<int> (std::floor (t));
        auto sum = 0.0;
        for (auto k = centre - halfWidth + 1; k <= centre + halfWidth; ++k)
        {
            if (k < 0 || k >= static_cast<int> (in.size()))
                continue;
            const auto x = (t - k) * cutoff;
            const auto sinc = std::abs (x) < 1.0e-9 ? 1.0 : std::sin (std::numbers::pi * x) / (std::numbers::pi * x);
            const auto w = 0.5 + 0.5 * (t - k) / halfWidth;
            const auto window = 0.42 - 0.5 * std::cos (2.0 * std::numbers::pi * w) + 0.08 * std::cos (4.0 * std::numbers::pi * w);
            sum += in[static_cast<size_t> (k)] * cutoff * sinc * window;
        }
        out[m] = static_cast<float> (sum);
    }
    return out;
}
} // namespace

std::vector<float> makeHit (double rate)
{
    static std::mutex mutex;
    static const auto source = decodeWav (LuckyCompSamples::TestSnare_wav, static_cast<size_t> (LuckyCompSamples::TestSnare_wavSize));
    static std::vector<Sound> cached;
    const std::scoped_lock lock (mutex);

    const auto found = std::find_if (cached.begin(), cached.end(), [rate] (const Sound& sound) { return std::abs (sound.rate - rate) < 1.0e-6; });
    if (found != cached.end())
        return found->samples;

    Sound sound { resample (source.samples, source.rate, rate), rate };
    auto peak = 0.0f;
    for (const auto s : sound.samples)
        peak = std::max (peak, std::abs (s));
    if (peak > 0.0f)
        for (auto& s : sound.samples)
            s *= hitPeak / peak;

    if (cached.size() >= maxCachedRates)
        cached.erase (cached.begin());
    cached.push_back (std::move (sound));
    return cached.back().samples;
}

HitTrace simulate (const DetectorSettings& settings, double window, double rate, Blend blend, const ColourSettings& colourSettings)
{
    const auto hit = makeHit (rate);
    const auto spacing = static_cast<int> (hitSpacingSeconds * rate);
    const auto lead = static_cast<int> (leadSeconds * rate);
    const auto shown = static_cast<int> (window * rate);
    const auto start = hitsBefore * spacing - lead;
    const auto total = start + shown;

    const auto sampleAt = [&] (int i)
    {
        if (i / spacing > hitsBefore)
            return 0.0f;
        const auto inHit = i % spacing;
        return inHit < static_cast<int> (hit.size()) ? hit[static_cast<size_t> (inHit)] : 0.0f;
    };

    Compressor compressor;
    compressor.prepare (rate);
    compressor.setSettings (settings);
    Colour colour;
    colour.prepare (rate);
    colour.setSettings (colourSettings);

    HitTrace trace;
    trace.sampleRate = rate;
    trace.leadSeconds = leadSeconds;
    trace.windowSeconds = window;
    trace.input.resize (static_cast<size_t> (shown));
    trace.output.resize (static_cast<size_t> (shown));
    trace.wet.resize (static_cast<size_t> (shown));
    trace.reduction.resize (static_cast<size_t> (shown));
    trace.releaseSeconds = compressor.getReleaseSeconds();

    auto releaseCaptured = false;
    auto deepest = 0.0f;
    for (int i = 0; i < total; ++i)
    {
        const auto x = sampleAt (i);
        const auto reduction = compressor.process (std::abs (x));
        const auto wet = colour.process (x * std::pow (10.0f, -reduction * 0.05f), reduction, 0);
        if (i < start)
            continue;

        const auto j = static_cast<size_t> (i - start);
        trace.input[j] = x;
        trace.output[j] = blend.dry * x + blend.wet * wet;
        trace.wet[j] = wet;
        trace.reduction[j] = reduction;
        deepest = std::max (deepest, reduction);
        if (! releaseCaptured && deepest > 0.5f && reduction < deepest * 0.9f)
        {
            trace.releaseSeconds = compressor.getReleaseSeconds();
            releaseCaptured = true;
        }
    }

    if (! releaseCaptured)
        trace.releaseSeconds = compressor.getReleaseSeconds();
    return trace;
}

HitRender renderHit (const DetectorSettings& settings, double rate, bool match, Blend blend, const ColourSettings& colour)
{
    auto trace = simulate (settings, hitSpacingSeconds, rate, blend, colour);
    HitRender render { std::move (trace.input), std::move (trace.output) };

    if (match)
    {
        const auto energy = [] (const std::vector<float>& x)
        {
            auto sum = 0.0;
            for (const auto s : x)
                sum += static_cast<double> (s) * s;
            return sum;
        };
        const auto rawEnergy = energy (render.raw), wetEnergy = energy (trace.wet);
        const auto wetGain = wetEnergy > 0.0 ? static_cast<float> (std::sqrt (rawEnergy / wetEnergy)) : 1.0f;
        for (size_t i = 0; i < render.compressed.size(); ++i)
            render.compressed[i] = blend.dry * render.raw[i] + blend.wet * wetGain * trace.wet[i];
        if (const auto mixedEnergy = energy (render.compressed); mixedEnergy > 0.0)
            for (auto& s : render.compressed)
                s *= static_cast<float> (std::sqrt (rawEnergy / mixedEnergy));
    }

    auto peak = 0.0f;
    for (const auto s : render.compressed)
        peak = std::max (peak, std::abs (s));
    if (peak > hitCeiling)
        for (auto& s : render.compressed)
            s *= hitCeiling / peak;
    return render;
}

} // namespace lucky::example_hit

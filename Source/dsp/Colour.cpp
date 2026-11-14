#include "Colour.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace lucky
{
namespace
{
float fromDb (float db) noexcept
{
    return std::pow (10.0f, db / 20.0f);
}
} // namespace

Colour::Biquad Colour::design (FilterType type, double hz, double q, double rate)
{
    const auto w0 = 2.0 * std::numbers::pi * std::min (hz, rate * 0.499) / rate;
    const auto alpha = std::sin (w0) / (2.0 * q);
    const auto c = std::cos (w0);
    const auto a0 = 1.0 + alpha;
    const auto b0 = (type == FilterType::lowPass ? (1.0 - c) : (1.0 + c)) / 2.0;
    const auto b1 = type == FilterType::lowPass ? (1.0 - c) : -(1.0 + c);
    Biquad f;
    f.b0 = static_cast<float> (b0 / a0);
    f.b1 = static_cast<float> (b1 / a0);
    f.b2 = static_cast<float> (b0 / a0);
    f.a1 = static_cast<float> (-2.0 * c / a0);
    f.a2 = static_cast<float> ((1.0 - alpha) / a0);
    return f;
}

void Colour::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    envelopeRelease = static_cast<float> (std::exp (-1.0 / (envelopeReleaseSeconds * sampleRate)));
    blockerCoefficient = static_cast<float> (std::exp (-2.0 * std::numbers::pi * blockerHz / sampleRate));
    eqActive = false;
    updateEq();
    reset();
}

void Colour::updateEq() noexcept
{
    const auto fallDb = settings.mode == Mode::opto && active ? std::min (settings.amount, maxEqAmount) * optoFallDb : 0.0f;
    const auto wasActive = eqActive;
    eqActive = fallDb > 0.01f;
    if (! eqActive)
        return;

    // Bilinear (prewarped) frequencies, so the fall is exact at optoFallLowHz and optoFallHighHz at any rate.
    const auto warped = [this] (double hz) { return std::tan (std::numbers::pi * hz / sampleRate); };
    const auto unwarped = [this] (double w) { return std::atan (w) * sampleRate / std::numbers::pi; };
    const auto highPassW = warped (optoFallLowHz) * std::pow (std::pow (10.0, fallDb / 10.0) - 1.0, 0.25);
    const auto lowPassW = warped (optoFallHighHz) / std::sqrt (std::pow (10.0, fallDb / 20.0) - 1.0);

    const auto high = design (FilterType::highPass, unwarped (highPassW), std::sqrt (0.5), sampleRate);
    const auto low = design (FilterType::lowPass, unwarped (lowPassW), 0.5, sampleRate);
    const auto replace = [wasActive] (Biquad& filter, Biquad designed)
    {
        if (wasActive)
            std::tie (designed.z1, designed.z2) = std::pair (filter.z1, filter.z2);
        filter = designed;
    };
    for (size_t c = 0; c < highPass.size(); ++c)
    {
        replace (highPass[c], high);
        replace (lowPass[c], low);
    }
}

void Colour::reset() noexcept
{
    envelope.fill (0.0f);
    blockerIn.fill (0.0f);
    blockerOut.fill (0.0f);
    for (auto* filters : { &highPass, &lowPass })
        for (auto& f : *filters)
            f.z1 = f.z2 = 0.0f;
}

void Colour::setSettings (const ColourSettings& newSettings) noexcept
{
    if (newSettings == settings)
        return;

    settings = newSettings;
    const auto wasActive = active;
    active = settings.mode != Mode::clean && settings.amount > 1.0e-4f;
    amountDb = active ? 20.0f * std::log10 (settings.amount) : -200.0f;
    if (active && ! wasActive)
    {
        envelope.fill (0.0f);
        blockerIn.fill (0.0f);
        blockerOut.fill (0.0f);
    }
    updateEq();
}

Colour::Harmonics Colour::harmonicsAt (float levelDbFs, float reductionDb) const noexcept
{
    if (! active)
        return {};

    const auto dbu = levelDbFs + dbuAtFullScale;
    const auto compressing = std::clamp (reductionDb, 1.0e-3f, 1000.0f);
    float h2Dbc = -200.0f, h3Dbc = -200.0f;
    if (settings.mode == Mode::vca)
    {
        const auto inputDbu = dbu + std::max (0.0f, reductionDb);
        h2Dbc = vcaH2AtReferenceDbc + vcaH2PerDb * (inputDbu - vcaReferenceDbu) + vcaH2PerReductionDb * std::max (0.0f, reductionDb);
        h3Dbc = h2Dbc - vcaH3BelowH2Db;
    }
    else if (settings.mode == Mode::fet)
    {
        const auto outputStageH3 = fetH3AtReferenceDbc + fetH3PerDb * (dbu - fetH3ReferenceDbu);
        const auto compressingH3 = fetCompressingH3Dbc + 20.0f * std::log10 (std::min (1.0f, compressing / fetH3RampToReductionDb));
        h3Dbc = 10.0f * std::log10 (std::pow (10.0f, outputStageH3 / 10.0f) + std::pow (10.0f, compressingH3 / 10.0f));
        const auto beyondPeak = std::max (0.0f, reductionDb - fetH2FallFromReductionDb);
        h2Dbc = fetH2PeakDbc + 20.0f * std::log10 (std::min (1.0f, compressing / fetH2RampToReductionDb))
              - fetH2FallPerDbSquared * beyondPeak * beyondPeak;
    }
    else
    {
        h2Dbc = optoH2AtZeroDbuDbc + optoH2PerDb * dbu;
        h3Dbc = std::max (optoH3AtZeroDbuDbc + optoH3PerDb * dbu,
                          optoCompressingH3Dbc + 20.0f * std::log10 (std::min (1.0f, compressing / optoCompressingH3AtReductionDb)));
    }
    return { std::min (h2Dbc + amountDb, maxHarmonicDbc), std::min (h3Dbc + amountDb, maxHarmonicDbc) };
}

float Colour::process (float x, float reductionDb, int channel) noexcept
{
    if (! active)
        return x;

    const auto c = static_cast<size_t> (channel);
    envelope[c] = std::max (std::abs (x), envelope[c] * envelopeRelease);
    const auto e = std::max (envelope[c], 1.0e-6f);
    const auto harmonics = harmonicsAt (20.0f * std::log10 (e), reductionDb);
    const auto h2 = 2.0f * fromDb (harmonics.secondDbc);
    const auto h3 = 4.0f * fromDb (harmonics.thirdDbc);
    const auto u = x / e;
    const auto added = e * (h2 * u * u + h3 * u * u * u);

    blockerOut[c] = added - blockerIn[c] + blockerCoefficient * blockerOut[c];
    blockerIn[c] = added;
    const auto coloured = x + blockerOut[c];

    return eqActive ? lowPass[c].process (highPass[c].process (coloured)) : coloured;
}

float Colour::amountAt (float position) noexcept
{
    const auto p = std::clamp (position, 0.0f, 1.0f);
    return p < offPosition ? 0.0f : std::pow (maxAmount, (p - offPosition) / (1.0f - offPosition));
}

float Colour::positionOf (float amount) noexcept
{
    const auto a = std::clamp (amount, 0.0f, maxAmount);
    if (a <= 0.0f)
        return 0.0f;
    return offPosition + (1.0f - offPosition) * std::log10 (std::max (a, 1.0f)) / std::log10 (maxAmount);
}

} // namespace lucky

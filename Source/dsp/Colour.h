#pragma once

#include "Modes.h"
#include <array>

namespace lucky
{

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
/** What the colour stage needs to know: whose tone, how much of it (0 clean, 1 like the measured unit, 2 twice),
    and the threshold FET's harmonics are measured against. */
struct ColourSettings
{
    Mode mode = Mode::clean;
    float amount = 1.0f;
    float thresholdDb = -24.0f;

    bool operator== (const ColourSettings&) const = default;
};
#pragma clang diagnostic pop

/** Each classic compressor's tone, voiced from published specifications and published measurements of hardware
    units (docs/colour-sources.md). It runs on the compressed signal, before it is combined with the dry one.

    Harmonics come from x + e·(h2·u² + h3·u³) with u = x / e, e the signal's own peak envelope, so each harmonic's
    level relative to the signal (dBc) is set directly: H2 = h2 / 2, H3 = h3 / 4. A DC blocker takes out the
    offset the even term adds. Levels are read as dBu with 0 dBFS = +18 dBu. The amount scales the harmonics (2× is
    +6 dB per harmonic, 10× is +20 dB) and blends the EQ (2× doubles its deviation, and it goes no further).

    - FET: the output stage's third harmonic, −52 dBc at +16 dBu falling 2 dB per dB below it (Moore). While
      compressing (our Behringer 676, measurements/676/): the FET's second harmonic ramps in to −62 dBc by 2.5 dB of
      reduction, holds to 5 dB, then falls 0.5·(GR − 5)² dB as the even-order cancelling feedback works harder;
      and a third harmonic of −69 dBc ramps in over the first 2 dB, summed in power with the output stage's. The
      detector itself adds the low-frequency distortion of fast release.
    - Opto: the tube stage's second harmonic (−54 dBc at 0 dBu, +0.8 dB per dB) and a little third (−85 dBc at
      0 dBu, +1.75 dB per dB); while compressing, the cell's third harmonic dominates, in proportion to the
      reduction (−36 dBc at 6 dB). The response falls 1 dB at 30 Hz and 15 kHz.
    - VCA: the VCA chip's own distortion, as on a 4000 G's unbalanced, untrimmed-style dbx 202 (THAT 2180B datasheet):
      second harmonic −80 dBc at 0 dBV in, +0.4 dB per dB of input level and +0.15 dB per dB of gain reduction,
      third 24 dB below it. The detector itself adds the low-frequency distortion of fast attack and release,
      which is most of what SSL measures under compression. No EQ (the published response is flat). */
class Colour
{
public:
    static constexpr float envelopeReleaseSeconds = 0.03f;
    static constexpr float blockerHz = 10.0f;
    /** dBu at 0 dBFS (EBU R68). */
    static constexpr float dbuAtFullScale = 18.0f;

    static constexpr float fetH3AtReferenceDbc = -52.0f;
    static constexpr float fetH3ReferenceDbu = 16.0f;
    static constexpr float fetH3PerDb = 2.0f;
    static constexpr float fetH2PeakDbc = -62.0f;
    static constexpr float fetH2RampToReductionDb = 2.5f;
    static constexpr float fetH2FallFromReductionDb = 5.0f;
    static constexpr float fetH2FallPerDbSquared = 0.5f;
    static constexpr float fetCompressingH3Dbc = -69.0f;
    static constexpr float fetH3RampToReductionDb = 2.0f;

    static constexpr float optoH2AtZeroDbuDbc = -54.0f;
    static constexpr float optoH2PerDb = 0.8f;
    static constexpr float optoH3AtZeroDbuDbc = -85.0f;
    static constexpr float optoH3PerDb = 1.75f;
    static constexpr float optoCompressingH3Dbc = -36.0f;
    static constexpr float optoCompressingH3AtReductionDb = 6.0f;
    /** Opto's response falls this much at both ends (a Butterworth high-pass and a Q 0.5 low-pass), times the
        amount: the filters are redesigned for each amount so the fall scales in dB without phase mixing. */
    static constexpr float optoFallDb = 1.0f;
    static constexpr float optoFallLowHz = 30.0f;
    static constexpr float optoFallHighHz = 15000.0f;

    static constexpr float vcaH2AtReferenceDbc = -80.0f;
    /** 0 dBV, the level of the datasheet's spectrum. */
    static constexpr float vcaReferenceDbu = 2.2f;
    static constexpr float vcaH2PerDb = 0.4f;
    static constexpr float vcaH2PerReductionDb = 0.15f;
    static constexpr float vcaH3BelowH2Db = 24.0f;

    /** The dial goes this far past the hardware, so the tone can be heard clearly and then backed off. */
    static constexpr float maxAmount = 10.0f;
    /** The EQ stops exaggerating here: ten times Opto's roll-off would be a treble cut, not colour. */
    static constexpr float maxEqAmount = 2.0f;
    /** No harmonic goes above this, which keeps the waveshaper monotonic (H2 must stay below -12 dBc). */
    static constexpr float maxHarmonicDbc = -14.0f;

    /** The second and third harmonics added, in dB relative to the signal (as measured on a sine). */
    struct Harmonics
    {
        float secondDbc = -200.0f, thirdDbc = -200.0f;
    };

    /** What the stage adds now to a signal peaking at levelDbFs while the compressor reduces by reductionDb. */
    Harmonics harmonicsAt (float levelDbFs, float reductionDb) const noexcept;

    /** Where the Colour dial leaves Off: below 1× the colour is too faint to hear, so the dial goes from Off
        straight to 1× here and on to maxAmount on a log scale. */
    static constexpr float offPosition = 0.08f;

    /** The Colour dial: Off up to offPosition, then 1× to maxAmount on a log scale. An amount between 0 and 1×
        (only from an old session or automation) sits at 1×. */
    static float amountAt (float position) noexcept;
    static float positionOf (float amount) noexcept;

    void prepare (double sampleRate);
    void reset() noexcept;
    void setSettings (const ColourSettings&) noexcept;

    /** True when the mode has a tone and the amount is above zero. */
    bool isActive() const noexcept { return active; }

    /** Colours one sample of one channel (0 or 1); reductionDb is the compressor's current gain reduction. */
    float process (float x, float reductionDb, int channel) noexcept;

private:
    enum class FilterType
    {
        lowPass,
        highPass
    };

    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float z1 = 0, z2 = 0;

        float process (float x) noexcept
        {
            const auto y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    static Biquad design (FilterType, double hz, double q, double sampleRate);
    void updateEq() noexcept;

    ColourSettings settings;
    double sampleRate = 48000.0;
    bool active = false, eqActive = false;
    float amountDb = 0.0f;
    float envelopeRelease = 0.999f, blockerCoefficient = 0.999f;
    std::array<float, 2> envelope {}, blockerIn {}, blockerOut {};
    std::array<Biquad, 2> highPass {}, lowPass {};
};

} // namespace lucky

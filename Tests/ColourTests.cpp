#include "dsp/Colour.h"
#include <cmath>
#include <juce_core/juce_core.h>
#include <numbers>
#include <vector>

namespace lucky
{

class ColourTests final : public juce::UnitTest
{
public:
    ColourTests() : juce::UnitTest ("Colour", "Lucky Compressor") {}

    static constexpr double rate = 48000.0;

    /** Colours a steady sine and returns the output's last half second. */
    static std::vector<float> render (const ColourSettings& settings, double hz, float amplitude, float reductionDb)
    {
        Colour colour;
        colour.prepare (rate);
        colour.setSettings (settings);
        std::vector<float> out (static_cast<size_t> (rate));
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = colour.process (amplitude * static_cast<float> (std::sin (2.0 * std::numbers::pi * hz * static_cast<double> (i) / rate)), reductionDb, 0);
        return { out.begin() + static_cast<std::ptrdiff_t> (out.size() / 2), out.end() };
    }

    static double amplitudeAt (const std::vector<float>& x, double hz)
    {
        const auto w = 2.0 * std::numbers::pi * hz / rate;
        auto re = 0.0, im = 0.0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            re += x[i] * std::cos (w * static_cast<double> (i));
            im += x[i] * std::sin (w * static_cast<double> (i));
        }
        return 2.0 * std::sqrt (re * re + im * im) / static_cast<double> (x.size());
    }

    static double harmonicDbc (const std::vector<float>& x, double hz, int n)
    {
        return 20.0 * std::log10 (amplitudeAt (x, hz * n) / amplitudeAt (x, hz));
    }

    void runTest() override
    {
        constexpr auto hz = 1000.0;

        beginTest ("Clean and amount 0 leave the signal untouched");
        for (const auto settings : { ColourSettings { Mode::clean, 1.0f, -20.0f }, ColourSettings { Mode::vca, 0.0f, -20.0f },
                                     ColourSettings { Mode::fet, 0.0f, -20.0f }, ColourSettings { Mode::opto, 0.0f, -20.0f } })
        {
            Colour colour;
            colour.prepare (rate);
            colour.setSettings (settings);
            expect (! colour.isActive());
            expect (juce::exactlyEqual (colour.process (0.3f, 5.0f, 0), 0.3f));
        }

        const auto atDbu = [] (float dbu) { return std::pow (10.0f, (dbu - Colour::dbuAtFullScale) / 20.0f); };

        beginTest ("FET: output-stage H3 as published for a Rev D (-52 dBc at +16 dBu), steeper below");
        {
            const ColourSettings fet { Mode::fet, 1.0f, -20.0f };
            expectWithinAbsoluteError (harmonicDbc (render (fet, hz, atDbu (16.0f), 0.0f), hz, 3), -52.0, 1.0);
            expectWithinAbsoluteError (harmonicDbc (render (fet, hz, atDbu (9.0f), 0.0f), hz, 3), -66.0, 1.0);
            expect (harmonicDbc (render (fet, hz, atDbu (16.0f), 0.0f), hz, 2) < -90.0, "no H2 without gain reduction");
        }

        beginTest ("FET: H2 peaks at -62 dBc from 2.5 to 5 dB of reduction, then falls (676 slow-release runs)");
        {
            const ColourSettings fet { Mode::fet, 1.0f, -20.0f };
            const auto h2At = [&] (float reductionDb) { return harmonicDbc (render (fet, hz, atDbu (0.0f), reductionDb), hz, 2); };
            expectWithinAbsoluteError (h2At (1.25f), -68.0, 1.0);
            expectWithinAbsoluteError (h2At (2.5f), -62.0, 1.0);
            expectWithinAbsoluteError (h2At (5.0f), -62.0, 1.0);
            expectWithinAbsoluteError (h2At (9.0f), -70.0, 1.0);
            expectWithinAbsoluteError (h2At (11.5f), -83.1, 1.5);
        }

        beginTest ("FET: H3 of -69 dBc while compressing, ramping in over 2 dB, summed with the output stage's");
        {
            const ColourSettings fet { Mode::fet, 1.0f, -20.0f };
            const auto h3At = [&] (float dbu, float reductionDb) { return harmonicDbc (render (fet, hz, atDbu (dbu), reductionDb), hz, 3); };
            expectWithinAbsoluteError (h3At (0.0f, 4.0f), -68.9, 1.0);
            expectWithinAbsoluteError (h3At (0.0f, 1.0f), -75.0, 1.0);
            expectWithinAbsoluteError (h3At (16.0f, 4.0f), -51.8, 1.0);
        }

        beginTest ("Opto: tube H2 first with no reduction (-54/-85 dBc at 0 dBu, -41/-57 at +16)");
        {
            const ColourSettings opto { Mode::opto, 1.0f, -20.0f };
            const auto quiet = render (opto, hz, atDbu (0.0f), 0.0f);
            const auto loud = render (opto, hz, atDbu (16.0f), 0.0f);
            expectWithinAbsoluteError (harmonicDbc (quiet, hz, 2), -54.0, 1.0);
            expectWithinAbsoluteError (harmonicDbc (loud, hz, 2), -41.2, 1.0);
            expectWithinAbsoluteError (harmonicDbc (loud, hz, 3), -57.0, 1.5);
        }

        beginTest ("Opto: the cell's H3 dominates while compressing (-36 dBc at 6 dB)");
        {
            const ColourSettings opto { Mode::opto, 1.0f, -20.0f };
            const auto compressed = render (opto, hz, atDbu (4.0f), 6.0f);
            expectWithinAbsoluteError (harmonicDbc (compressed, hz, 3), -36.0, 1.0);
            expect (harmonicDbc (compressed, hz, 3) > harmonicDbc (compressed, hz, 2) + 10.0);
        }

        beginTest ("VCA: the chip's second harmonic, -80 dBc at 0 dBV, third 24 dB below (THAT 2180B datasheet)");
        {
            const ColourSettings vca { Mode::vca, 1.0f, -20.0f };
            const auto nominal = render (vca, hz, atDbu (Colour::vcaReferenceDbu), 0.0f);
            expectWithinAbsoluteError (harmonicDbc (nominal, hz, 2), -80.0, 1.0);
            expectWithinAbsoluteError (harmonicDbc (nominal, hz, 3), -104.0, 1.5);
            const auto hot = render (vca, hz, atDbu (Colour::vcaReferenceDbu + 20.0f), 0.0f);
            expectWithinAbsoluteError (harmonicDbc (hot, hz, 2), -72.0, 1.0);
            const auto reduced = render (vca, hz, atDbu (Colour::vcaReferenceDbu - 15.0f), 15.0f);
            expectWithinAbsoluteError (harmonicDbc (reduced, hz, 2), -80.0 + 0.15 * 15.0, 1.0);
            const auto exaggerated = render ({ Mode::vca, 10.0f, -20.0f }, hz, atDbu (Colour::vcaReferenceDbu), 0.0f);
            expectWithinAbsoluteError (harmonicDbc (exaggerated, hz, 2), -60.0, 1.0);
        }

        beginTest ("The amount scales the harmonics: 2x is about +6 dB (strong H3 also lifts the fundamental a little)");
        {
            const auto one = harmonicDbc (render ({ Mode::opto, 1.0f, -20.0f }, hz, atDbu (4.0f), 6.0f), hz, 3);
            const auto two = harmonicDbc (render ({ Mode::opto, 2.0f, -20.0f }, hz, atDbu (4.0f), 6.0f), hz, 3);
            expectWithinAbsoluteError (two - one, 6.02, 0.5);
        }

        beginTest ("The dial: Off, then straight to 1x, 20x at full, and back again");
        {
            const auto off = Colour::offPosition;
            expectWithinAbsoluteError (Colour::amountAt (0.0f), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (Colour::amountAt (off * 0.9f), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (Colour::amountAt (off), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError (Colour::amountAt (off + (1.0f - off) * 0.5f), std::sqrt (Colour::maxAmount), 1.0e-4f);
            expectWithinAbsoluteError (Colour::amountAt (1.0f), Colour::maxAmount, 1.0e-4f);
            for (const auto amount : { 0.0f, 1.0f, 2.0f, 6.5f, 10.0f })
                expectWithinAbsoluteError (Colour::amountAt (Colour::positionOf (amount)), amount, 1.0e-4f);
            expectWithinAbsoluteError (Colour::amountAt (Colour::positionOf (0.3f)), 1.0f, 1.0e-4f);
        }

        beginTest ("10x is +20 dB per harmonic, but never above the cap");
        {
            const auto fetAt = [&] (float amount) { return harmonicDbc (render ({ Mode::fet, amount, -20.0f }, hz, atDbu (0.0f), 7.0f), hz, 2); };
            expectWithinAbsoluteError (fetAt (10.0f) - fetAt (1.0f), 20.0, 0.5);
            const auto opto = render ({ Mode::opto, 10.0f, -20.0f }, hz, atDbu (16.0f), 20.0f);
            expect (harmonicDbc (opto, hz, 3) < Colour::maxHarmonicDbc + 1.0);
        }

        beginTest ("The harmonics it reports are the ones it adds (the Lab's readout)");
        for (const auto settings : { ColourSettings { Mode::fet, 3.0f, -20.0f }, ColourSettings { Mode::opto, 1.0f, -20.0f }, ColourSettings { Mode::vca, 10.0f, -20.0f } })
        {
            Colour colour;
            colour.setSettings (settings);
            const auto reported = colour.harmonicsAt (-2.0f, 7.0f);
            const auto measured = render (settings, hz, std::pow (10.0f, -2.0f / 20.0f), 7.0f);
            expectWithinAbsoluteError (static_cast<double> (reported.secondDbc), harmonicDbc (measured, hz, 2), 1.0);
            expectWithinAbsoluteError (static_cast<double> (reported.thirdDbc), harmonicDbc (measured, hz, 3), 1.0);
        }

        beginTest ("Opto's fall scales in dB with the amount, up to 2x and no further");
        {
            const auto gainAt = [&] (float amount, double f)
            {
                const auto small = 0.001f;
                return 20.0 * std::log10 (amplitudeAt (render ({ Mode::opto, amount, -20.0f }, f, small, 0.0f), f) / small);
            };
            for (const auto f : { 30.0, 15000.0 })
            {
                expectWithinAbsoluteError (gainAt (0.5f, f), -0.5, 0.05);
                expectWithinAbsoluteError (gainAt (2.0f, f), -2.0, 0.05);
                expectWithinAbsoluteError (gainAt (10.0f, f), -2.0, 0.05);
            }
        }

        beginTest ("Opto's response falls 1 dB at 30 Hz and 15 kHz, flat in between");
        {
            const ColourSettings opto { Mode::opto, 1.0f, -20.0f };
            const auto small = 0.001f;
            const auto gainAt = [&] (double f) { return 20.0 * std::log10 (amplitudeAt (render (opto, f, small, 0.0f), f) / small); };
            expectWithinAbsoluteError (gainAt (1000.0), 0.0, 0.05);
            expectWithinAbsoluteError (gainAt (30.0), -1.0, 0.05);
            expectWithinAbsoluteError (gainAt (15000.0), -1.0, 0.05);
            expectWithinAbsoluteError (gainAt (5000.0), 0.0, 0.1);
        }
    }
};

static ColourTests colourTests;

} // namespace lucky

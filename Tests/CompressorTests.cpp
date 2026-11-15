#include "dsp/Compressor.h"
#include <cmath>
#include <juce_core/juce_core.h>

namespace lucky
{

class CompressorTests final : public juce::UnitTest
{
public:
    CompressorTests() : juce::UnitTest ("Compressor", "Lucky Compressor") {}

    static constexpr double rate = 48000.0;

    static float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

    /** Runs a steady level for some seconds and returns the last reduction. */
    static float hold (Compressor& c, float levelDb, double seconds)
    {
        float r = 0.0f;
        for (int i = 0; i < static_cast<int> (seconds * rate); ++i)
            r = c.process (dbToGain (levelDb));
        return r;
    }

    void runTest() override
    {
        beginTest ("Static curve: no reduction below the knee, exact slope above it");
        {
            Compressor c;
            c.prepare (rate);
            c.setSettings ({ -20.0f, 4.0f, 6.0f, 10.0f, 100.0f, ReleaseShape::fixed });
            expectEquals (c.staticReduction (-30.0f), 0.0f);
            expectEquals (c.staticReduction (-23.0f), 0.0f);
            expectWithinAbsoluteError (c.staticReduction (-8.0f), 12.0f * 0.75f, 1.0e-4f);
            expectWithinAbsoluteError (c.staticReduction (-20.0f), 0.75f * 9.0f / 12.0f, 1.0e-4f);
            expect (c.staticReduction (-21.0f) < c.staticReduction (-19.0f));
        }

        beginTest ("Attack reaches 63% of a step in the attack time");
        {
            Compressor c;
            c.prepare (rate);
            c.setSettings ({ -20.0f, 4.0f, 0.0001f, 10.0f, 100.0f, ReleaseShape::fixed });
            const auto full = c.staticReduction (-8.0f);
            const auto atAttack = hold (c, -8.0f, 0.010);
            expectWithinAbsoluteError (atAttack / full, 0.632f, 0.01f);
        }

        beginTest ("Release recovers 63% in the release time");
        {
            Compressor c;
            c.prepare (rate);
            c.setSettings ({ -20.0f, 4.0f, 0.0001f, 0.01f, 100.0f, ReleaseShape::fixed });
            const auto full = hold (c, -8.0f, 0.1);
            const auto after = hold (c, -60.0f, 0.1);
            expectWithinAbsoluteError (after / full, 0.368f, 0.01f);
        }

        beginTest ("Opto lets half go fast and the rest slowly");
        {
            Compressor c;
            c.prepare (rate);
            c.setSettings (modes::resolve (Mode::opto, { -30.0f, 3.0f, 10.0f, 500.0f, false }));
            const auto full = hold (c, -6.0f, 2.0);
            const auto after200ms = hold (c, -80.0f, 0.2);
            expect (after200ms < full * 0.62f, "fast stage lets go of about half");
            expect (after200ms > full * 0.4f, "slow stage still holds about half");
        }

        beginTest ("Opto holds on longer the harder it is pushed");
        {
            const auto releaseAfter = [] (float levelDb)
            {
                Compressor c;
                c.prepare (rate);
                c.setSettings (modes::resolve (Mode::opto, { -30.0f, 3.0f, 10.0f, 500.0f, false }));
                hold (c, levelDb, 4.0);
                return c.getReleaseSeconds();
            };
            const auto gentle = releaseAfter (-27.0f), hard = releaseAfter (-3.0f);
            expect (hard > gentle + 1.0f, "release " + juce::String (gentle) + " s vs " + juce::String (hard) + " s");
        }

        beginTest ("Fixed release reports its own time");
        {
            Compressor c;
            c.prepare (rate);
            c.setSettings ({ -20.0f, 4.0f, 6.0f, 10.0f, 250.0f, ReleaseShape::fixed });
            expectWithinAbsoluteError (c.getReleaseSeconds(), 0.25f, 1.0e-6f);
        }
    }
};

static CompressorTests compressorTests;

} // namespace lucky

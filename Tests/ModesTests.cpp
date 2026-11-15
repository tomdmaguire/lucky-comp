#include "dsp/Modes.h"
#include <juce_core/juce_core.h>

namespace lucky
{

class ModesTests final : public juce::UnitTest
{
public:
    ModesTests() : juce::UnitTest ("Modes", "Lucky Compressor") {}

    void runTest() override
    {
        beginTest ("FET dials span the classic FET range, 7 fastest");
        expectWithinAbsoluteError (modes::fetAttackMs (1.0f), 0.8f, 1.0e-4f);
        expectWithinAbsoluteError (modes::fetAttackMs (7.0f), 0.02f, 1.0e-5f);
        expectWithinAbsoluteError (modes::fetReleaseMs (1.0f), 1100.0f, 0.01f);
        expectWithinAbsoluteError (modes::fetReleaseMs (7.0f), 50.0f, 0.01f);
        for (const auto dial : { 1.0f, 2.5f, 4.0f, 6.3f, 7.0f })
        {
            expectWithinAbsoluteError (modes::fetAttackDial (modes::fetAttackMs (dial)), dial, 1.0e-4f);
            expectWithinAbsoluteError (modes::fetReleaseDial (modes::fetReleaseMs (dial)), dial, 1.0e-4f);
        }

        beginTest ("FET clamps times and snaps ratio");
        {
            const auto s = modes::resolve (Mode::fet, { -50.0f, 6.0f, 20.0f, 3000.0f, true });
            expectEquals (s.thresholdDb, -40.0f);
            expectEquals (s.ratio, 8.0f);
            expectEquals (s.attackMs, 0.8f);
            expectEquals (s.releaseMs, 1100.0f);
            expect (s.release == ReleaseShape::fixed);
        }

        beginTest ("Opto fixes ratio and attack and follows the music");
        {
            const auto s = modes::resolve (Mode::opto, { -20.0f, 12.0f, 0.1f, 50.0f, false });
            expectEquals (s.ratio, modes::optoRatio);
            expectEquals (s.attackMs, 10.0f);
            expect (s.release == ReleaseShape::opto);
        }

        beginTest ("VCA snaps to its switch positions");
        {
            const auto s = modes::resolve (Mode::vca, { -18.0f, 5.0f, 2.0f, 450.0f, false });
            expectEquals (s.ratio, 4.0f);
            expectEquals (s.attackMs, 3.0f);
            expectEquals (s.releaseMs, 600.0f);
            expect (s.release == ReleaseShape::fixed);
            expect (modes::resolve (Mode::vca, { -18.0f, 4.0f, 10.0f, 300.0f, true }).release == ReleaseShape::automatic);
        }

        beginTest ("Clean passes values through and caps release at the scale's end");
        {
            const auto s = modes::resolve (Mode::clean, { -33.0f, 2.7f, 0.37f, 9000.0f, true });
            expectEquals (s.thresholdDb, -33.0f);
            expectEquals (s.ratio, 2.7f);
            expectEquals (s.attackMs, 0.37f);
            expectEquals (s.releaseMs, modes::maxReleaseMs);
            expect (s.release == ReleaseShape::fixed);
        }

        beginTest ("Defaults are already within their mode");
        for (int m = 0; m < numModes; ++m)
        {
            const auto mode = static_cast<Mode> (m);
            const auto d = modes::defaults (mode);
            const auto c = modes::constrain (mode, d);
            expectWithinAbsoluteError (c.attackMs, d.attackMs, 1.0e-5f);
            expectWithinAbsoluteError (c.releaseMs, d.releaseMs, 1.0e-3f);
            expectEquals (c.ratio, d.ratio);
        }
    }
};

static ModesTests modesTests;

} // namespace lucky

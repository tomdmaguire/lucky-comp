#include "dsp/ExampleHit.h"
#include <algorithm>
#include <cmath>
#include <juce_core/juce_core.h>

namespace lucky
{

class ExampleHitTests final : public juce::UnitTest
{
public:
    ExampleHitTests() : juce::UnitTest ("Example hit", "Lucky Comp") {}

    /** Loudest output in the first few milliseconds of the hit, in dB. */
    static float frontPeakDb (const HitTrace& trace)
    {
        const auto from = static_cast<size_t> (trace.leadSeconds * trace.sampleRate);
        const auto to = from + static_cast<size_t> (0.003 * trace.sampleRate);
        auto peak = 0.0f;
        for (auto i = from; i < to; ++i)
            peak = std::max (peak, std::abs (trace.output[i]));
        return 20.0f * std::log10 (peak);
    }

    void runTest() override
    {
        beginTest ("The snare loads at any rate, peaks at -6 dBFS, rings on and fades to silence");
        {
            for (const auto rate : { 44100.0, 48000.0, 96000.0 })
            {
                const auto hit = example_hit::makeHit (rate);
                const auto seconds = static_cast<double> (hit.size()) / rate;
                expectWithinAbsoluteError (seconds, 0.95, 0.01);
                expect (seconds < example_hit::hitSpacingSeconds, "the tail fits between hits");

                auto peak = 0.0f, room = 0.0f, end = 0.0f;
                for (size_t i = 0; i < hit.size(); ++i)
                {
                    const auto t = static_cast<double> (i) / rate;
                    peak = std::max (peak, std::abs (hit[i]));
                    if (t > 0.2 && t < 0.25)
                        room = std::max (room, std::abs (hit[i]));
                    if (t > 0.9)
                        end = std::max (end, std::abs (hit[i]));
                }
                expectWithinAbsoluteError (peak, example_hit::hitPeak, 0.01f);
                expect (room > peak * 0.01f, "the room is still sounding 200 ms in: " + juce::String (room));
                expect (end < peak * 0.001f, "silent by the end: " + juce::String (end));
                expect (std::abs (hit.front()) < 1.0e-3f, "starts without a click");
            }
        }

        beginTest ("Resampling keeps the snare's level and timing");
        {
            const auto at48 = example_hit::makeHit (48000.0);
            const auto at96 = example_hit::makeHit (96000.0);
            const auto energy = [] (const std::vector<float>& x, double rate)
            {
                auto sum = 0.0;
                for (const auto s : x)
                    sum += static_cast<double> (s) * s;
                return 10.0 * std::log10 (sum / rate);
            };
            const auto peakTime = [] (const std::vector<float>& x, double rate)
            {
                const auto peak = std::max_element (x.begin(), x.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); });
                return static_cast<double> (std::distance (x.begin(), peak)) / rate;
            };
            expectWithinAbsoluteError (energy (at96, 96000.0), energy (at48, 48000.0), 0.2);
            expectWithinAbsoluteError (peakTime (at96, 96000.0), peakTime (at48, 48000.0), 0.0002);
        }

        beginTest ("A slow attack lets the front of the hit through");
        {
            const auto settings = [] (float attackMs) { return modes::resolve (Mode::clean, { -24.0f, 4.0f, attackMs, 150.0f, false }); };
            const auto fast = example_hit::simulate (settings (0.05f), 0.4);
            const auto slow = example_hit::simulate (settings (30.0f), 0.4);
            expect (frontPeakDb (slow) > frontPeakDb (fast) + 6.0f,
                    juce::String (frontPeakDb (slow)) + " vs " + juce::String (frontPeakDb (fast)));
        }

        beginTest ("Opto reports a release that follows the music");
        {
            auto controls = modes::defaults (Mode::opto);
            controls.thresholdDb = -20.0f;
            const auto trace = example_hit::simulate (modes::resolve (Mode::opto, controls), example_hit::windowSeconds);
            expectEquals (static_cast<int> (trace.input.size()), static_cast<int> (example_hit::windowSeconds * trace.sampleRate));
            expect (trace.releaseSeconds > 0.4f);
            expect (*std::max_element (trace.reduction.begin(), trace.reduction.end()) > 1.0f);
        }
    }
};

static ExampleHitTests exampleHitTests;

} // namespace lucky

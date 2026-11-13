#include "TestSignals.h"
#include "dsp/Compressor.h"
#include "dsp/LevelMatcher.h"
#include <juce_core/juce_core.h>

namespace lucky
{

class LevelMatcherTests final : public juce::UnitTest
{
public:
    LevelMatcherTests() : juce::UnitTest ("Level matcher", "Lucky Comp") {}

    static constexpr double rate = 48000.0;

    static void record (LevelMatcher& m, const juce::AudioBuffer<float>& audio, int start, int length)
    {
        for (int i = start; i < start + length; ++i)
            m.record (audio.getSample (0, i), audio.getSample (1, i));
    }

    /** The make-up that really matches: K-weighted energy of the input over that of the actual compressed output,
        both over the last historySeconds of the audio. */
    static float renderedMatch (const juce::AudioBuffer<float>& audio, const DetectorSettings& settings, Blend blend = {})
    {
        Compressor compressor;
        compressor.prepare (rate);
        compressor.setSettings (settings);
        std::array<KWeighting, 4> filters {};
        for (auto& f : filters)
            f.prepare (rate);

        const auto total = audio.getNumSamples();
        const auto from = total - static_cast<int> (LevelMatcher::historySeconds * rate);
        auto in = 0.0, out = 0.0;
        for (int i = 0; i < total; ++i)
        {
            const auto l = audio.getSample (0, i), r = audio.getSample (1, i);
            const auto g = blend.dry + blend.wet * std::pow (10.0f, -compressor.process (std::max (std::abs (l), std::abs (r))) * 0.05f);
            const auto a = filters[0].process (l), b = filters[1].process (r), c = filters[2].process (l * g), d = filters[3].process (r * g);
            if (i >= from)
            {
                in += a * a + b * b;
                out += c * c + d * d;
            }
        }
        return static_cast<float> (std::sqrt (in / out));
    }

    void runTest() override
    {
        const auto loop = test::drumLoop (rate, 2);
        const auto total = loop.getNumSamples();
        const auto heavy = modes::resolve (Mode::clean, { -30.0f, 8.0f, 1.0f, 100.0f, false });

        beginTest ("Predicts nothing until it has heard some sound");
        {
            LevelMatcher m;
            m.prepare (rate);
            expect (! m.predict (heavy).has_value());
            juce::AudioBuffer<float> silence (2, static_cast<int> (rate));
            silence.clear();
            record (m, silence, 0, silence.getNumSamples());
            expect (! m.predict (heavy).has_value());
        }

        beginTest ("Prediction matches a real render within 0.5 dB in every mode (Opto remembers further back than the history)");
        for (int mode = 0; mode < numModes; ++mode)
        {
            const auto settings = modes::resolve (static_cast<Mode> (mode), { -30.0f, 4.0f, 1.0f, 100.0f, false });
            LevelMatcher m;
            m.prepare (rate);
            record (m, loop, 0, total);
            const auto predicted = m.predict (settings);
            expect (predicted.has_value());
            const auto errorDb = 20.0f * std::log10 (predicted.value_or (1.0f) / renderedMatch (loop, settings));
            expectWithinAbsoluteError (errorDb, 0.0f, 0.5f, modes::name (static_cast<Mode> (mode)));
        }

        beginTest ("Prediction follows Mix: parallel needs less make-up, dry needs none");
        {
            LevelMatcher m;
            m.prepare (rate);
            record (m, loop, 0, total);
            const auto half = m.predict (heavy, Blend::mix (50.0f)).value_or (0.0f);
            expectWithinAbsoluteError (20.0f * std::log10 (half / renderedMatch (loop, heavy, Blend::mix (50.0f))), 0.0f, 0.5f);
            expect (half < m.predict (heavy, Blend::mix (100.0f)).value_or (0.0f));
            expectWithinAbsoluteError (m.predict (heavy, Blend::mix (0.0f)).value_or (0.0f), 1.0f, 0.001f);
        }

        beginTest ("Blend adds to the dry signal, so matching turns it down");
        {
            LevelMatcher m;
            m.prepare (rate);
            record (m, loop, 0, total);
            const auto blend = Blend::added (0.0f, -60.0f);
            const auto gain = m.predict (heavy, blend).value_or (1.0f);
            expect (gain < 1.0f, "gain " + juce::String (gain));
            expectWithinAbsoluteError (20.0f * std::log10 (gain / renderedMatch (loop, heavy, blend)), 0.0f, 0.5f);
            expectWithinAbsoluteError (m.predict (heavy, Blend::added (-60.0f, -60.0f)).value_or (0.0f), 1.0f, 0.001f);
        }

        beginTest ("A change of settings is answered at once from the same history");
        {
            LevelMatcher m;
            m.prepare (rate);
            record (m, loop, 0, total);
            const auto gentle = m.predict (modes::resolve (Mode::clean, { -10.0f, 2.0f, 10.0f, 100.0f, false }));
            const auto hard = m.predict (heavy);
            expect (gentle.has_value() && hard.has_value());
            expect (*hard > *gentle * 1.5f);
        }

        beginTest ("Settings that don't compress need no make-up");
        {
            LevelMatcher m;
            m.prepare (rate);
            record (m, loop, 0, total);
            expectWithinAbsoluteError (m.predict (modes::resolve (Mode::clean, { 0.0f, 4.0f, 10.0f, 100.0f, false })).value_or (0.0f), 1.0f, 0.01f);
        }

        beginTest ("Never more than +30 dB");
        {
            LevelMatcher m;
            m.prepare (rate);
            juce::AudioBuffer<float> loud (test::sine (rate, 1.0, 1.0f));
            record (m, loud, 0, loud.getNumSamples());
            const auto brickwall = modes::resolve (Mode::clean, { -60.0f, 20.0f, 0.01f, 5.0f, false });
            expect (m.predict (brickwall).value_or (100.0f) <= std::pow (10.0f, 1.5f) + 0.001f);
        }
    }
};

static LevelMatcherTests levelMatcherTests;

} // namespace lucky

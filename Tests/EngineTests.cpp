#include "TestSignals.h"
#include "dsp/CompressorEngine.h"
#include <juce_core/juce_core.h>

namespace lucky
{

class EngineTests final : public juce::UnitTest
{
public:
    EngineTests() : juce::UnitTest ("Engine", "Lucky Comp") {}

    static constexpr double rate = 48000.0;
    static constexpr int blockSize = 512;

    static juce::AudioBuffer<float> run (CompressorEngine& engine, const juce::AudioBuffer<float>& input)
    {
        juce::AudioBuffer<float> output (input);
        for (int start = 0; start < output.getNumSamples(); start += blockSize)
        {
            juce::AudioBuffer<float> block (output.getArrayOfWritePointers(), 2, start, std::min (blockSize, output.getNumSamples() - start));
            engine.process (block);
        }
        return output;
    }

    static DetectorSettings heavy() { return modes::resolve (Mode::clean, { -30.0f, 8.0f, 1.0f, 100.0f, false }); }

    void runTest() override
    {
        const auto loop = test::drumLoop (rate, 3);
        const auto total = loop.getNumSamples();

        beginTest ("Bypass is bit-identical to the input");
        {
            CompressorEngine engine;
            engine.setSettings (heavy());
            engine.setBypass (true);
            engine.prepare (rate);
            const auto out = run (engine, loop);
            auto identical = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < total; ++i)
                    identical = identical && juce::exactlyEqual (out.getSample (ch, i), loop.getSample (ch, i));
            expect (identical);
            expect (engine.getReduction() >= 0.0f);
        }

        beginTest ("The predicted make-up brings the compressed loop back to the input's loudness");
        {
            CompressorEngine engine;
            engine.setSettings (heavy());
            engine.prepare (rate);
            run (engine, loop);
            const auto gain = engine.getMatcher().predict (heavy());
            expect (gain.has_value());
            engine.setMakeupTarget (gain.value_or (1.0f));
            const auto out = run (engine, loop);

            const auto tail = static_cast<int> (rate * 2.0);
            std::array<KWeighting, 4> filters {};
            for (auto& f : filters)
                f.prepare (rate);
            auto in = 0.0, matched = 0.0;
            for (int i = 0; i < total; ++i)
            {
                const auto a = filters[0].process (loop.getSample (0, i)), b = filters[1].process (loop.getSample (1, i));
                const auto c = filters[2].process (out.getSample (0, i)), d = filters[3].process (out.getSample (1, i));
                if (i >= total - tail)
                {
                    in += a * a + b * b;
                    matched += c * c + d * d;
                }
            }
            expectWithinAbsoluteError (10.0 * std::log10 (matched / in), 0.0, 0.5);
            expect (engine.getMakeupDb() > 3.0f, "make-up " + juce::String (engine.getMakeupDb()) + " dB");
        }

        beginTest ("The test hit plays on its own path, under the ceiling, without touching your audio");
        {
            CompressorEngine engine;
            engine.setSettings (heavy());
            engine.prepare (rate);
            juce::AudioBuffer<float> silence (2, static_cast<int> (rate * example_hit::hitSpacingSeconds));
            silence.clear();
            engine.playTestHit (example_hit::renderHit (heavy(), rate, true));
            const auto out = run (engine, silence);
            const auto peak = out.getMagnitude (0, out.getNumSamples());
            expect (peak > 0.05f);
            expect (peak <= example_hit::hitCeiling + 1.0e-4f, "peak " + juce::String (peak));
            expect (engine.getMeterEntry (engine.getMeterPosition() - 1).inputDb < -100.0f, "meters show only your audio");
            expect (! engine.getMatcher().predict (heavy()).has_value(), "level match never hears the hit");
            const auto again = run (engine, silence);
            expectEquals (again.getMagnitude (0, again.getNumSamples()), 0.0f);
        }

        beginTest ("A matched hit is as loud as the raw one, unless that would cross the ceiling");
        {
            const auto loudness = [] (const example_hit::HitRender& render)
            {
                auto raw = 0.0, compressed = 0.0;
                auto peak = 0.0f;
                for (size_t i = 0; i < render.raw.size(); ++i)
                {
                    raw += static_cast<double> (render.raw[i]) * render.raw[i];
                    compressed += static_cast<double> (render.compressed[i]) * render.compressed[i];
                    peak = std::max (peak, std::abs (render.compressed[i]));
                }
                return std::pair { 10.0 * std::log10 (compressed / raw), peak };
            };
            const auto [gentleDb, gentlePeak] = loudness (example_hit::renderHit (modes::resolve (Mode::clean, { -20.0f, 2.0f, 10.0f, 150.0f, false }), rate, true));
            expect (gentlePeak < example_hit::hitCeiling);
            expectWithinAbsoluteError (gentleDb, 0.0, 0.1);
            const auto [heavyDb, heavyPeak] = loudness (example_hit::renderHit (modes::resolve (Mode::clean, { -50.0f, 20.0f, 10.0f, 150.0f, false }), rate, true));
            expectWithinAbsoluteError (heavyPeak, example_hit::hitCeiling, 1.0e-4f);
            expect (heavyDb < 0.0);
        }

        beginTest ("Mix at 0% passes the input through unchanged");
        {
            CompressorEngine engine;
            engine.setSettings (heavy());
            engine.setMatch (false);
            engine.setBlend (Blend::mix (0.0f));
            engine.prepare (rate);
            const auto out = run (engine, loop);
            auto worst = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < total; ++i)
                    worst = std::max (worst, std::abs (out.getSample (ch, i) - loop.getSample (ch, i)));
            expect (worst < 1.0e-6f, "worst " + juce::String (worst));
        }

        beginTest ("Without level match the compressed loop is quieter");
        {
            CompressorEngine engine;
            engine.setSettings (heavy());
            engine.setMatch (false);
            engine.prepare (rate);
            const auto out = run (engine, loop);
            expect (test::energy (out, 0, total) < test::energy (loop, 0, total) * 0.5);
            expectWithinAbsoluteError (engine.getMakeupDb(), 0.0f, 0.01f);
        }

        beginTest ("Switching what you hear fades instead of jumping");
        {
            CompressorEngine engine;
            engine.setSettings (heavy());
            engine.prepare (rate);
            auto signal = test::sine (rate, 0.5, 0.5f, 50.0);
            juce::AudioBuffer<float> first (signal.getArrayOfWritePointers(), 2, 0, blockSize);
            engine.process (first);
            engine.setBypass (true);
            juce::AudioBuffer<float> second (signal.getArrayOfWritePointers(), 2, blockSize, blockSize);
            const auto before = test::sine (rate, 0.5, 0.5f, 50.0);
            engine.process (second);
            auto biggestStep = 0.0f;
            for (int i = blockSize - 1; i < 2 * blockSize - 1; ++i)
                biggestStep = std::max (biggestStep, std::abs (signal.getSample (0, i + 1) - signal.getSample (0, i)));
            const auto sineStep = static_cast<float> (0.5 * 2.0 * std::numbers::pi * 50.0 / rate);
            expect (biggestStep < sineStep * 4.0f, "step " + juce::String (biggestStep));
            juce::ignoreUnused (before);
        }

        beginTest ("Meters record input, output and reduction");
        {
            CompressorEngine engine;
            engine.setSettings (heavy());
            engine.prepare (rate);
            run (engine, loop);
            const auto newest = engine.getMeterPosition();
            expectEquals (newest, total / CompressorEngine::meterBlockSize);
            auto deepest = 0.0f, loudestIn = -120.0f;
            for (int i = newest - 1200; i < newest; ++i)
            {
                const auto e = engine.getMeterEntry (i);
                deepest = std::max (deepest, e.reductionDb);
                loudestIn = std::max (loudestIn, e.inputDb);
                expect (e.outputDb <= e.inputDb + 0.001f);
            }
            expect (deepest > 6.0f);
            expect (loudestIn > -3.0f);
            expect (engine.isHearingAudio());
        }

        beginTest ("One NaN or infinite input sample is heard as silence and doesn't stick");
        {
            const auto fet = modes::resolve (Mode::fet, modes::defaults (Mode::fet));
            for (const auto bad : { std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() })
            {
                CompressorEngine engine;
                engine.setSettings (fet);
                engine.setColour ({ Mode::fet, 1.0f });
                engine.prepare (rate);
                auto input = loop;
                input.setSample (0, 1000, bad);
                input.setSample (1, 1000, bad);
                const auto out = run (engine, input);

                auto finite = true;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < total; ++i)
                        finite = finite && std::isfinite (out.getSample (ch, i));
                expect (finite);
                expect (std::isfinite (engine.getReduction()));
                const auto makeup = engine.getMatcher().predictMakeup (fet, Blend::mix (1.0f));
                expect (makeup.has_value() && std::isfinite (makeup->trim) && std::isfinite (makeup->wet));
            }
        }
    }
};

static EngineTests engineTests;

} // namespace lucky

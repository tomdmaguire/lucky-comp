#include "PluginProcessor.h"
#include "TestSignals.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
constexpr double rate = 48000.0;
constexpr int blockSize = 512;

std::unique_ptr<LuckyCompProcessor> makeProcessor()
{
    std::unique_ptr<LuckyCompProcessor> processor (dynamic_cast<LuckyCompProcessor*> (createPluginFilter()));
    processor->setPlayConfigDetails (2, 2, rate, blockSize);
    processor->prepareToPlay (rate, blockSize);
    return processor;
}

void setParameter (LuckyCompProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getParameters().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

/** Level match's whole gain at Mix 100%: the compressed signal's own make-up times the final trim. */
float totalGain (const LuckyCompProcessor& processor)
{
    return processor.getEngine().getWetMakeupTarget() * processor.getEngine().getMakeupTarget();
}

/** The drum loop 12 dB down, peaking near -13 dBFS: level match has room to match it in full. The loop itself
    peaks at -1 dBFS, so heavy settings run into the peak ceiling. */
juce::AudioBuffer<float> quietLoop()
{
    auto loop = lucky::test::drumLoop (rate);
    loop.applyGain (juce::Decibels::decibelsToGain (-12.0f));
    return loop;
}

juce::AudioBuffer<float> run (LuckyCompProcessor& processor, const juce::AudioBuffer<float>& input)
{
    juce::AudioBuffer<float> output (input);
    juce::MidiBuffer midi;
    for (int start = 0; start < output.getNumSamples(); start += blockSize)
    {
        juce::AudioBuffer<float> block (output.getArrayOfWritePointers(), 2, start, std::min (blockSize, output.getNumSamples() - start));
        processor.processBlock (block, midi);
    }
    return output;
}
} // namespace

class ProcessorTests final : public juce::UnitTest
{
public:
    ProcessorTests() : juce::UnitTest ("Processor", "Lucky Comp Processor") {}

    void runTest() override
    {
        using lucky::Mode;

        beginTest ("Starts in Clean with its defaults");
        {
            auto p = makeProcessor();
            expect (p->getMode() == Mode::clean);
            const auto c = p->getControls(), d = lucky::modes::defaults (Mode::clean);
            expectWithinAbsoluteError (c.thresholdDb, d.thresholdDb, 0.001f);
            expectWithinAbsoluteError (c.ratio, d.ratio, 0.001f);
            expectWithinAbsoluteError (c.attackMs, d.attackMs, 0.001f);
            expectWithinAbsoluteError (c.releaseMs, d.releaseMs, 0.01f);
        }

        beginTest ("Each mode remembers its own settings");
        {
            auto p = makeProcessor();
            setParameter (*p, LuckyCompProcessor::thresholdId, -33.0f);
            setParameter (*p, LuckyCompProcessor::attackId, 2.0f);
            p->switchMode (Mode::fet);
            expect (p->getMode() == Mode::fet);
            expectWithinAbsoluteError (p->getControls().thresholdDb, lucky::modes::defaults (Mode::fet).thresholdDb, 0.01f);
            expectWithinAbsoluteError (p->getControls().attackMs, lucky::modes::defaults (Mode::fet).attackMs, 0.001f);

            setParameter (*p, LuckyCompProcessor::thresholdId, -12.0f);
            setParameter (*p, LuckyCompProcessor::colourId, 400.0f);
            p->switchMode (Mode::opto);
            expectWithinAbsoluteError (p->getColour().amount, 1.0f, 0.001f);
            p->switchMode (Mode::clean);
            expectWithinAbsoluteError (p->getControls().thresholdDb, -33.0f, 0.01f);
            expectWithinAbsoluteError (p->getControls().attackMs, 2.0f, 0.01f);
            p->switchMode (Mode::fet);
            expectWithinAbsoluteError (p->getControls().thresholdDb, -12.0f, 0.01f);
            expectWithinAbsoluteError (p->getColour().amount, 4.0f, 0.01f);
        }

        beginTest ("State round-trips the parameters and every mode's memory");
        {
            auto p = makeProcessor();
            setParameter (*p, LuckyCompProcessor::thresholdId, -40.0f);
            p->switchMode (Mode::opto);
            setParameter (*p, LuckyCompProcessor::colourId, 300.0f);
            p->switchMode (Mode::vca);
            setParameter (*p, LuckyCompProcessor::autoReleaseId, 1.0f);
            setParameter (*p, LuckyCompProcessor::matchId, 0.0f);
            juce::MemoryBlock state;
            p->getStateInformation (state);

            auto q = makeProcessor();
            q->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
            expect (q->getMode() == Mode::vca);
            expect (q->getControls().autoRelease);
            expect (q->getParameters().getRawParameterValue (LuckyCompProcessor::matchId)->load() < 0.5f);
            q->switchMode (Mode::opto);
            expectWithinAbsoluteError (q->getColour().amount, 3.0f, 0.01f);
            q->switchMode (Mode::clean);
            expectWithinAbsoluteError (q->getControls().thresholdDb, -40.0f, 0.01f);
        }

        beginTest ("Level match answers a settings change straight away");
        {
            auto p = makeProcessor();
            setParameter (*p, LuckyCompProcessor::thresholdId, -24.0f);
            run (*p, quietLoop());
            p->updateLevelMatch();
            const auto before = totalGain (*p);
            setParameter (*p, LuckyCompProcessor::thresholdId, -40.0f);
            p->updateLevelMatch();
            const auto after = totalGain (*p);
            expect (before > 1.0f);
            expect (after > before * 1.2f, juce::String (before) + " -> " + juce::String (after));
        }

        beginTest ("A reopened session starts at its saved level-match gain, with no dip");
        {
            auto p = makeProcessor();
            setParameter (*p, LuckyCompProcessor::thresholdId, -24.0f);
            run (*p, quietLoop());
            p->updateLevelMatch();
            const auto matched = totalGain (*p);
            expect (matched > 1.2f, "the loop needs make-up: " + juce::String (matched));
            juce::MemoryBlock state;
            p->getStateInformation (state);

            auto q = makeProcessor();
            q->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
            expectWithinAbsoluteError (totalGain (*q), matched, 1.0e-3f);
            expectWithinAbsoluteError (q->getEngine().getUncompressedTarget(), p->getEngine().getUncompressedTarget(), 1.0e-3f);
            juce::AudioBuffer<float> firstBlock (2, blockSize);
            firstBlock.clear();
            run (*q, firstBlock);
            expectWithinAbsoluteError (q->getEngine().getMakeupDb(), juce::Decibels::gainToDecibels (q->getEngine().getMakeupTarget()), 0.01f);
        }

        beginTest ("The saved gain holds while the history refills, then updates once");
        {
            auto p = makeProcessor();
            setParameter (*p, LuckyCompProcessor::thresholdId, -24.0f);
            const auto loop = quietLoop();
            run (*p, loop);
            p->updateLevelMatch();
            const auto matched = totalGain (*p);

            p->prepareToPlay (rate, blockSize);
            expectWithinAbsoluteError (totalGain (*p), matched, 1.0e-6f);

            juce::AudioBuffer<float> start (2, static_cast<int> (rate * 0.5));
            for (int ch = 0; ch < 2; ++ch)
                start.copyFrom (ch, 0, loop, ch, 0, start.getNumSamples());
            run (*p, start);
            p->updateLevelMatch();
            expectWithinAbsoluteError (totalGain (*p), matched, 1.0e-6f);

            run (*p, loop);
            p->updateLevelMatch();
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (totalGain (*p)),
                                       juce::Decibels::gainToDecibels (matched), 0.5f);
        }

        beginTest ("A changed setting is matched at once, even while a saved gain is held");
        {
            auto p = makeProcessor();
            setParameter (*p, LuckyCompProcessor::thresholdId, -24.0f);
            run (*p, quietLoop());
            p->updateLevelMatch();
            const auto before = totalGain (*p);
            p->prepareToPlay (rate, blockSize);
            juce::AudioBuffer<float> start (quietLoop());
            start.setSize (2, static_cast<int> (rate * 0.5), true);
            run (*p, start);
            setParameter (*p, LuckyCompProcessor::thresholdId, -40.0f);
            p->updateLevelMatch();
            expect (totalGain (*p) > before * 1.2f);
        }

        beginTest ("The test snare sounds different when comparing (Clean, threshold -44 dB)");
        {
            const auto playHit = [] (bool bypassed)
            {
                auto p = makeProcessor();
                setParameter (*p, LuckyCompProcessor::thresholdId, -44.0f);
                setParameter (*p, LuckyCompProcessor::bypassId, bypassed ? 1.0f : 0.0f);
                juce::AudioBuffer<float> silence (2, blockSize);
                silence.clear();
                run (*p, silence);
                p->playTestHit();
                juce::AudioBuffer<float> out (2, static_cast<int> (rate));
                out.clear();
                return run (*p, out);
            };
            const auto compressed = playHit (false), raw = playHit (true);
            const auto peak = [] (const juce::AudioBuffer<float>& b) { return b.getMagnitude (0, 0, b.getNumSamples()); };
            const auto tail = [] (const juce::AudioBuffer<float>& b) { return b.getRMSLevel (0, static_cast<int> (rate * 0.2), static_cast<int> (rate * 0.2)); };
            logMessage ("peak " + juce::String (peak (compressed)) + " vs " + juce::String (peak (raw)) + ", tail "
                        + juce::String (tail (compressed)) + " vs " + juce::String (tail (raw)));
            expect (raw.getMagnitude (0, raw.getNumSamples()) > 0.1f, "the raw hit plays");
            expect (std::abs (juce::Decibels::gainToDecibels (tail (compressed) / peak (compressed))
                              - juce::Decibels::gainToDecibels (tail (raw) / peak (raw))) > 3.0f,
                    "compression changes the balance of crack and tail");
        }

        beginTest ("Level match doesn't predict while it's off");
        {
            auto p = makeProcessor();
            p->setNonRealtime (true);
            setParameter (*p, LuckyCompProcessor::matchId, 0.0f);
            setParameter (*p, LuckyCompProcessor::thresholdId, -30.0f);
            run (*p, quietLoop());
            expectEquals (totalGain (*p), 1.0f);
            setParameter (*p, LuckyCompProcessor::matchId, 1.0f);
            juce::AudioBuffer<float> moment (quietLoop());
            moment.setSize (2, static_cast<int> (rate * 0.1), true);
            run (*p, moment);
            expect (totalGain (*p) > 1.5f, "matches once turned back on: " + juce::String (totalGain (*p)));
        }

        beginTest ("Offline, level match predicts at most every 50 ms of audio while a setting moves");
        {
            auto p = makeProcessor();
            p->setNonRealtime (true);
            setParameter (*p, LuckyCompProcessor::thresholdId, -20.0f);
            run (*p, quietLoop());
            const auto loop = quietLoop();
            constexpr int smallBlock = 64;
            juce::MidiBuffer midi;
            auto changes = 0;
            auto last = totalGain (*p);
            const auto seconds = 1.0;
            for (int start = 0, i = 0; start + smallBlock <= static_cast<int> (seconds * rate); start += smallBlock, ++i)
            {
                setParameter (*p, LuckyCompProcessor::thresholdId, -20.0f - static_cast<float> (i % 20));
                juce::AudioBuffer<float> block (2, smallBlock);
                for (int ch = 0; ch < 2; ++ch)
                    block.copyFrom (ch, 0, loop, ch, start, smallBlock);
                p->processBlock (block, midi);
                if (! juce::approximatelyEqual (totalGain (*p), last))
                {
                    ++changes;
                    last = totalGain (*p);
                }
            }
            logMessage ("predictions in 1 s of audio: " + juce::String (changes));
            expect (changes >= 10 && changes <= static_cast<int> (seconds / LuckyCompProcessor::matchIntervalSeconds) + 1);
        }

        beginTest ("After reopening a session, the dials still compress");
        {
            auto p = makeProcessor();
            run (*p, lucky::test::drumLoop (rate));
            p->updateLevelMatch();
            juce::MemoryBlock state;
            p->getStateInformation (state);

            auto q = makeProcessor();
            q->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
            q->prepareToPlay (rate, blockSize);
            setParameter (*q, LuckyCompProcessor::matchId, 0.0f);
            const auto loop = lucky::test::drumLoop (rate);
            const auto n = loop.getNumSamples();
            setParameter (*q, LuckyCompProcessor::thresholdId, 0.0f);
            const auto gentle = lucky::test::energy (run (*q, loop), 0, n);
            setParameter (*q, LuckyCompProcessor::thresholdId, -44.0f);
            const auto heavy = lucky::test::energy (run (*q, loop), 0, n);
            logMessage ("energy at 0 dB " + juce::String (gentle) + ", at -44 dB " + juce::String (heavy));
            expect (heavy < gentle * 0.25, "a lower threshold compresses harder after a reload");
        }

        beginTest ("With level match, Mix sounds even: 50% sits between dry and fully compressed");
        {
            const auto loop = lucky::test::drumLoop (rate, 2);
            const auto crestAt = [&loop] (float mixPercent)
            {
                auto p = makeProcessor();
                setParameter (*p, LuckyCompProcessor::thresholdId, -44.0f);
                setParameter (*p, LuckyCompProcessor::mixId, mixPercent);
                run (*p, loop);
                p->updateLevelMatch();
                juce::AudioBuffer<float> settle (2, static_cast<int> (rate * 0.2));
                settle.clear();
                run (*p, settle);
                const auto out = run (*p, loop);
                const auto n = out.getNumSamples();
                return juce::Decibels::gainToDecibels (out.getMagnitude (0, 0, n) / out.getRMSLevel (0, 0, n));
            };
            const auto dry = crestAt (0.0f), half = crestAt (50.0f), full = crestAt (100.0f);
            logMessage ("crest factor: dry " + juce::String (dry, 1) + " dB, 50% " + juce::String (half, 1) + " dB, 100% "
                        + juce::String (full, 1) + " dB");
            const auto position = (dry - half) / (dry - full);
            expect (position > 0.3f && position < 0.85f, "50% is " + juce::String (position * 100.0f, 0) + "% of the way to fully compressed");
        }

        beginTest ("Level match never pushes a peak over -1 dBFS, even on the first hit after a silence");
        {
            const auto loop = lucky::test::drumLoop (rate, 2);
            juce::AudioBuffer<float> silence (2, static_cast<int> (rate * 4.0));
            silence.clear();
            for (const auto mode : { Mode::clean, Mode::fet, Mode::opto, Mode::vca })
            {
                auto p = makeProcessor();
                p->switchMode (mode);
                setParameter (*p, LuckyCompProcessor::thresholdId, -40.0f);
                run (*p, loop);
                p->updateLevelMatch();
                run (*p, silence);
                const auto heard = static_cast<int> (lucky::LevelMatcher::historySeconds * rate);
                juce::AudioBuffer<float> again (2, heard);
                for (int ch = 0; ch < 2; ++ch)
                    again.copyFrom (ch, 0, loop, ch, loop.getNumSamples() - heard, heard);
                const auto out = run (*p, again);
                const auto peakDb = juce::Decibels::gainToDecibels (out.getMagnitude (0, out.getNumSamples()));
                logMessage (juce::String (lucky::modes::name (mode)) + ": loudest peak " + juce::String (peakDb, 2) + " dBFS");
                expect (peakDb < lucky::LevelMatcher::peakCeilingDb + 0.5f, juce::String (peakDb, 2) + " dBFS");
            }
        }

        beginTest ("When peaks hold the match down, the uncompressed side is turned down by as much");
        {
            auto p = makeProcessor();
            setParameter (*p, LuckyCompProcessor::thresholdId, -40.0f);
            run (*p, lucky::test::drumLoop (rate));
            p->updateLevelMatch();
            const auto& engine = p->getEngine();
            const auto full = engine.getMatcher().predict (p->getDetectorSettings()).value_or (0.0f);
            const auto held = engine.getWetMakeupTarget() * engine.getMakeupTarget();
            logMessage ("loudness match " + juce::String (juce::Decibels::gainToDecibels (full), 1) + " dB, held to "
                        + juce::String (juce::Decibels::gainToDecibels (held), 1) + " dB, uncompressed at "
                        + juce::String (juce::Decibels::gainToDecibels (engine.getUncompressedTarget()), 1) + " dB");
            expect (held < full * 0.9f, "a hot loop at -40 dB can't be matched under the ceiling");
            expectWithinAbsoluteError (engine.getUncompressedTarget(), held / full, 0.02f);
        }

        beginTest ("Comparing plays the input untouched when level match isn't held down by peaks");
        {
            auto p = makeProcessor();
            const auto loop = quietLoop();
            run (*p, loop);
            p->updateLevelMatch();
            expect (juce::exactlyEqual (p->getEngine().getUncompressedTarget(), 1.0f));
            setParameter (*p, LuckyCompProcessor::bypassId, 1.0f);
            juce::AudioBuffer<float> first (loop);
            run (*p, first);
            const auto out = run (*p, loop);
            auto identical = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < loop.getNumSamples(); ++i)
                    identical = identical && juce::exactlyEqual (out.getSample (ch, i), loop.getSample (ch, i));
            expect (identical);
        }

        beginTest ("Compresses: without level match the loop comes out quieter");
        {
            auto p = makeProcessor();
            setParameter (*p, LuckyCompProcessor::thresholdId, -24.0f);
            setParameter (*p, LuckyCompProcessor::matchId, 0.0f);
            const auto loop = lucky::test::drumLoop (rate);
            const auto out = run (*p, loop);
            const auto n = loop.getNumSamples();
            expect (lucky::test::energy (out, 0, n) < lucky::test::energy (loop, 0, n) * 0.5);
            expect (p->getEngine().getMeterPosition() > 0);
        }

        beginTest ("A damaged session can't set an extreme or broken level-match gain");
        {
            auto p = makeProcessor();
            juce::MemoryBlock state;
            p->getStateInformation (state);
            const auto withGains = [&state] (const juce::var& gainDb)
            {
                auto tree = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize())));
                tree.setProperty ("matchGainDb", gainDb, nullptr);
                tree.setProperty ("wetMatchGainDb", gainDb, nullptr);
                tree.setProperty ("uncompressedMatchGainDb", gainDb, nullptr);
                juce::MemoryBlock damaged;
                juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), damaged);
                return damaged;
            };

            const auto loud = withGains (200.0);
            auto q = makeProcessor();
            q->setStateInformation (loud.getData(), static_cast<int> (loud.getSize()));
            const auto maxGain = juce::Decibels::decibelsToGain (lucky::LevelMatcher::maxGainDb);
            expect (q->getEngine().getMakeupTarget() <= maxGain * 1.001f);
            expect (q->getEngine().getWetMakeupTarget() <= maxGain * 1.001f);
            expect (q->getEngine().getUncompressedTarget() <= maxGain * 1.001f);

            const auto broken = withGains (std::numeric_limits<double>::quiet_NaN());
            auto r = makeProcessor();
            r->setStateInformation (broken.getData(), static_cast<int> (broken.getSize()));
            expect (std::isfinite (r->getEngine().getMakeupTarget()));
            expect (std::isfinite (r->getEngine().getWetMakeupTarget()));
            expect (std::isfinite (r->getEngine().getUncompressedTarget()));
        }
    }
};

static ProcessorTests processorTests;

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("Lucky Comp Processor");
    for (int i = 0; i < runner.getNumResults(); ++i)
        if (runner.getResult (i)->failures > 0)
            return 1;
    return 0;
}

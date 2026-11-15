#include "PluginProcessor.h"
#include "TestSignals.h"
#include "dsp/ExampleHit.h"
#include <iostream>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

/** Measures Lucky Compressor's CPU cost: the audio thread per instance, in every mode and at several buffer sizes; the
    level-match prediction the background thread runs while a knob moves; and the Lab's simulation, which the editor
    reruns on every settings change.

    LuckyCompressorBench [seconds of audio per run, default 20] */
namespace
{
constexpr double rate = 48000.0;

void setParameter (LuckyCompressorProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getParameters().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

double secondsSince (double startMs) { return (juce::Time::getMillisecondCounterHiRes() - startMs) * 0.001; }

/** Fraction of one core used to process the audio in real time, and the slowest block against its deadline. */
std::pair<double, double> audioLoad (lucky::Mode mode, int blockSize, double seconds)
{
    std::unique_ptr<LuckyCompressorProcessor> processor (dynamic_cast<LuckyCompressorProcessor*> (createPluginFilter()));
    processor->setPlayConfigDetails (2, 2, rate, blockSize);
    processor->prepareToPlay (rate, blockSize);
    processor->switchMode (mode);
    setParameter (*processor, LuckyCompressorProcessor::thresholdId, -30.0f);
    setParameter (*processor, LuckyCompressorProcessor::mixId, 60.0f);

    const auto loop = lucky::test::drumLoop (rate);
    juce::AudioBuffer<float> block (2, blockSize);
    juce::MidiBuffer midi;
    const auto blocks = static_cast<int> (seconds * rate / blockSize);
    auto position = 0;
    auto busy = 0.0, slowest = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
                block.setSample (ch, i, loop.getSample (ch, (position + i) % loop.getNumSamples()));
        position += blockSize;
        if (b % 200 == 0)
            processor->updateLevelMatch();
        const auto start = juce::Time::getMillisecondCounterHiRes();
        processor->processBlock (block, midi);
        const auto took = secondsSince (start);
        busy += took;
        slowest = std::max (slowest, took);
    }
    return { busy / seconds, slowest / (blockSize / rate) };
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto seconds = argc > 1 ? juce::String (argv[1]).getDoubleValue() : 20.0;
    const char* names[] { "Clean", "FET", "Opto", "VCA" };

    std::cout << "Audio thread, one stereo instance at 48 kHz, Colour 1x, Mix 60%, level match on (% of one core; "
                 "slowest block as % of its deadline)\n";
    for (int m = 0; m < lucky::numModes; ++m)
    {
        std::cout << "  " << juce::String (names[m]).paddedRight (' ', 6);
        for (const auto blockSize : { 32, 64, 128, 512 })
        {
            const auto [load, slowest] = audioLoad (static_cast<lucky::Mode> (m), blockSize, seconds);
            std::cout << "  " << blockSize << ": " << juce::String (load * 100.0, 2) << "% (slowest " << juce::String (slowest * 100.0, 1) << "%)";
        }
        std::cout << "\n";
    }

    std::unique_ptr<LuckyCompressorProcessor> processor (dynamic_cast<LuckyCompressorProcessor*> (createPluginFilter()));
    processor->setPlayConfigDetails (2, 2, rate, 512);
    processor->prepareToPlay (rate, 512);
    const auto loop = lucky::test::drumLoop (rate);
    juce::AudioBuffer<float> audio (loop);
    juce::MidiBuffer midi;
    for (int start = 0; start + 512 <= audio.getNumSamples(); start += 512)
    {
        juce::AudioBuffer<float> block (audio.getArrayOfWritePointers(), 2, start, 512);
        processor->processBlock (block, midi);
    }
    constexpr auto predictions = 40;
    auto start = juce::Time::getMillisecondCounterHiRes();
    for (int i = 0; i < predictions; ++i)
    {
        setParameter (*processor, LuckyCompressorProcessor::thresholdId, -20.0f - static_cast<float> (i % 10));
        processor->updateLevelMatch();
    }
    const auto predictMs = secondsSince (start) * 1000.0 / predictions;
    const auto intervalMs = LuckyCompressorProcessor::matchIntervalSeconds * 1000.0;
    std::cout << "\nLevel match, one prediction with a full 3 s history: " << juce::String (predictMs, 2) << " ms (at most every "
              << juce::String (juce::roundToInt (intervalMs)) << " ms while a setting changes, so " << juce::String (juce::roundToInt (predictMs / (intervalMs + predictMs) * 100.0))
              << "% of a background core while dragging; none while Level Match is off)\n";

    constexpr auto simulations = 20;
    start = juce::Time::getMillisecondCounterHiRes();
    for (int i = 0; i < simulations; ++i)
        lucky::example_hit::simulate (lucky::modes::resolve (lucky::Mode::opto, { -20.0f - static_cast<float> (i), 3.0f, 10.0f, 500.0f, false }),
                                      lucky::example_hit::windowSeconds, lucky::example_hit::sampleRate, {}, { lucky::Mode::opto, 1.0f, -20.0f });
    const auto simulateMs = secondsSince (start) * 1000.0 / simulations;
    start = juce::Time::getMillisecondCounterHiRes();
    for (int i = 0; i < simulations; ++i)
        lucky::example_hit::renderHit (lucky::modes::resolve (lucky::Mode::fet, { -20.0f - static_cast<float> (i), 4.0f, 0.2f, 100.0f, false }), 44100.0, true);
    const auto renderMs = secondsSince (start) * 1000.0 / simulations;
    std::cout << "Lab, one simulation (message thread, on each settings change): " << juce::String (simulateMs, 2) << " ms\n";
    std::cout << "Test snare, one render at 44.1 kHz (message thread, on S): " << juce::String (renderMs, 2) << " ms\n";

    start = juce::Time::getMillisecondCounterHiRes();
    for (int i = 0; i < simulations; ++i)
    {
        const auto settings = lucky::modes::resolve (lucky::Mode::fet, { -20.0f - static_cast<float> (i), 4.0f, 0.2f, 100.0f, false });
        lucky::example_hit::simulate (settings, lucky::example_hit::windowSeconds, lucky::example_hit::sampleRate);
        lucky::example_hit::renderHit (settings, 44100.0, true);
    }
    const auto alternatingMs = secondsSince (start) * 1000.0 / simulations;
    std::cout << "Lab update then S in a 44.1 kHz session (message thread): " << juce::String (alternatingMs, 2) << " ms\n";
    processor->releaseResources();
    return 0;
}

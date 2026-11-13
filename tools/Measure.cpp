#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>
#include <iostream>
#include <numbers>

/** Measures a compressor, plugin or hardware, for Lucky Comp's colour and dynamics models. See docs/colour-plan.md.

    LuckyCompMeasure <plugin.vst3> params
    LuckyCompMeasure <plugin.vst3> [Name=value ...] <test> <out.csv>
    LuckyCompMeasure --devices
    LuckyCompMeasure --hardware "<device name>" <output channel> <input channel> <test> <out.csv>

    Plugin values are the parameter's own text (e.g. "Ratio=4") or a normalised 0-1 number written as "Name=@0.5".
    Hardware: the test signal plays from the output channel (1-based) and is recorded from the input channel; the
    round trip is aligned with a click before every run. Use a loopback cable first to measure the interface alone.
    Tests: response, harmonics100, harmonics1k, curve, timing. */
namespace
{
constexpr double rate = 48000.0;
constexpr int blockSize = 512;

std::unique_ptr<juce::AudioPluginInstance> load (juce::AudioPluginFormatManager& formats, const juce::String& path)
{
    juce::OwnedArray<juce::PluginDescription> descriptions;
    for (auto* format : formats.getFormats())
        format->findAllTypesForFile (descriptions, path);
    if (descriptions.isEmpty())
    {
        std::cerr << "no plugin found in " << path << "\n";
        return {};
    }

    juce::String error;
    auto plugin = formats.createPluginInstance (*descriptions[0], rate, blockSize, error);
    if (plugin == nullptr)
    {
        std::cerr << "load failed: " << error << "\n";
        return {};
    }

    plugin->setPlayConfigDetails (2, 2, rate, blockSize);
    plugin->prepareToPlay (rate, blockSize);
    return plugin;
}

/** Something to measure: plays a mono signal through it and returns the aligned output. */
using Target = std::function<std::vector<float> (const std::vector<float>&)>;

/** Plays a mono signal (duplicated to both channels) through the plugin and returns the left output, delayed by its
    latency. */
std::vector<float> process (juce::AudioPluginInstance& plugin, const std::vector<float>& input)
{
    std::vector<float> output (input.size());
    juce::AudioBuffer<float> block (std::max (2, plugin.getTotalNumInputChannels()), blockSize);
    juce::MidiBuffer midi;
    for (size_t start = 0; start < input.size(); start += blockSize)
    {
        const auto n = static_cast<int> (std::min<size_t> (blockSize, input.size() - start));
        block.clear();
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                block.setSample (ch, i, input[start + static_cast<size_t> (i)]);
        plugin.processBlock (block, midi);
        for (int i = 0; i < n; ++i)
            output[start + static_cast<size_t> (i)] = block.getSample (0, i);
    }
    return output;
}

juce::AudioProcessorParameter* find (juce::AudioPluginInstance& plugin, const juce::String& name)
{
    for (auto* p : plugin.getParameters())
        if (p->getName (128).equalsIgnoreCase (name))
            return p;
    return nullptr;
}

void listParameters (juce::AudioPluginInstance& plugin)
{
    for (auto* p : plugin.getParameters())
    {
        const auto original = p->getValue();
        std::cout << p->getParameterIndex() << "\t" << p->getName (128) << "\t[" << p->getCurrentValueAsText() << "]\tsteps "
                  << p->getNumSteps() << "\t";
        for (const auto v : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            p->setValueNotifyingHost (v);
            std::cout << v << "=" << p->getCurrentValueAsText() << "  ";
        }
        p->setValueNotifyingHost (original);
        std::cout << "\n";
    }
}

/** A sine of the given peak amplitude. */
std::vector<float> sine (double hz, float amplitude, double seconds)
{
    std::vector<float> s (static_cast<size_t> (seconds * rate));
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = amplitude * static_cast<float> (std::sin (2.0 * std::numbers::pi * hz * static_cast<double> (i) / rate));
    return s;
}

/** Amplitude (peak) of the component at hz over [from, to), by Goertzel; the range should span whole cycles. */
double amplitudeAt (const std::vector<float>& x, double hz, size_t from, size_t to)
{
    const auto w = 2.0 * std::numbers::pi * hz / rate;
    auto re = 0.0, im = 0.0;
    for (auto i = from; i < to; ++i)
    {
        re += x[i] * std::cos (w * static_cast<double> (i));
        im += x[i] * std::sin (w * static_cast<double> (i));
    }
    return 2.0 * std::sqrt (re * re + im * im) / static_cast<double> (to - from);
}

/** A window of the last part of a buffer that holds a whole number of cycles of hz. */
std::pair<size_t, size_t> steadyWindow (const std::vector<float>& x, double hz, double seconds)
{
    const auto period = rate / hz;
    const auto cycles = std::max (1.0, std::floor (seconds * rate / period));
    const auto length = static_cast<size_t> (std::round (cycles * period));
    return { x.size() - length, x.size() };
}

double toDb (double gain)
{
    return 20.0 * std::log10 (std::max (gain, 1.0e-12));
}

/** Some plugins output silence or garbage for their first blocks; run a little audio through first. */
void warmUp (juce::AudioPluginInstance& plugin)
{
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        process (plugin, sine (1000.0, 0.01f, 0.25));
        juce::Thread::sleep (300);
    }
    process (plugin, std::vector<float> (static_cast<size_t> (rate), 0.0f));
}

const std::vector<double>& thirdOctaves()
{
    static const std::vector<double> f { 20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800,
                                         1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000 };
    return f;
}

/** Gain at each third-octave frequency for a quiet sine (-40 dBFS peak). */
juce::String measureResponse (const Target& process)
{
    juce::String csv ("hz,gain_db\n");
    constexpr auto amplitude = 0.01f;
    for (const auto hz : thirdOctaves())
    {
        const auto out = process (sine (hz, amplitude, hz < 60.0 ? 1.0 : 0.5));
        const auto [from, to] = steadyWindow (out, hz, 0.2);
        csv << hz << "," << juce::String (toDb (amplitudeAt (out, hz, from, to) / amplitude), 3) << "\n";
    }
    return csv;
}

/** Harmonics H2-H5 (dB relative to the fundamental) and gain for sines from -40 to 0 dBFS peak in 3 dB steps. */
juce::String measureHarmonics (const Target& process, double hz)
{
    juce::String csv ("in_db,out_db,gain_db,h2_dbc,h3_dbc,h4_dbc,h5_dbc\n");
    for (auto db = -40.0; db <= 0.01; db += 3.0)
    {
        const auto amplitude = static_cast<float> (std::pow (10.0, db / 20.0));
        const auto out = process (sine (hz, amplitude, 1.5));
        const auto [from, to] = steadyWindow (out, hz, 0.5);
        const auto fundamental = amplitudeAt (out, hz, from, to);
        csv << db << "," << juce::String (toDb (fundamental), 2) << "," << juce::String (toDb (fundamental / amplitude), 2);
        for (int h = 2; h <= 5; ++h)
            csv << "," << juce::String (toDb (amplitudeAt (out, hz * h, from, to) / fundamental), 2);
        csv << "\n";
        process (std::vector<float> (static_cast<size_t> (rate * 0.5), 0.0f));
    }
    return csv;
}

/** Steady-state output level of a 1 kHz sine from -50 to 0 dBFS peak in 2 dB steps, rising then falling. */
juce::String measureCurve (const Target& process)
{
    juce::String csv ("in_db,out_db\n");
    for (auto db = -50.0; db <= 0.01; db += 2.0)
    {
        const auto amplitude = static_cast<float> (std::pow (10.0, db / 20.0));
        const auto out = process (sine (1000.0, amplitude, 1.0));
        const auto [from, to] = steadyWindow (out, 1000.0, 0.2);
        csv << db << "," << juce::String (toDb (amplitudeAt (out, 1000.0, from, to)), 2) << "\n";
    }
    return csv;
}

/** Steps a 6 kHz carrier -30 -> -6 -> -30 dBFS and follows the gain cycle by cycle (0.17 ms). Reports the times
    for the gain reduction to cover 50, 63 and 90% of each change, and writes the envelope beside the csv. */
juce::String measureTiming (const Target& process, size_t latency, const juce::File& envelopeFile)
{
    constexpr auto hz = 6000.0;
    constexpr auto low = 0.0316f, high = 0.5f;
    const auto quiet = sine (hz, low, 2.0), loud = sine (hz, high, 2.0), after = sine (hz, low, 6.0);
    std::vector<float> input;
    for (const auto* part : { &quiet, &loud, &after })
        input.insert (input.end(), part->begin(), part->end());
    const auto out = process (input);

    const auto period = static_cast<size_t> (rate / hz);
    std::vector<double> reductionDb;
    std::vector<double> times;
    for (size_t start = 0; start + period + latency < out.size(); start += period)
    {
        auto peakOut = 0.0f, peakIn = 0.0f;
        for (size_t i = 0; i < period; ++i)
        {
            peakOut = std::max (peakOut, std::abs (out[start + latency + i]));
            peakIn = std::max (peakIn, std::abs (input[start + i]));
        }
        reductionDb.push_back (toDb (peakIn) - toDb (peakOut));
        times.push_back (static_cast<double> (start) / rate);
    }

    juce::String envelope ("t_s,reduction_db\n");
    for (size_t i = 0; i < times.size(); ++i)
        envelope << juce::String (times[i], 5) << "," << juce::String (reductionDb[i], 3) << "\n";
    envelopeFile.replaceWithText (envelope);

    const auto indexAt = [&] (double t) { return static_cast<size_t> (t * rate / static_cast<double> (period)); };
    const auto average = [&] (double from, double to)
    {
        auto sum = 0.0;
        for (auto i = indexAt (from); i < indexAt (to); ++i)
            sum += reductionDb[i];
        return sum / static_cast<double> (indexAt (to) - indexAt (from));
    };
    const auto base = average (1.5, 2.0), held = average (3.5, 4.0), rest = average (9.5, 10.0);
    const auto crossing = [&] (double from, double startDb, double endDb, double fraction)
    {
        const auto target = startDb + fraction * (endDb - startDb);
        for (auto i = indexAt (from); i < reductionDb.size(); ++i)
            if ((endDb > startDb) ? reductionDb[i] >= target : reductionDb[i] <= target)
                return (times[i] - from) * 1000.0;
        return -1.0;
    };

    juce::String csv ("phase,start_db,end_db,t50_ms,t63_ms,t90_ms\n");
    csv << "attack," << juce::String (base, 2) << "," << juce::String (held, 2);
    for (const auto f : { 0.5, 0.632, 0.9 })
        csv << "," << juce::String (crossing (2.0, base, held, f), 3);
    csv << "\nrelease," << juce::String (held, 2) << "," << juce::String (rest, 2);
    for (const auto f : { 0.5, 0.632, 0.9 })
        csv << "," << juce::String (crossing (4.0, held, rest, f), 3);
    csv << "\n";
    return csv;
}

/** Plays signals out of one channel of an audio interface and records them back from another, in real time. */
class HardwareLink final : private juce::AudioIODeviceCallback
{
public:
    HardwareLink (const juce::String& deviceName, int outputChannel, int inputChannel)
        : output (outputChannel - 1), input (inputChannel - 1)
    {
        manager.initialiseWithDefaultDevices (0, 0);
        auto setup = manager.getAudioDeviceSetup();
        setup.outputDeviceName = setup.inputDeviceName = deviceName;
        setup.sampleRate = rate;
        setup.bufferSize = blockSize;
        setup.useDefaultInputChannels = setup.useDefaultOutputChannels = false;
        setup.inputChannels.clear();
        setup.outputChannels.clear();
        setup.inputChannels.setBit (input);
        setup.outputChannels.setBit (output);
        error = manager.setAudioDeviceSetup (setup, true);
        if (error.isEmpty() && manager.getCurrentAudioDevice() != nullptr
            && ! juce::approximatelyEqual (manager.getCurrentAudioDevice()->getCurrentSampleRate(), rate))
            error = "the device runs at " + juce::String (manager.getCurrentAudioDevice()->getCurrentSampleRate()) + " Hz; set it to 48 kHz";
        if (error.isEmpty())
            manager.addAudioCallback (this);
    }

    ~HardwareLink() override { manager.removeAudioCallback (this); }

    juce::String getError() const { return error; }

    /** The loudest sample recorded so far, alignment click included, in dBFS. */
    double getLoudestDb() const { return toDb (loudestRecorded); }

    /** Finds the round-trip delay with a click; call before measuring. */
    bool align()
    {
        std::vector<float> click (static_cast<size_t> (rate), 0.0f);
        const auto at = static_cast<size_t> (rate * 0.1);
        click[at] = 0.5f;
        const auto heard = playAndRecord (click);
        const auto loudest = std::max_element (heard.begin(), heard.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); });
        if (std::abs (*loudest) < 0.001f)
        {
            error = "no click came back: check the cables, the channel numbers and the levels";
            return false;
        }
        offset = static_cast<size_t> (loudest - heard.begin()) - at;
        std::cerr << "round trip " << offset << " samples, click came back at " << 20.0 * std::log10 (std::abs (*loudest) / 0.5) << " dB\n";
        return true;
    }

    /** Plays a signal and returns what came back, aligned to it. */
    std::vector<float> process (const std::vector<float>& signal)
    {
        auto heard = playAndRecord (signal);
        std::vector<float> aligned (signal.size(), 0.0f);
        for (size_t i = 0; i < aligned.size() && i + offset < heard.size(); ++i)
            aligned[i] = heard[i + offset];
        return aligned;
    }

private:
    std::vector<float> playAndRecord (const std::vector<float>& signal)
    {
        const auto padding = static_cast<size_t> (rate * 0.25);
        playing = signal;
        playing.resize (signal.size() + padding, 0.0f);
        recorded.assign (playing.size(), 0.0f);
        position.store (0);
        running.store (true);
        while (running.load())
            juce::Thread::sleep (5);
        for (const auto x : recorded)
            loudestRecorded = std::max (loudestRecorded, static_cast<double> (std::abs (x)));
        return recorded;
    }

    void audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs, float* const* outputs, int numOutputs,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override
    {
        for (int ch = 0; ch < numOutputs; ++ch)
            if (outputs[ch] != nullptr)
                juce::FloatVectorOperations::clear (outputs[ch], numSamples);
        if (! running.load())
            return;

        auto pos = position.load();
        for (int i = 0; i < numSamples && pos < playing.size(); ++i, ++pos)
        {
            if (numOutputs > 0 && outputs[0] != nullptr)
                outputs[0][i] = playing[pos];
            if (numInputs > 0 && inputs[0] != nullptr)
                recorded[pos] = inputs[0][i];
        }
        position.store (pos);
        if (pos >= playing.size())
            running.store (false);
    }

    void audioDeviceAboutToStart (juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}

    juce::AudioDeviceManager manager;
    const int output, input;
    juce::String error;
    std::vector<float> playing, recorded;
    std::atomic<size_t> position { 0 };
    std::atomic<bool> running { false };
    size_t offset = 0;
    double loudestRecorded = 0.0;
};

juce::String runTest (const juce::String& test, const Target& target, size_t latency, const juce::File& output)
{
    if (test == "response")
        return measureResponse (target);
    if (test == "harmonics100" || test == "harmonics1k")
        return measureHarmonics (target, test == "harmonics100" ? 100.0 : 1000.0);
    if (test == "curve")
        return measureCurve (target);
    if (test == "timing")
        return measureTiming (target, latency, output.withFileExtension ("envelope.csv"));
    return {};
}

bool applySetting (juce::AudioPluginInstance& plugin, const juce::String& setting)
{
    const auto name = setting.upToFirstOccurrenceOf ("=", false, false);
    const auto value = setting.fromFirstOccurrenceOf ("=", false, false);
    auto* parameter = find (plugin, name);
    if (parameter == nullptr)
    {
        std::cerr << "no parameter named " << name << "\n";
        return false;
    }
    parameter->setValueNotifyingHost (value.startsWith ("@") ? value.substring (1).getFloatValue()
                                                             : parameter->getValueForText (value));
    std::cerr << name << " -> " << parameter->getCurrentValueAsText() << "\n";
    return true;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 3 && ! (argc == 2 && juce::String (argv[1]) == "--devices"))
    {
        std::cerr << "usage: LuckyCompMeasure <plugin.vst3> params | [Name=value ...] <test> <out.csv> | --devices | --hardware\n";
        return 1;
    }

    juce::ScopedJuceInitialiser_GUI init;

    if (juce::String (argv[1]) == "--devices")
    {
        juce::AudioDeviceManager manager;
        juce::OwnedArray<juce::AudioIODeviceType> types;
        manager.createAudioDeviceTypes (types);
        for (auto* type : types)
        {
            type->scanForDevices();
            for (const auto& name : type->getDeviceNames (true))
                if (auto device = std::unique_ptr<juce::AudioIODevice> (type->createDevice (name, name)))
                    std::cout << type->getTypeName() << "\t" << name << "\tins " << device->getInputChannelNames().size()
                              << "\touts " << device->getOutputChannelNames().size() << "\n";
        }
        return 0;
    }

    if (juce::String (argv[1]) == "--hardware")
    {
        if (argc != 7)
        {
            std::cerr << "usage: LuckyCompMeasure --hardware \"<device>\" <output channel> <input channel> <test> <out.csv>\n";
            return 1;
        }
        HardwareLink link (argv[2], juce::String (argv[3]).getIntValue(), juce::String (argv[4]).getIntValue());
        if (link.getError().isEmpty())
            link.align();
        if (link.getError().isNotEmpty())
        {
            std::cerr << link.getError() << "\n";
            return 1;
        }
        const juce::File output (juce::File::getCurrentWorkingDirectory().getChildFile (argv[6]));
        output.getParentDirectory().createDirectory();
        const auto csv = runTest (argv[5], [&link] (const std::vector<float>& x) { return link.process (x); }, 0, output);
        if (csv.isEmpty())
        {
            std::cerr << "unknown test " << argv[5] << "\n";
            return 1;
        }
        output.replaceWithText (csv);
        std::cout << csv;
        const auto loudestDb = link.getLoudestDb();
        std::cerr << "loudest sample recorded: " << juce::String (loudestDb, 2) << " dBFS\n";
        if (loudestDb > -0.1)
            std::cerr << "WARNING: the input clipped; check the step onsets and the click before trusting the top steps\n";
        return 0;
    }

    juce::AudioPluginFormatManager formats;
    juce::addDefaultFormatsToManager (formats);
    auto plugin = load (formats, argv[1]);
    if (plugin == nullptr)
        return 1;

    std::cerr << "loaded " << plugin->getName() << ", latency " << plugin->getLatencySamples() << " samples\n";

    if (juce::String (argv[2]) == "params")
    {
        listParameters (*plugin);
        return 0;
    }

    auto argument = 2;
    while (argument < argc - 2 && juce::String (argv[argument]).contains ("="))
        if (! applySetting (*plugin, argv[argument++]))
            return 1;
    if (argc - argument != 2)
    {
        std::cerr << "expected <test> <out.csv> after the settings\n";
        return 1;
    }

    const juce::String test (argv[argument]);
    const juce::File output (juce::File::getCurrentWorkingDirectory().getChildFile (argv[argument + 1]));
    output.getParentDirectory().createDirectory();
    warmUp (*plugin);
    const auto csv = runTest (test, [&plugin] (const std::vector<float>& x) { return process (*plugin, x); },
                              static_cast<size_t> (plugin->getLatencySamples()), output);
    if (csv.isEmpty())
    {
        std::cerr << "unknown test " << test << "\n";
        return 1;
    }

    output.replaceWithText (csv);
    std::cout << csv;
    return 0;
}

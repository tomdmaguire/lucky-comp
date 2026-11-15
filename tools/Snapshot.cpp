#include "PluginEditor.h"
#include "ui/RevealPrompt.h"
#include "PluginProcessor.h"
#include "TestSignals.h"
#include <iostream>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
constexpr double rate = 48000.0;
constexpr int blockSize = 512;

void setParameter (LuckyCompressorProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getParameters().getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

/** Plays a few seconds of the drum loop through the processor so the live view has something to show. */
void play (LuckyCompressorProcessor& processor, double seconds)
{
    auto loop = lucky::test::drumLoop (rate, static_cast<int> (std::ceil (seconds / 3.33)));
    juce::MidiBuffer midi;
    for (int start = 0; start < loop.getNumSamples(); start += blockSize)
    {
        juce::AudioBuffer<float> block (loop.getArrayOfWritePointers(), 2, start, std::min (blockSize, loop.getNumSamples() - start));
        processor.processBlock (block, midi);
    }
}
} // namespace

/** Renders the editor to a PNG: LuckyCompressorSnapshot <clean|fet|opto|vca|vca-auto|empty|notips|gr|ask|fet-colour|opto-colour|vca-colour|vca-colour10|nolab|parallel|blend|fet-compare|new> <output.png> */
int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: LuckyCompressorSnapshot <clean|fet|opto|vca|vca-auto|empty|notips|gr|ask|fet-colour|opto-colour|vca-colour|vca-colour10|nolab|parallel|blend|fet-compare|new> <output.png>\n";
        return 1;
    }

    juce::ScopedJuceInitialiser_GUI init;
    const juce::String scene (argv[1]);
    const juce::File output (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));

    std::unique_ptr<LuckyCompressorProcessor> processor (dynamic_cast<LuckyCompressorProcessor*> (createPluginFilter()));
    processor->setPlayConfigDetails (2, 2, rate, blockSize);
    processor->prepareToPlay (rate, blockSize);

    if (scene.startsWith ("fet"))
        processor->switchMode (lucky::Mode::fet);
    else if (scene.startsWith ("opto"))
        processor->switchMode (lucky::Mode::opto);
    else if (scene.startsWith ("vca"))
        processor->switchMode (lucky::Mode::vca);
    if (! scene.startsWith ("new"))
    {
        const std::array<float, lucky::numModes> working { -24.0f, -20.0f, -20.0f, -18.0f };
        setParameter (*processor, LuckyCompressorProcessor::thresholdId, working[static_cast<size_t> (processor->getMode())]);
    }
    if (scene == "vca-auto")
        setParameter (*processor, LuckyCompressorProcessor::autoReleaseId, 1.0f);
    if (scene == "parallel")
        setParameter (*processor, LuckyCompressorProcessor::mixId, 50.0f);
    if (scene == "fet-colour" || scene == "opto-colour")
        setParameter (*processor, LuckyCompressorProcessor::colourId, 500.0f);
    if (scene == "vca-colour10")
        setParameter (*processor, LuckyCompressorProcessor::colourId, 1000.0f);
    if (scene == "blend")
        setParameter (*processor, LuckyCompressorProcessor::mixModeId, 1.0f);

    if (scene != "empty")
        play (*processor, 8.0);
    if (scene.endsWith ("compare"))
        setParameter (*processor, LuckyCompressorProcessor::bypassId, 1.0f);

    juce::SharedResourcePointer<lucky::Settings> settings;
    const auto tipsBefore = settings->getBool ("showTips", true);
    const auto reductionBefore = settings->getBool ("showGainReduction", false);
    const auto labBefore = settings->getBool ("showLab", true);
    settings->setBool ("showLab", scene != "nolab");
    settings->setBool ("showTips", scene != "notips");
    settings->setBool ("showGainReduction", scene == "gr");

    auto editor = std::make_unique<LuckyCompressorEditor> (*processor);
    editor->setSize (LuckyCompressorView::baseWidth, LuckyCompressorView::baseHeight);
    editor->getView().sync();
    std::unique_ptr<lucky::RevealPrompt> prompt;
    std::unique_ptr<juce::CallOutBox> callout;
    if (scene.contains ("-colour"))
        editor->getView().revealColour();
    if (scene == "ask")
    {
        prompt = std::make_unique<lucky::RevealPrompt> (lucky::theme::modeColour (processor->getMode()), nullptr, nullptr);
        callout = std::make_unique<juce::CallOutBox> (*prompt, editor->getView().getReductionButtonArea(), &editor->getView());
    }
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    output.deleteFile();
    juce::FileOutputStream stream (output);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    callout.reset();
    prompt.reset();
    processor->editorBeingDeleted (editor.get());
    editor.reset();
    settings->setBool ("showTips", tipsBefore);
    settings->setBool ("showGainReduction", reductionBefore);
    settings->setBool ("showLab", labBefore);
    std::cout << "wrote " << output.getFullPathName() << "\n";
    return 0;
}

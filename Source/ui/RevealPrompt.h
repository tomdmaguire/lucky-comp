#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace lucky
{

/** Asks before showing the gain-reduction number, recommending to keep it hidden and listen instead. Shown in a
    callout from the GR button. "Keep it hidden" is the default: Return picks it, and Escape or clicking away
    closes the callout without changing anything. */
class RevealPrompt final : public juce::Component
{
public:
    static constexpr int width = 330;

    /** onChoice gets whether to show the number and whether "Don't ask again" was ticked; onClose runs however
        the callout closes. */
    RevealPrompt (juce::Colour accent, std::function<void (bool show, bool dontAskAgain)> onChoice, std::function<void()> onClose);
    ~RevealPrompt() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void parentHierarchyChanged() override;

private:
    static juce::TextLayout layout (const juce::String&, juce::FontOptions, juce::Colour, float width);
    void choose (bool show);

    juce::TextLayout title, body;
    juce::TextButton showButton { "Show GR" }, hideButton { "Keep it hidden" };
    juce::ToggleButton dontAskAgain { "Don't ask again" };
    std::function<void (bool, bool)> onChoice;
    std::function<void()> onClose;
};

} // namespace lucky

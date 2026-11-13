#include "RevealPrompt.h"
#include "ModeColours.h"

namespace lucky
{
namespace
{
constexpr int padding = 16, gap = 10, buttonHeight = 28, checkHeight = 22;
} // namespace

juce::TextLayout RevealPrompt::layout (const juce::String& text, juce::FontOptions font, juce::Colour colour, float layoutWidth)
{
    juce::AttributedString string;
    string.append (text, juce::Font (font), colour);
    string.setLineSpacing (3.0f);
    juce::TextLayout result;
    result.createLayout (string, layoutWidth);
    return result;
}

RevealPrompt::RevealPrompt (juce::Colour accent, std::function<void (bool, bool)> choice, std::function<void()> close)
    : onChoice (std::move (choice)), onClose (std::move (close))
{
    const auto textWidth = static_cast<float> (width - 2 * padding);
    title = layout ("Show the gain-reduction number?", theme::labelFont (15.0f).withKerningFactor (0.0f), theme::text, textWidth);
    body = layout (juce::String (juce::CharPointer_UTF8 ("We recommend keeping it hidden for now. A number on screen pulls your eyes off the sound: you start "
                   "compressing to hit \"\xe2\x88\x92" "3 dB\" instead of listening for when the drum starts losing its punch. "
                   "Try guessing by ear first, then check.")),
                   juce::FontOptions (13.5f), theme::textDim, textWidth);

    for (auto* button : { &showButton, &hideButton })
    {
        button->getProperties().set ("outline", true);
        button->setColour (juce::Slider::rotarySliderFillColourId, accent);
        button->setWantsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }
    hideButton.getProperties().set ("primary", true);
    showButton.onClick = [this] { choose (true); };
    hideButton.onClick = [this] { choose (false); };

    dontAskAgain.setColour (juce::ToggleButton::textColourId, theme::textDim);
    dontAskAgain.setColour (juce::ToggleButton::tickColourId, accent);
    dontAskAgain.setColour (juce::ToggleButton::tickDisabledColourId, theme::textDim);
    dontAskAgain.setWantsKeyboardFocus (false);
    addAndMakeVisible (dontAskAgain);

    setWantsKeyboardFocus (true);
    const auto height = padding + juce::roundToInt (title.getHeight()) + 6 + juce::roundToInt (body.getHeight()) + 14 + buttonHeight + gap
                      + checkHeight + padding - 4;
    setSize (width, height);
}

RevealPrompt::~RevealPrompt()
{
    if (onClose)
        onClose();
}

void RevealPrompt::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().reduced (padding).toFloat();
    title.draw (g, area.removeFromTop (title.getHeight()));
    area.removeFromTop (6.0f);
    body.draw (g, area.removeFromTop (body.getHeight()));
}

void RevealPrompt::resized()
{
    auto area = getLocalBounds().reduced (padding);
    area.removeFromTop (juce::roundToInt (title.getHeight()) + 6 + juce::roundToInt (body.getHeight()) + 14);
    auto buttons = area.removeFromTop (buttonHeight);
    hideButton.setBounds (buttons.removeFromRight (128));
    buttons.removeFromRight (gap);
    showButton.setBounds (buttons.removeFromRight (96));
    area.removeFromTop (gap);
    dontAskAgain.setBounds (area.removeFromTop (checkHeight).withWidth (160).translated (-4, 0));
}

bool RevealPrompt::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey)
    {
        choose (false);
        return true;
    }
    return false;
}

void RevealPrompt::parentHierarchyChanged()
{
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<RevealPrompt> (this)]
                                     {
                                         if (safe != nullptr && safe->isShowing())
                                             safe->grabKeyboardFocus();
                                     });
}

void RevealPrompt::choose (bool show)
{
    if (onChoice)
        onChoice (show, dontAskAgain.getToggleState());
    if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
        box->dismiss();
}

} // namespace lucky

#pragma once

#include "PluginProcessor.h"
#include "ui/HitView.h"
#include "ui/LiveView.h"
#include "ui/ModeColours.h"

/** The editor's content at its base size; the editor scales it to the window. */
class LuckyCompView final : public juce::Component,
                            private juce::Timer
{
public:
    static constexpr int baseWidth = 900;
    static constexpr int baseHeight = 600;

    explicit LuckyCompView (LuckyCompProcessor&);

    /** Adds the editor's Window size items to the ••• menu. */
    std::function<void (juce::PopupMenu&)> addWindowSizeMenu;

    /** Brings everything up to date with the processor now, instead of on the next timer tick. */
    void sync();

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

    /** The GR button's area in this view, where the prompt that asks before showing the number points. */
    juce::Rectangle<int> getReductionButtonArea() const { return getLocalArea (&liveView, liveView.getEyeBounds()); }

    /** Shows the Lab's harmonics readout, as turning the Colour knob does. */
    void revealColour() { hitView.revealColour(); }

private:
    enum Control
    {
        amount,
        ratio,
        attack,
        release,
        colour,
        mix,
        numControls
    };

    /** A mode tab: a dot in the mode's colour, the name, and an underline when selected. */
    struct ModeTab final : juce::Button
    {
        explicit ModeTab (lucky::Mode);
        /** Greys the dot and underline while comparing with uncompressed: the mode is still chosen, just not heard. */
        bool comparing = false;
        void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
        const lucky::Mode mode;
    };

    void timerCallback() override;
    void applyMode (lucky::Mode);
    void setParameter (const juce::String& id, float value);
    void setComparing (bool);
    void toggleComparing() { setComparing (! comparing); }
    /** The mode's colour, or grey while comparing with uncompressed. */
    juce::Colour accent() const;
    void applyAccent();
    void setKnobValue (Control, float value);
    float releaseSeconds() const;
    void showMenu();
    void eyeClicked();
    void setReductionShown (bool);
    juce::Rectangle<int> tipsArea() const;
    juce::Rectangle<int> stripArea() const;

    LuckyCompProcessor& owner;
    juce::SharedResourcePointer<lucky::Settings> settings;
    std::array<std::unique_ptr<ModeTab>, lucky::numModes> tabs;
    juce::DrawableButton menuButton { "Menu", juce::DrawableButton::ImageFitted };
    lucky::LiveView liveView;
    lucky::HitView hitView;
    std::array<lucky::Knob, numControls> knobs;
    lucky::KeyHintButton compareHint { "C", "Compare with uncompressed" };
    lucky::ToggleNotice compareNotice { "Hearing it uncompressed", "C", "Back to compressed" };
    lucky::SmallSwitch matchSwitch { "LEVEL MATCH" };
    lucky::LabelTabs mixTabs { LuckyCompProcessor::mixModes };
    std::optional<bool> shownBlendMode;
    juce::TooltipWindow tooltips { this, 500 };
    std::optional<lucky::Mode> shownMode;
    juce::String modeTip;
    bool tipsShown = true, labShown = true, comparing = false;
    bool focusTaken = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuckyCompView)
};

class LuckyCompEditor final : public lucky::ScaledEditor
{
public:
    explicit LuckyCompEditor (LuckyCompProcessor&);

    /** The view at base size, for snapshots. */
    LuckyCompView& getView() noexcept { return view; }

private:
    LuckyCompView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuckyCompEditor)
};

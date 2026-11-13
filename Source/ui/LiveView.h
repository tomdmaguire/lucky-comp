#pragma once

#include "ModeColours.h"
#include "dsp/CompressorEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace lucky
{

/** The last few seconds of audio: input peaks faint, compressed peaks (before make-up) solid, the threshold
    dashed and gain reduction as a white line from the top. The gain-reduction number is hidden until the eye
    button reveals it, to the button's left so the pointer never covers it: the curve explains, while a number
    always on show invites compressing by eye. */
class LiveView final : public juce::Component
{
public:
    static constexpr double shownSeconds = 4.0;
    static constexpr float rangeDb = 54.0f;
    static constexpr float reductionRangeDb = 24.0f;
    static constexpr int headerHeight = 42;
    static constexpr int eyeWidth = 64;
    /** Room the header keeps left of the GR button for its readout. */
    static constexpr int readoutWidth = 80;
    /** Width of one slice of history, in pixels at base size. */
    static constexpr float columnWidth = 1.0f;
    /** How fast a drawn envelope may fall from one column to the next; real decays are slower. */
    static constexpr float fallDbPerColumn = 4.0f;

    LiveView (const CompressorEngine&, std::function<double()> sampleRate);

    /** How far back the readout looks for the deepest reduction, so the number is steady enough to read. */
    static constexpr double readoutWindowSeconds = 0.3;

    void setColour (juce::Colour);
    void setThreshold (float db);

    /** Shows or hides the gain-reduction number. */
    void setReductionShown (bool);

    bool isReductionShown() const noexcept { return reductionShown; }

    /** Called when the eye button is clicked; the owner decides whether (and after asking, when) to toggle. */
    std::function<void()> onEyeClicked;

    /** The eye button's area, for anchoring the prompt that asks before showing the number. */
    juce::Rectangle<int> getEyeBounds() const { return eye.getBounds(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** An eye: open in the mode's colour while the number is shown, crossed out while hidden. */
    struct EyeButton final : juce::Component, juce::SettableTooltipClient
    {
        EyeButton();
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;

        std::function<void()> onClick;
        bool open = false;
        juce::Colour colour = theme::accent;
    };

    float recentReductionDb() const;

    EyeButton eye;
    bool reductionShown = false;
    const CompressorEngine& engine;
    std::function<double()> sampleRate;
    juce::Colour colour = theme::accent;
    float thresholdDb = -24.0f;
    juce::VBlankAttachment vblank { this, [this] { repaint(); } };
};

} // namespace lucky

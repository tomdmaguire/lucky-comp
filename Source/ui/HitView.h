#pragma once

#include "ModeColours.h"
#include "dsp/ExampleHit.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace lucky
{

/** The Lab: a test snare through the current settings, drawn as a real waveform (each cycle visible, on a dB scale)
    on a measuring grid, so it reads as a test bench rather than your audio: input faint, output solid, the
    threshold, and the gain-reduction line. */
class HitView final : public juce::Component
{
public:
    static constexpr float rangeDb = 54.0f;
    static constexpr float reductionRangeDb = 24.0f;
    static constexpr int headerHeight = 42;
    static constexpr double reductionSmoothingSeconds = 0.006;
    static constexpr double rulerStepMs = 100.0;
    static constexpr double rulerLabelEveryMs = 200.0;

    HitView();

    /** Re-runs the hit if the settings, blend or window changed. */
    void update (const DetectorSettings&, double windowSeconds, Blend, const ColourSettings&);

    void setColour (juce::Colour);

    /** The detector's release when the example hit lets go, for when no audio is playing. */
    float getReleaseSeconds() const noexcept { return trace.releaseSeconds; }

    /** Called when the test hit button is clicked. */
    std::function<void()> onPlayHit;

    /** Lights the test hit button briefly, as when S is pressed. */
    void flashHit();

    /** Shows, for a moment, what Colour adds to the snare at its loudest: the second and third harmonics in dB below
        the hit. The waveform can't show tone (harmonics 30 dB down barely move it), so this does. */
    void revealColour();

    /** Keeps the harmonics showing while the Colour dial is held; letting go starts the usual fade. */
    void holdColour (bool held);

    static constexpr double colourShownSeconds = 2.0, colourFadeSeconds = 0.4;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** "▶ TEST SNARE [S]": plays the test snare through the compressor. */
    struct HitButton final : juce::Component, juce::SettableTooltipClient, private juce::Timer
    {
        static constexpr int flashMs = 250;
        static constexpr float playWidth = 18.0f;

        HitButton();
        int getIdealWidth() const;
        void flash();
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void timerCallback() override;
        static juce::FontOptions font() { return theme::labelFont (11.0f).withStyle ("Bold"); }

        std::function<void()> onClick;
        juce::Colour colour = theme::accent;
        const juce::String text { "TEST SNARE" };
        bool lit = false;
    };

    /** The measuring grid behind the hit: a dotted line every rulerStepMs. */
    void paintBench (juce::Graphics&, juce::Rectangle<float> area) const;

    /** The harmonics panel, in the bottom right of the plot (clear of the gain-reduction line), faded by alpha. */
    void paintColour (juce::Graphics&, juce::Rectangle<float> area, float alpha) const;
    float colourAlpha() const;

    HitTrace trace;
    DetectorSettings shownSettings;
    double shownWindow = 0.0;
    Blend shownBlend;
    ColourSettings shownColour;
    Colour::Harmonics harmonics;
    double colourShownUntil = 0.0;
    bool colourHeld = false;
    juce::TimedCallback colourFade { [this]
                                     {
                                         repaint();
                                         if (colourAlpha() <= 0.0f)
                                             colourFade.stopTimer();
                                     } };
    juce::Colour colour = theme::accent;
    HitButton hitButton;
    InfoDot info;
};

} // namespace lucky

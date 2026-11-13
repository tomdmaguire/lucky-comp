#include "HitView.h"
#include "WaveformDrawing.h"

namespace lucky
{

HitView::HitButton::HitButton()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (false);
    setRepaintsOnMouseActivity (true);
    setTooltip ("Play the test snare through the compressor (S)");
}

int HitView::HitButton::getIdealWidth() const
{
    return static_cast<int> (playWidth + juce::GlyphArrangement::getStringWidth (font(), text) + 8.0f + keycapWidth ("S", 16.0f) + 16.0f);
}

void HitView::HitButton::flash()
{
    lit = true;
    repaint();
    startTimer (flashMs);
}

void HitView::HitButton::timerCallback()
{
    stopTimer();
    lit = false;
    repaint();
}

void HitView::HitButton::mouseDown (const juce::MouseEvent&)
{
    if (onClick)
        onClick();
}

void HitView::HitButton::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.75f);
    const auto active = lit || isMouseOver();
    if (lit)
    {
        g.setColour (colour.withAlpha (0.18f));
        g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
    }
    g.setColour (lit ? colour : theme::outline.brighter (isMouseOver() ? 0.45f : 0.2f));
    g.drawRoundedRectangle (bounds, bounds.getHeight() * 0.5f, 1.0f);

    auto content = bounds.reduced (10.0f, 0.0f);
    const auto play = content.removeFromLeft (playWidth - 10.0f).withSizeKeepingCentre (7.0f, 8.0f);
    juce::Path triangle;
    triangle.addTriangle (play.getX(), play.getY(), play.getX(), play.getBottom(), play.getRight(), play.getCentreY());
    g.setColour (lit ? colour : (active ? theme::text : theme::textDim));
    g.fillPath (triangle);

    const auto key = content.removeFromRight (keycapWidth ("S", 16.0f)).withSizeKeepingCentre (keycapWidth ("S", 16.0f), 16.0f);
    paintKeycap (g, key, "S");

    g.setColour (active ? theme::text : theme::textDim);
    g.setFont (font());
    g.drawText (text, content.withTrimmedLeft (4.0f), juce::Justification::centredLeft);
}

HitView::HitView()
{
    setOpaque (true);
    shownSettings.thresholdDb = 1000.0f;
    hitButton.onClick = [this] { if (onPlayHit) onPlayHit(); };
    addAndMakeVisible (hitButton);
    info.setTooltip ("A test snare through your settings, so you can see what they do to one hit. S plays it, C compares it with uncompressed.");
    addAndMakeVisible (info);
}

void HitView::resized()
{
    const auto width = hitButton.getIdealWidth();
    hitButton.setBounds (getWidth() - 18 - width, 9, width, 24);
    const auto labWidth = juce::roundToInt (juce::GlyphArrangement::getStringWidth (theme::labelFont (12.0f), "LAB"));
    info.setBounds (18 + labWidth + 8, 9 + (24 - InfoDot::size) / 2, InfoDot::size, InfoDot::size);
}

void HitView::flashHit()
{
    hitButton.flash();
}

void HitView::update (const DetectorSettings& settings, double windowSeconds, Blend blend, const ColourSettings& tone)
{
    if (settings == shownSettings && juce::approximatelyEqual (windowSeconds, shownWindow) && blend == shownBlend && tone == shownColour)
        return;
    shownColour = tone;
    shownSettings = settings;
    shownWindow = windowSeconds;
    shownBlend = blend;
    trace = example_hit::simulate (settings, windowSeconds, example_hit::sampleRate, blend, tone);

    const auto deepest = trace.reduction.empty() ? 0.0f : *std::max_element (trace.reduction.begin(), trace.reduction.end());
    Colour stage;
    stage.setSettings (tone);
    harmonics = stage.harmonicsAt (juce::Decibels::gainToDecibels (example_hit::hitPeak) - deepest, deepest);
    repaint();
}

void HitView::holdColour (bool held)
{
    colourHeld = held;
    revealColour();
}

void HitView::revealColour()
{
    colourShownUntil = juce::Time::getMillisecondCounterHiRes() * 0.001 + colourShownSeconds;
    colourFade.startTimerHz (30);
    repaint();
}

float HitView::colourAlpha() const
{
    if (colourHeld)
        return 1.0f;
    const auto left = colourShownUntil - juce::Time::getMillisecondCounterHiRes() * 0.001;
    return static_cast<float> (juce::jlimit (0.0, 1.0, left / colourFadeSeconds));
}

void HitView::paintColour (juce::Graphics& g, juce::Rectangle<float> area, float alpha) const
{
    constexpr auto floorDb = -90.0f, ceilingDb = -10.0f;
    auto panel = juce::Rectangle<float> (area.getRight() - 258.0f, area.getBottom() - 122.0f, 250.0f, 112.0f);
    g.setColour (theme::panelRaised.withAlpha (0.94f * alpha));
    g.fillRoundedRectangle (panel, 6.0f);
    g.setColour (theme::outline.brighter (0.25f).withAlpha (alpha));
    g.drawRoundedRectangle (panel, 6.0f, 1.0f);

    auto content = panel.reduced (12.0f, 9.0f);
    g.setFont (theme::labelFont (10.0f));
    g.setColour (theme::textDim.withAlpha (alpha));
    g.drawText ("HARMONICS ADDED", content.removeFromTop (14.0f), juce::Justification::centredLeft);
    content.removeFromTop (6.0f);

    const auto row = [&] (const juce::String& name, float dbc)
    {
        auto line = content.removeFromTop (18.0f);
        g.setFont (theme::labelFont (10.0f));
        g.setColour (theme::text.withAlpha (alpha));
        g.drawText (name, line.removeFromLeft (78.0f), juce::Justification::centredLeft);
        const auto shown = dbc > floorDb;
        const juce::String minus (juce::CharPointer_UTF8 ("\xe2\x88\x92"));
        const auto off = dbc <= -199.0f;
        g.drawText (shown ? minus + juce::String (juce::roundToInt (-dbc)) + " dB" : off ? juce::String ("off") : "< " + minus + juce::String (juce::roundToInt (-floorDb)) + " dB",
                    line.removeFromRight (64.0f), juce::Justification::centredRight);
        const auto track = line.reduced (4.0f, 6.0f);
        g.setColour (theme::grid.brighter (0.3f).withAlpha (alpha));
        g.fillRoundedRectangle (track, 2.0f);
        if (shown)
        {
            const auto fill = juce::jlimit (0.0f, 1.0f, (dbc - floorDb) / (ceilingDb - floorDb));
            g.setColour (colour.withAlpha (alpha));
            g.fillRoundedRectangle (track.withWidth (std::max (3.0f, track.getWidth() * fill)), 2.0f);
        }
    };
    const juce::String dot (juce::CharPointer_UTF8 (" \xc2\xb7 "));
    row ("2ND" + dot + "WARM", harmonics.secondDbc);
    row ("3RD" + dot + "EDGY", harmonics.thirdDbc);

    content.removeFromTop (4.0f);
    g.setFont (juce::FontOptions (11.5f));
    g.setColour (theme::textDim.withAlpha (alpha));
    g.drawFittedText (juce::String (juce::CharPointer_UTF8 ("Overtones at 2\xc3\x97 and 3\xc3\x97 each note's pitch, in dB below the hit.")),
                      content.removeFromTop (30.0f).toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
}

void HitView::setColour (juce::Colour newColour)
{
    colour = hitButton.colour = newColour;
    hitButton.repaint();
    repaint();
}

void HitView::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    const auto header = juce::Rectangle<int> (18, 9, getWidth() - 36, 24).toFloat();
    g.setFont (theme::labelFont (12.0f));
    g.setColour (theme::textDim);
    g.drawText ("LAB", header, juce::Justification::centredLeft);

    const auto area = getLocalBounds().withTrimmedTop (headerHeight).reduced (18, 0).withTrimmedBottom (14).toFloat();
    const auto half = area.getHeight() * 0.5f - 2.0f;
    paintBench (g, area);

    if (trace.input.empty())
        return;

    const auto step = 1.0f;
    const auto columns = static_cast<size_t> (area.getWidth() / step) + 1;
    const auto perColumn = static_cast<double> (trace.input.size()) / static_cast<double> (columns - 1);
    waveform::Columns inAbove (columns), inBelow (columns), outAbove (columns), outBelow (columns), deepest (columns);
    const auto height = [half] (float magnitude)
    {
        return magnitude > 1.0e-6f ? waveform::heightOf (20.0f * std::log10 (magnitude), rangeDb, half) : 0.0f;
    };

    for (size_t x = 0; x < columns; ++x)
    {
        const auto first = static_cast<size_t> (static_cast<double> (x) * perColumn);
        const auto last = std::max (first + 1, static_cast<size_t> (static_cast<double> (x + 1) * perColumn));
        auto inMax = 0.0f, inMin = 0.0f, outMax = 0.0f, outMin = 0.0f;
        for (auto i = first; i < last && i < trace.input.size(); ++i)
        {
            inMax = std::max (inMax, trace.input[i]);
            inMin = std::min (inMin, trace.input[i]);
            outMax = std::max (outMax, trace.output[i]);
            outMin = std::min (outMin, trace.output[i]);
            deepest[x] = std::max (deepest[x], trace.reduction[i]);
        }
        inAbove[x] = height (inMax);
        inBelow[x] = height (-inMin);
        outAbove[x] = height (outMax);
        outBelow[x] = height (-outMin);
    }

    // The reduction line is held and averaged over a few milliseconds to lose the ripple the hit's single
    // cycles put on it; the waveform itself is drawn as it is.
    const auto hold = static_cast<size_t> (std::ceil (reductionSmoothingSeconds * trace.sampleRate / perColumn));
    waveform::paintSignals (g, area, inAbove, inBelow, outAbove, outBelow, area.getX(), step, colour);
    waveform::paintThreshold (g, area, shownSettings.thresholdDb, rangeDb);
    waveform::paintReduction (g, area, waveform::average (waveform::holdPeaks (deepest, hold), hold), area.getX(), step, reductionRangeDb);
    waveform::paintThresholdLabel (g, area, shownSettings.thresholdDb, rangeDb);

    if (const auto alpha = colourAlpha(); alpha > 0.0f && shownColour.mode != Mode::clean)
        paintColour (g, area, alpha);

    g.setFont (theme::labelFont (10.0f));
    g.setColour (theme::textDim);
    for (auto ms = rulerLabelEveryMs; ms <= trace.windowSeconds * 1000.0 + 0.5; ms += rulerLabelEveryMs)
    {
        const auto x = area.getX() + area.getWidth() * static_cast<float> (ms / (trace.windowSeconds * 1000.0));
        const auto last = ms + rulerLabelEveryMs > trace.windowSeconds * 1000.0 + 0.5;
        g.drawText (format::time (static_cast<float> (ms)), juce::Rectangle<float> (x - (last ? 60.0f : 30.0f), area.getBottom() - 2.0f, 60.0f, 14.0f),
                    last ? juce::Justification::centredRight : juce::Justification::centred);
    }
}

void HitView::paintBench (juce::Graphics& g, juce::Rectangle<float> area) const
{
    const auto window = example_hit::windowSeconds * 1000.0;
    const float dots[] { 1.0f, 3.0f };
    g.setColour (theme::grid.brighter (0.25f));
    for (auto ms = 0.0; ms <= window + 0.5; ms += rulerStepMs)
    {
        const auto x = area.getX() + area.getWidth() * static_cast<float> (ms / window);
        g.drawDashedLine ({ x, area.getY(), x, area.getBottom() }, dots, 2, 1.0f);
    }
    g.setColour (theme::grid);
    g.fillRect (area.getX(), area.getCentreY(), area.getWidth(), 1.0f);
}

} // namespace lucky

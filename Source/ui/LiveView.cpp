#include "LiveView.h"
#include "WaveformDrawing.h"

namespace lucky
{

namespace
{
/** An eye, for the reveal pill. */
juce::Path eyeIcon (juce::Rectangle<float> r)
{
    juce::Path eye;
    eye.startNewSubPath (r.getX(), r.getCentreY());
    eye.quadraticTo (r.getCentreX(), r.getY() - r.getHeight() * 0.35f, r.getRight(), r.getCentreY());
    eye.quadraticTo (r.getCentreX(), r.getBottom() + r.getHeight() * 0.35f, r.getX(), r.getCentreY());
    eye.closeSubPath();
    return eye;
}
} // namespace

LiveView::EyeButton::EyeButton()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (false);
    setRepaintsOnMouseActivity (true);
}

void LiveView::EyeButton::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.75f);
    g.setColour (open ? colour.withAlpha (0.8f) : theme::outline.brighter (isMouseOver() ? 0.45f : 0.2f));
    g.drawRoundedRectangle (bounds, bounds.getHeight() * 0.5f, 1.0f);

    auto content = bounds.reduced (11.0f, 0.0f);
    const auto icon = content.removeFromLeft (15.0f).withSizeKeepingCentre (15.0f, 9.0f);
    const auto tint = open ? colour : (isMouseOver() ? theme::text : theme::textDim);
    g.setColour (tint);
    g.strokePath (eyeIcon (icon), juce::PathStrokeType (1.2f));
    g.fillEllipse (icon.withSizeKeepingCentre (3.8f, 3.8f));
    if (! open)
        g.drawLine ({ icon.getX() + 1.0f, icon.getBottom() + 2.0f, icon.getRight() - 1.0f, icon.getY() - 2.0f }, 1.2f);

    g.setColour (open ? theme::text : tint);
    g.setFont (theme::labelFont (12.0f));
    g.drawText ("GR", content.withTrimmedLeft (7.0f), juce::Justification::centredLeft);
}

void LiveView::EyeButton::mouseUp (const juce::MouseEvent& event)
{
    if (getLocalBounds().contains (event.getPosition()) && onClick)
        onClick();
}

LiveView::LiveView (const CompressorEngine& engineToShow, std::function<double()> rate)
    : engine (engineToShow), sampleRate (std::move (rate))
{
    setOpaque (true);
    eye.onClick = [this]
    {
        if (onEyeClicked)
            onEyeClicked();
    };
    addAndMakeVisible (eye);
    setReductionShown (false);
}

void LiveView::resized()
{
    eye.setBounds (getWidth() - 18 - eyeWidth, 9, eyeWidth, 24);
}

void LiveView::setReductionShown (bool shown)
{
    reductionShown = eye.open = shown;
    eye.setTooltip (shown ? "Hide how much it's compressing" : "Show how much it's compressing. Try guessing by ear first.");
    eye.repaint();
    repaint();
}

float LiveView::recentReductionDb() const
{
    const auto rate = sampleRate() > 0.0 ? sampleRate() : 48000.0;
    const auto entries = std::max (1, static_cast<int> (readoutWindowSeconds * rate / CompressorEngine::meterBlockSize));
    const auto newest = engine.getMeterPosition();
    auto deepest = 0.0f;
    for (auto index = std::max (0, newest - entries); index < newest; ++index)
        deepest = std::max (deepest, engine.getMeterEntry (index).reductionDb);
    return deepest;
}

void LiveView::setColour (juce::Colour newColour)
{
    colour = eye.colour = newColour;
    eye.repaint();
}

void LiveView::setThreshold (float db)
{
    thresholdDb = db;
}

void LiveView::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    const auto header = juce::Rectangle<int> (18, 9, getWidth() - 36, 24).toFloat();

    g.setFont (theme::labelFont (12.0f));
    g.setColour (theme::textDim);
    g.drawText ("YOUR SOUND", header, juce::Justification::centredLeft);
    if (reductionShown)
    {
        const auto reduction = recentReductionDb();
        g.setColour (theme::text);
        g.drawText ((reduction > 0.05f ? format::minus : juce::String()) + juce::String (reduction, 1) + " dB",
                    header.withRight (static_cast<float> (eye.getX()) - 10.0f).withLeft (static_cast<float> (eye.getX()) - 80.0f),
                    juce::Justification::centredRight);
    }

    const auto area = getLocalBounds().withTrimmedTop (headerHeight).reduced (18, 0).withTrimmedBottom (14).toFloat();
    const auto mid = area.getCentreY();
    const auto half = area.getHeight() * 0.5f - 2.0f;
    g.setColour (theme::grid);
    g.fillRect (area.getX(), mid, area.getWidth(), 1.0f);

    // Columns are fixed slices of the meter history (not of the screen), so they don't shimmer as new audio
    // arrives; the whole picture slides left by the fraction of a column that has filled since.
    const auto rate = sampleRate() > 0.0 ? sampleRate() : 48000.0;
    const auto blocks = std::min (CompressorEngine::meterCapacity - 1, static_cast<int> (shownSeconds * rate / CompressorEngine::meterBlockSize));
    const auto newest = engine.getMeterPosition();
    const auto step = columnWidth;
    const auto columns = static_cast<size_t> (area.getWidth() / step) + 2;
    const auto perColumn = static_cast<double> (blocks) / (area.getWidth() / step);
    const auto newestColumn = static_cast<double> (newest) / perColumn;
    const auto lastColumn = static_cast<juce::int64> (std::floor (newestColumn));
    const auto slide = static_cast<float> (newestColumn - static_cast<double> (lastColumn)) * step;

    waveform::Columns input (columns), output (columns), reductionDb (columns);
    for (size_t c = 0; c < columns; ++c)
    {
        const auto column = lastColumn - static_cast<juce::int64> (columns - 1 - c);
        const auto first = std::max (static_cast<juce::int64> (std::ceil (static_cast<double> (column) * perColumn)),
                                     static_cast<juce::int64> (newest) - CompressorEngine::meterCapacity + 1);
        const auto last = std::min (static_cast<juce::int64> (std::ceil (static_cast<double> (column + 1) * perColumn)),
                                    static_cast<juce::int64> (newest));
        auto inDb = -120.0f, outDb = -120.0f, deepest = 0.0f;
        for (auto index = std::max (first, juce::int64 { 0 }); index < last; ++index)
        {
            const auto e = engine.getMeterEntry (static_cast<int> (index));
            inDb = std::max (inDb, e.inputDb);
            outDb = std::max (outDb, e.outputDb);
            deepest = std::max (deepest, e.reductionDb);
        }
        input[c] = waveform::heightOf (inDb, rangeDb, half);
        output[c] = waveform::heightOf (outDb, rangeDb, half);
        reductionDb[c] = deepest;
    }

    const auto maxFall = half * fallDbPerColumn / rangeDb;
    waveform::easeFalls (input, maxFall);
    waveform::easeFalls (output, maxFall);
    input = waveform::average (input, 1);
    output = waveform::average (output, 1);
    const auto x0 = area.getRight() - static_cast<float> (columns - 1) * step - slide + step;

    waveform::paintEnvelopes (g, area, input, output, x0, step, colour);
    waveform::paintThreshold (g, area, thresholdDb, rangeDb);
    waveform::paintReduction (g, area, waveform::average (reductionDb, 1), x0, step, reductionRangeDb);
    waveform::paintThresholdLabel (g, area, thresholdDb, rangeDb);

    if (newest == 0)
    {
        g.setColour (theme::textDim);
        g.setFont (theme::labelFont (13.0f));
        g.drawText ("Play something to see it here", area.withTrimmedBottom (area.getHeight() * 0.3f), juce::Justification::centred);
    }
}

} // namespace lucky

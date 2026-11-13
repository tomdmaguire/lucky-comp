#pragma once

#include "ModeColours.h"
#include <vector>

/** Drawing shared by the live view and the Lab, so both speak the same visual language: a faint
    input envelope, a gradient-filled output envelope with a crisp edge, dashed threshold lines and a glowing
    gain-reduction line. */
namespace lucky::waveform
{

/** One value per column, in pixels from the centre line (envelopes) or from the top (reduction). */
using Columns = std::vector<float>;

/** Pixels above the centre line for a level, on a mirrored dB scale that fills half the height at 0 dB. */
inline float heightOf (float db, float rangeDb, float half)
{
    return juce::jlimit (0.0f, 1.0f, (db + rangeDb) / rangeDb) * half;
}

/** Where a gain reduction sits on the line drawn down from the top: rangeDb fills half the height. */
inline float reductionY (juce::Rectangle<float> area, float reductionDb, float rangeDb)
{
    return area.getY() + 1.0f + (area.getHeight() * 0.5f) * juce::jlimit (0.0f, 1.0f, reductionDb / rangeDb);
}

/** Lets values rise at once but fall by at most maxFall per column, so decays read as curves instead of noise. */
inline void easeFalls (Columns& values, float maxFall)
{
    for (size_t i = 1; i < values.size(); ++i)
        values[i] = std::max (values[i], values[i - 1] - maxFall);
}

/** Largest value within reach columns either side, to hide ripples shorter than the window. */
inline Columns holdPeaks (const Columns& values, size_t reach)
{
    Columns held (values.size());
    for (size_t i = 0; i < values.size(); ++i)
    {
        const auto from = i > reach ? i - reach : 0;
        const auto to = std::min (values.size() - 1, i + reach);
        held[i] = *std::max_element (values.begin() + static_cast<std::ptrdiff_t> (from), values.begin() + static_cast<std::ptrdiff_t> (to) + 1);
    }
    return held;
}

/** Average over reach columns either side, to round off steps. */
inline Columns average (const Columns& values, size_t reach)
{
    Columns averaged (values.size());
    for (size_t i = 0; i < values.size(); ++i)
    {
        const auto from = i > reach ? i - reach : 0;
        const auto to = std::min (values.size() - 1, i + reach);
        auto sum = 0.0f;
        for (auto j = from; j <= to; ++j)
            sum += values[j];
        averaged[i] = sum / static_cast<float> (to - from + 1);
    }
    return averaged;
}

/** Adds a curve through the points, bending through each midpoint so corners become smooth. */
inline void addCurve (juce::Path& path, const std::vector<juce::Point<float>>& points, bool startNew)
{
    if (points.empty())
        return;
    if (startNew)
        path.startNewSubPath (points.front());
    else
        path.lineTo (points.front());

    for (size_t i = 1; i + 1 < points.size(); ++i)
    {
        const auto& p = points[i];
        path.quadraticTo (p, (p + points[i + 1]) * 0.5f);
    }
    if (points.size() > 1)
        path.lineTo (points.back());
}

/** Points along the top or bottom edge of a shape drawn around the centre line. */
inline std::vector<juce::Point<float>> edgePoints (const Columns& heights, float x0, float step, float mid, bool top)
{
    std::vector<juce::Point<float>> points;
    points.reserve (heights.size());
    for (size_t i = 0; i < heights.size(); ++i)
        points.emplace_back (x0 + static_cast<float> (i) * step, top ? mid - heights[i] : mid + heights[i]);
    return points;
}

/** Fills the shape between an upper and a lower edge (pixels above and below the centre line) and returns the
    edges for stroking. */
inline juce::Path fillBetween (juce::Graphics& g, const Columns& above, const Columns& below, float x0, float step, float mid)
{
    auto top = edgePoints (above, x0, step, mid, true);
    auto bottom = edgePoints (below, x0, step, mid, false);

    juce::Path fill;
    addCurve (fill, top, true);
    std::reverse (bottom.begin(), bottom.end());
    addCurve (fill, bottom, false);
    fill.closeSubPath();
    g.fillPath (fill);

    std::reverse (bottom.begin(), bottom.end());
    juce::Path edges;
    addCurve (edges, top, true);
    addCurve (edges, bottom, true);
    return edges;
}

/** Paints the input faintly and the output with a gradient that is brightest at its edges. Each is given as its
    upper and lower extent around the centre line, so the same drawing serves mirrored envelopes and real waveforms. */
inline void paintSignals (juce::Graphics& g, juce::Rectangle<float> area, const Columns& inputAbove, const Columns& inputBelow,
                          const Columns& outputAbove, const Columns& outputBelow, float x0, float step, juce::Colour colour)
{
    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (area.toNearestIntEdges());
    const auto mid = area.getCentreY();
    const auto half = area.getHeight() * 0.5f;

    g.setColour (colour.withAlpha (0.12f));
    const auto inputEdges = fillBetween (g, inputAbove, inputBelow, x0, step, mid);
    g.setColour (colour.withAlpha (0.3f));
    g.strokePath (inputEdges, juce::PathStrokeType (1.0f));

    juce::ColourGradient gradient (colour.withAlpha (0.95f), 0.0f, mid - half, colour.withAlpha (0.95f), 0.0f, mid + half, false);
    gradient.addColour (0.5, colour.withAlpha (0.5f));
    g.setGradientFill (gradient);
    const auto outputEdges = fillBetween (g, outputAbove, outputBelow, x0, step, mid);
    g.setColour (colour.brighter (0.3f));
    g.strokePath (outputEdges, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

/** Paints mirrored input and output envelopes. */
inline void paintEnvelopes (juce::Graphics& g, juce::Rectangle<float> area, const Columns& input, const Columns& output,
                            float x0, float step, juce::Colour colour)
{
    paintSignals (g, area, input, input, output, output, x0, step, colour);
}

/** Dashed threshold lines above and below the centre line, on a mirrored dB scale. */
inline void paintThreshold (juce::Graphics& g, juce::Rectangle<float> area, float thresholdDb, float rangeDb)
{
    const auto mid = area.getCentreY();
    const auto offset = heightOf (thresholdDb, rangeDb, area.getHeight() * 0.5f - 2.0f);
    const float dashes[] { 5.0f, 4.0f };
    g.setColour (theme::text.withAlpha (0.4f));
    for (const auto y : { mid - offset, mid + offset })
        g.drawDashedLine ({ area.getX(), y, area.getRight(), y }, dashes, 2, 1.0f);
}

/** Names the upper threshold line, on a chip so it reads over the waveform; drawn last, above everything. */
inline void paintThresholdLabel (juce::Graphics& g, juce::Rectangle<float> area, float thresholdDb, float rangeDb)
{
    const auto mid = area.getCentreY();
    const auto offset = heightOf (thresholdDb, rangeDb, area.getHeight() * 0.5f - 2.0f);
    const auto font = theme::labelFont (9.0f);
    const auto chip = juce::Rectangle<float> (area.getX() + 4.0f, mid - offset - 7.0f,
                                              juce::GlyphArrangement::getStringWidth (font, "THRESHOLD") + 10.0f, 14.0f);
    g.setColour (theme::panel.withAlpha (0.85f));
    g.fillRoundedRectangle (chip, 3.0f);
    g.setColour (theme::text.withAlpha (0.7f));
    g.setFont (font);
    g.drawText ("THRESHOLD", chip, juce::Justification::centred);
}

/** The gain-reduction line: a smooth white curve with a soft glow underneath. */
inline void paintReduction (juce::Graphics& g, juce::Rectangle<float> area, const Columns& reductionDb, float x0, float step, float rangeDb)
{
    std::vector<juce::Point<float>> points;
    points.reserve (reductionDb.size());
    for (size_t i = 0; i < reductionDb.size(); ++i)
        points.emplace_back (x0 + static_cast<float> (i) * step, reductionY (area, reductionDb[i], rangeDb));

    juce::Path line;
    addCurve (line, points, true);

    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (area.expanded (0.0f, 4.0f).toNearestIntEdges());
    g.setColour (theme::text.withAlpha (0.06f));
    g.strokePath (line, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (theme::text.withAlpha (0.12f));
    g.strokePath (line, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (theme::text);
    g.strokePath (line, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

} // namespace lucky::waveform

#pragma once

#include "dsp/Modes.h"
#include <lucky_ui/lucky_ui.h>

namespace lucky::theme
{

/** A mode's colour from the Lucky palette: Clean gold, FET pink, Opto green, VCA blue. */
inline juce::Colour modeColour (Mode mode)
{
    static const juce::Colour colours[] { gold, pink, green, blue };
    return colours[static_cast<size_t> (juce::jlimit (0, numModes - 1, static_cast<int> (mode)))];
}

} // namespace lucky::theme

namespace lucky::format
{

/** A ratio like "4.0:1" or "12:1". */
inline juce::String ratio (float r)
{
    return (r < 10.0f ? juce::String (r, 1) : juce::String (juce::roundToInt (r))) + ":1";
}

} // namespace lucky::format

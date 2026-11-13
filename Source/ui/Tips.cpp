#include "Tips.h"

namespace lucky::tips
{

juce::String mode (Mode m)
{
    switch (m)
    {
        case Mode::clean: return "Clean. Transparent, with no colour of its own.";
        case Mode::fet: return "FET, think 1176. Fast and bold: grabs the hits on drums and vocals.";
        case Mode::opto: return "Opto, think LA-2A. Slow and smooth: gently evens out vocals and bass.";
        case Mode::vca: return "VCA, think SSL bus compressor. Tight and punchy: glues a mix together.";
    }
    return {};
}

juce::String attackWord (float attackMs)
{
    if (attackMs < 0.5f)
        return "Flattens";
    if (attackMs < 5.0f)
        return "Tight";
    return attackMs < 25.0f ? "Punchy" : "Gentle";
}

juce::String releaseWord (float releaseMs)
{
    if (releaseMs < 80.0f)
        return "Fast";
    return releaseMs < 400.0f ? "Natural" : "Glued";
}

} // namespace lucky::tips

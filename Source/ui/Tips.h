#pragma once

#include "dsp/Modes.h"
#include <juce_core/juce_core.h>

namespace lucky::tips
{

/** One short line on what the mode is and what it's good for. */
juce::String mode (Mode);

/** One word on what an attack time does to a hit, for the Attack dial's caption. */
juce::String attackWord (float attackMs);

/** One word on what a release time does between hits, for the Release dial's caption. */
juce::String releaseWord (float releaseMs);

} // namespace lucky::tips

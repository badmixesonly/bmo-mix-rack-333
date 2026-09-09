#pragma once

#include "modules/dim/params.h"
#include <vector>

namespace bmo::dim
{

/** The presets that ship with the module.

    Init is index 0 and is every default, which for this module means a wire:
    width at unity, shuffle at 1.0, detune switched out and diffuse at zero. A
    stereo imager that widened the moment it was inserted would be making a
    decision the user has not made yet.

    The rest are named for the job rather than for the settings, and split
    along the one line that matters here -- whether the source already has
    side content. The three that turn detune on are the ones that work on a
    mono track; the others need a stereo source to do anything at all.
*/
inline const std::vector<FactoryPreset>& factory()
{
    static const std::vector<FactoryPreset> presets {
        { "Init", {} },

        // Needs a mono source: manufactures the width, then shapes it.
        { "Wide Vocal", { { kDetuneOn, 1.0f }, { kDetune, 12.0f },
                          { kWidth, 130.0f } } },

        { "Mono to Stereo", { { kDetuneOn, 1.0f }, { kDetune, 8.0f },
                              { kDiffuse, 40.0f }, { kDepth, 45.0f } } },

        { "Thicken", { { kDetuneOn, 1.0f }, { kDetune, 5.0f },
                       { kDiffuse, 25.0f }, { kRate, 0.20f } } },

        // Needs a stereo source: these only scale and steer what is there.
        { "Bass Shuffle", { { kShuffle, 2.0f }, { kShuffleFreq, 650.0f } } },

        { "Diffuse Pad", { { kDiffuse, 70.0f }, { kDepth, 60.0f },
                           { kRate, 0.25f }, { kWidth, 140.0f } } },

        { "Narrow", { { kWidth, 60.0f } } },
    };

    return presets;
}

} // namespace bmo::dim

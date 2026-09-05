#pragma once

#include "modules/sat/params.h"
#include <vector>

namespace bmo::sat
{

/** The presets that ship with the plugin.

    These are starting points, not verdicts. They have been checked against
    what the plugin measures -- `measure sweep` reports every one of these
    drive settings -- and they have not been checked by ear on real material,
    which is the only test that finally matters.

    Most of them push Input and pull Output back, because the curve is
    level-dependent: how hard the signal arrives is half of how much colour it
    gets, exactly as it is on hardware. The two are not equal and opposite,
    since the curve has a broadband loss of its own; the Output figures below
    are the ones that measured level on the reference voice, which is why they
    are odd numbers rather than round ones.

    Anything a preset does not mention goes back to its default, so a preset
    cannot leave a stray setting behind from whatever was loaded before it.
*/
inline const std::vector<FactoryPreset>& factory()
{
    static const std::vector<FactoryPreset> presets {
        { "Init", {} },

        // The calibration point: the drive at which the curve measures the
        // reference's own asymmetry, 0.62 against 0.84. Everything else in
        // this list is a move away from here.
        { "Reference", {
            { kDrive, 40.0f }, { kAutoGain, 1.0f } } },

        { "Vocal Sheen", {
            { kInputGain, 2.0f }, { kDrive, 34.0f },
            { kOutputLevel, -1.40f } } },

        // Pulled back in 0.4.0: at Drive 52 with 5 dB of input on top of it,
        // this sat past the point where the curve turns from colour into
        // overdrive, and it distorted on every source it was tried on. The
        // gain was doing most of the damage.
        { "Vocal Front", {
            { kInputGain, 1.5f }, { kDrive, 46.0f },
            { kOutputLevel, -0.57f } } },

        { "Whisper", {                             // barely there, for a take that only needs air
            { kDrive, 18.0f }, { kMix, 60.0f }, { kTone, 55.0f },
            { kOutputLevel, 0.86f } } },

        { "Drum Bus Glue", {
            { kInputGain, 3.0f }, { kDrive, 46.0f }, { kTone, 40.0f },
            { kMix, 70.0f },                       // parallel, so the transients stay whole
            { kOutputLevel, -0.77f } } },

        { "Snare Edge", {
            { kInputGain, 6.0f }, { kDrive, 66.0f },
            { kOutputLevel, -3.59f } } },

        { "Bass Warmth", {                         // no air on a bass; the curve only
            { kInputGain, 4.0f }, { kDrive, 30.0f }, { kTone, 0.0f },
            { kOutputLevel, -3.25f } } },

        { "Guitar Grit", {
            { kInputGain, 8.0f }, { kDrive, 78.0f },
            { kOutputLevel, -5.44f } } },

        { "Mix Bus Colour", {
            { kDrive, 24.0f }, { kMix, 45.0f },    // gentle, in parallel, level-matched
            { kAutoGain, 1.0f } } },

        // The top of the range, where the curve stops adding harmonics and
        // starts rearranging the waveform. Not subtle and not meant to be.
        { "Ruined", {
            { kInputGain, 10.0f }, { kDrive, 100.0f },
            { kOversampling, 2.0f },               // 4x: it needs the headroom up there
            { kOutputLevel, -7.73f } } },
    };

    return presets;
}

} // namespace bmo::sat

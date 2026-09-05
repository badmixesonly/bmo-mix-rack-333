#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace bmo::sat::tables
{

//==============================================================================
/** DRIVE, as the panel reads it, mapped to the shaper's positive-half drive.

    The two ends are the design decision, and they were re-scaled in 0.3.0
    after a listening test rather than by measurement.

    The band deltas cannot hear distortion. Fitted against them alone, the
    default landed at a curve drive of 4.0, and three independent listening
    tests said the same thing about it: DRIVE 40 was already overdriven, the
    audible crossover from colour to distortion sat at 25-30 rather than the
    predicted 55, and everything useful was bunched into the bottom third of
    the control. All one finding -- the range was about three times too hot.

    So the whole scale is divided by 2.75, which is the ratio the ear asked
    for. The default now produces a curve drive of 1.45, the crossover lands
    near the middle of the travel where it belongs, and the preset numbers did
    not have to change: shifting both ends by the same factor rescales every
    position on the knob at once.

    kDriveMin is still not zero: at DRIVE 0 the curve is still the curve,
    sitting far enough below its knee to be nearly linear, so the control
    changes how much of a character there is rather than fading one in.

    Geometric between them, because what the ear follows is the ratio between
    where the signal sits and where the knee is, not the difference.
*/
inline constexpr float kDriveMin = 0.24f;
inline constexpr float kDriveMax = 21.54f;

//==============================================================================
/** Static output compensation, in decibels, at eleven points across the DRIVE
    range and linearly interpolated between them.

    Measured, not guessed: `measure makeup` drives the real signal path with
    the harness's voice-like reference at -18 dBFS RMS and reports the broadband
    loss at each step. Regenerate it with that command if the curve changes.

    A level match matters more here than on most plugins. Saturation that
    arrives louder gets credit for the loudness, which is the oldest way there
    is to make a change sound like an improvement; without this, every A/B of
    the Drive control would be confounded by it.

    Static by construction. Following the programme with a level detector would
    make this a compressor, and the one thing this plugin must not do is
    reduce dynamics.
*/
inline constexpr std::array<float, 11> kMakeupDb
{
    +0.47f, +0.72f, +1.23f, +1.75f, +1.62f, +0.72f, -0.31f, -1.15f, -1.75f, -2.18f, -2.47f
};

inline float makeupDb (float amountPercent) noexcept
{
    const auto t = std::clamp (amountPercent, 0.0f, 100.0f) * 0.01f
                     * (float) (kMakeupDb.size() - 1);
    const auto i = (size_t) t;
    const auto f = t - (float) i;

    if (i + 1 >= kMakeupDb.size())
        return kMakeupDb.back();

    return kMakeupDb[i] + f * (kMakeupDb[i + 1] - kMakeupDb[i]);
}

} // namespace bmo::sat::tables

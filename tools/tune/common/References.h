#pragma once

/*
    Other tuners, measured: the numbers BMO Tune RT is held to. Each one is a
    render of the reference stimulus (tools/common/Stimulus.h) through that
    tuner, scored by bmo-tune-ref -- the same code that scores BMO -- and each
    says where, how, and with what settings, so it can be measured again.

    How they were rendered: bmo-tune-hostrender, which loads the VST3 the way
    a host does and applies NO delay compensation, at 48 kHz in blocks of
    128, sample 0 out against sample 0 in. What a plugin reports to the host
    is recorded beside what it does: neither of these reports what it does.

    Re-measure when a reference's version changes, and say so in the commit.
    The renders and tables: testing-notes/latency-and-lag-2026-09-11.md.
*/

namespace bmo::tune::references
{

struct Reference
{
    const char* who;
    const char* version;
    const char* settings;
    const char* measured;          ///< where and when
    double reportedLatencyMs;      ///< what it tells the host
    double trueLatencyMs;          ///< Stimulus score: worst delay, in tune or correcting
    double meanLagMs;              ///< Stimulus score: correction lag, mean over the vibratos
    double worstLagMs;             ///< and the worst of them
    double meanRmsCents;           ///< Stimulus score: RMS of out - target over the vibratos, mean
};

inline constexpr Reference kAntares {
    "Antares Auto-Tune Artist", "VST3 dated 2024-10-15, as installed on AURORA",
    "Input Type Low Male, Key C, Scale Chromatic, Retune Speed 0, Humanize 0, Natural Vibrato 0, "
    "Flex-Tune 0, Tracking 50 (default)",
    "AURORA, 2026-09-11",
    2.33, 6.49, -0.24, 1.66, 1.30 };

// Speed and Note Transition bottom out at 0.1 ms: set to 0, they read 0.1.
inline constexpr Reference kWaves {
    "Waves Tune Real-Time (Mono)", "16.0.23.24",
    "Speed 0.1 ms, Note Transition 0.1 ms (their minimum), Correction 100 %, Scale Chromatic, "
    "Vibrato off, everything else default",
    "AURORA, 2026-09-11",
    0.0, 10.62, 1.32, 2.13, 1.73 };

} // namespace bmo::tune::references

#pragma once

#include "core/state/ParamSpec.h"

namespace bmo::tune
{

//==============================================================================
// Parameter IDs. Permanent and append-only once a session has been saved --
// see BMO Mix Rack's modules/eq/params.h for why, and its root AGENTS.md for
// what "permanent" covers. Until 0.1 ships nothing here is frozen.
//==============================================================================

inline constexpr auto kModuleId   = "tune";
inline constexpr auto kModuleName = "BMO Tune RT";

// What a user reaches for first: how hard, and toward what.
inline constexpr auto kRetune  = "retune";
inline constexpr auto kKey     = "key";
inline constexpr auto kScale   = "scale";
inline constexpr auto kEngine  = "engine";
inline constexpr auto kRange   = "range";

// How natural.
inline constexpr auto kVibrato = "vibrato";
inline constexpr auto kFlex    = "flex";
inline constexpr auto kGlide   = "glide";

// HYBRID's formants: kept where the singer put them (on), or left to follow
// the correction (off), and shifted independently. Kept pending Frosty's call
// on 2026-09-10 -- see modules/tune/AGENTS.md, "Open".
inline constexpr auto kFormant      = "formant";
inline constexpr auto kFormantShift = "formant_shift";

// Set once per session and left.
inline constexpr auto kLatency = "latency";
inline constexpr auto kRefA    = "ref_a";

// The twelve-note allow map: each pitch class may be switched out of the
// scale. Twelve booleans rather than one packed integer so a host can
// automate and display each note, the way every tuner's keyboard does.
inline constexpr const char* kNoteIds[12] = {
    "note_c", "note_cs", "note_d", "note_ds", "note_e", "note_f",
    "note_fs", "note_g", "note_gs", "note_a", "note_as", "note_b" };

enum Index
{
    retune, key, scale, engine, range,
    vibrato, flex, glide,
    formant, formantShift,
    latency, refA,
    noteC, noteCs, noteD, noteDs, noteE, noteF,
    noteFs, noteG, noteGs, noteA, noteAs, noteB,
    count
};

enum class Engine { classic, hybrid };
enum class Range  { autoRange, soprano, altoTenor, bass, instrument };
enum class LatencyMode { live, studio };

/** Each range's search limits, in Hz. Auto floors at 80 Hz and only Bass and
    Instrument reach 55, which is Frosty's 2026-09-10 call on spec Part IV
    question 3: the 25 Hz between them costs ~9 ms of worst-case latency, so
    only the ranges that need it pay for it. */
struct RangeLimits { double minHz, maxHz; };

inline RangeLimits limitsOf (Range r) noexcept
{
    switch (r)
    {
        case Range::autoRange:  return { 80.0, 1400.0 };
        case Range::soprano:    return { 160.0, 1400.0 };
        case Range::altoTenor:  return { 100.0, 1000.0 };
        case Range::bass:       return { 55.0, 500.0 };
        case Range::instrument: return { 55.0, 1760.0 };
    }

    return { 80.0, 1400.0 };
}

inline constexpr int kVersionHint   = 1;
inline constexpr int kStateVersion  = 1;
inline constexpr int kSchemaVersion = 1;

inline const ParamSpecs& specs()
{
    using S = ParamSpec;
    using F = ParamFormat;

    static const ParamSpecs s = []
    {
        ParamSpecs p
        {
            // RETUNE SPEED: 0 is the hard snap and the default, because that
            // is what this plugin is for. The knob is exponential inside the
            // core (CorrectionLaw::retuneMsFromKnob), so the range stays
            // linear for the rack's ParamSpec/JUCE agreement.
            S::floatParam (kRetune, "Retune Speed", 0.0f, 100.0f, 0.1f, 0.0f),

            S::choiceParam (kKey, "Key", { "C", "C#", "D", "D#", "E", "F",
                                           "F#", "G", "G#", "A", "A#", "B" }, 0),

            // Chromatic by default: a fresh instance corrects to the nearest
            // semitone without knowing the song, which is the safe wrong answer.
            // Three scales, the ones a hard-tune session uses (Frosty,
            // 2026-09-10); more can be appended to the choice list later
            // without moving any saved session, since new entries go on the end.
            S::choiceParam (kScale, "Scale", { "Chromatic", "Major", "Minor" }, 0),

            S::choiceParam (kEngine, "Engine", { "Classic", "Hybrid" }, 0),
            S::choiceParam (kRange, "Pitch Range", { "Auto", "Soprano", "Alto/Tenor", "Bass", "Instrument" }, 0),

            // VIBRATO: 0 flattens it (the effect), 100 keeps the singer's own,
            // 150 exaggerates it.
            S::floatParam (kVibrato, "Vibrato", 0.0f, 150.0f, 1.0f, 0.0f, F::Percent),

            // FLEX: a soft-knee deadzone, off by default and not the point of
            // this plugin -- see modules/tune/AGENTS.md for its IP note.
            S::floatParam (kFlex, "Flex", 0.0f, 100.0f, 1.0f, 0.0f, F::Percent),

            // GLIDE: HYBRID only; CLASSIC never glides (spec §4.5).
            S::floatParam (kGlide, "Glide", 0.0f, 200.0f, 1.0f, 0.0f),

            S::boolParam (kFormant, "Formant Correct", true),
            S::floatParam (kFormantShift, "Formant Shift", -600.0f, 600.0f, 1.0f, 0.0f),

            // Live is the default: Frosty's call on spec Part IV question 4,
            // 2026-09-10. Live reports 0 to the host and runs dynamically
            // late, as Waves does; Studio reports the range's worst case.
            S::choiceParam (kLatency, "Latency", { "Live", "Studio" }, 0),

            S::floatParam (kRefA, "Ref A", 380.0f, 480.0f, 0.1f, 440.0f),
        };

        static constexpr const char* names[12] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

        for (int i = 0; i < 12; ++i)
            p.push_back (S::boolParam (kNoteIds[i], names[i], true));

        return p;
    }();

    return s;
}

} // namespace bmo::tune

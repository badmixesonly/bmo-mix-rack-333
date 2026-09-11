#pragma once

#include "core/state/ParamSpec.h"
#include <iterator>

namespace bmo::tune
{

//==============================================================================
// Parameter IDs. FROZEN from the first plugin build (Frosty, 2026-09-10):
// permanent and append-only, because the build goes into Ableton and a saved
// session references every id, its range, step and default. See BMO Mix
// Rack's modules/eq/params.h for why, and its root AGENTS.md for what
// "permanent" covers. tests/dsp/SchemaTests.cpp holds the table.
//
// One argued change since, 2026-09-11: engine, glide, formant, formant_shift
// and latency were REMOVED when Tune RT went CLASSIC and Live only (Frosty,
// after hearing 0.1 in Ableton). Removing is safe where reordering would not
// be: a VST3 host knows each parameter by a hash of its id string, and saved
// state is by id, so every remaining parameter keeps its identity and a 0.1
// session reloads everything but the five that are gone. Those ids are
// retired -- never reuse one for something else. The code that used them is
// on branch archive/hybrid-studio; testing-notes/nrt-tune-handoff-2026-09-11.md.
//==============================================================================

inline constexpr auto kModuleId   = "tune";
inline constexpr auto kModuleName = "BMO Tune RT";

// What a user reaches for first: how hard, and toward what.
inline constexpr auto kRetune  = "retune";
inline constexpr auto kKey     = "key";
inline constexpr auto kScale   = "scale";
inline constexpr auto kRange   = "range";

// How natural.
inline constexpr auto kVibrato = "vibrato";
inline constexpr auto kFlex    = "flex";

// Set once per session and left.
inline constexpr auto kRefA    = "ref_a";

/** Ids that 0.1 had and this version does not. Retired for good: a new
    parameter must never take one, or a 0.1 session would feed it a value
    meant for something else. SchemaTests checks none comes back. */
inline constexpr const char* kRetiredIds[] = { "engine", "glide", "formant", "formant_shift", "latency" };

// The twelve-note allow map: each pitch class may be switched out of the
// scale. Twelve booleans rather than one packed integer so a host can
// automate and display each note, the way every tuner's keyboard does.
inline constexpr const char* kNoteIds[12] = {
    "note_c", "note_cs", "note_d", "note_ds", "note_e", "note_f",
    "note_fs", "note_g", "note_gs", "note_a", "note_as", "note_b" };

enum Index
{
    retune, key, scale, range,
    vibrato, flex,
    refA,
    noteC, noteCs, noteD, noteDs, noteE, noteF,
    noteFs, noteG, noteGs, noteA, noteAs, noteB,
    count
};

/** The Key parameter's choices: every spelling a key signature uses, so the
    host's automation lane shows the key the way it was picked -- B♭, not A#.
    E#, B#, Cb and Fb are left out: no one picks those as a key.

    ASCII for the host, which may not have the glyphs; the panel draws ♯ and ♭.
    Frosty's call, 2026-09-10, over twelve pitch classes plus a spelling flag. */
inline constexpr int kNumKeySpellings = 17;

inline constexpr const char* kKeySpellings[kNumKeySpellings] = {
    "C", "C#", "Db", "D", "D#", "Eb", "E", "F", "F#", "Gb", "G", "G#", "Ab", "A", "A#", "Bb", "B" };

/** The pitch class (0 = C) a Key choice names. */
inline int pitchClassOfKey (int spelling) noexcept
{
    static constexpr int pc[kNumKeySpellings] = { 0, 1, 1, 2, 3, 3, 4, 5, 6, 6, 7, 8, 8, 9, 10, 10, 11 };
    return pc[spelling < 0 ? 0 : (spelling >= kNumKeySpellings ? kNumKeySpellings - 1 : spelling)];
}

enum class Range  { autoRange, soprano, altoTenor, bass, instrument };

/** Each range's search limits, in Hz. Auto floors at 80 Hz and only Bass and
    Instrument reach 55, which is Frosty's 2026-09-10 call on spec Part IV
    question 3: the lower the floor, the longer the longest period the
    detector must see and the further a correcting read can wander behind,
    so only the ranges that need 55 Hz pay for it. */
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

            S::choiceParam (kKey, "Key", { std::begin (kKeySpellings), std::end (kKeySpellings) }, 0),

            // Chromatic by default: a fresh instance corrects to the nearest
            // semitone without knowing the song, which is the safe wrong answer.
            // Three scales, the ones a hard-tune session uses (Frosty,
            // 2026-09-10); more can be appended to the choice list later
            // without moving any saved session, since new entries go on the end.
            S::choiceParam (kScale, "Scale", { "Chromatic", "Major", "Minor" }, 0),

            S::choiceParam (kRange, "Pitch Range", { "Auto", "Soprano", "Alto/Tenor", "Bass", "Instrument" }, 0),

            // VIBRATO: 0 flattens it (the effect), 100 keeps the singer's own,
            // 150 exaggerates it.
            S::floatParam (kVibrato, "Vibrato", 0.0f, 150.0f, 1.0f, 0.0f, F::Percent),

            // FLEX: a soft-knee deadzone, off by default and not the point of
            // this plugin -- see modules/tune/AGENTS.md for its IP note.
            S::floatParam (kFlex, "Flex", 0.0f, 100.0f, 1.0f, 0.0f, F::Percent),

            // No latency parameter: Tune RT is Live only, reporting 0 to the
            // host and running 0.4 ms behind at rest (Frosty, 2026-09-11).

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

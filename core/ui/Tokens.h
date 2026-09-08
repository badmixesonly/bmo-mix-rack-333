#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bmo::ui
{

/** The design tokens: every colour the suite draws with, named for what it is
    used on rather than for what it looks like, so a theme can change any of
    them without touching a panel.

    Every module uses the same neutral plate and the same structure; only
    `accent` differs between modules, and that is set per module by
    ModuleDef::accent rather than here. A rack reads as one instrument, and you
    can still tell the EQ from the saturator at a glance.

    The non-colour tokens -- corner radius, stroke weights, the knob-to-label
    gaps -- are fixed and not themable, so a theme cannot break a layout.
*/
struct Tokens
{
    juce::Colour plate      { 0xffefefef };   ///< the faceplate
    juce::Colour plateEdge  { 0xffe4e4e4 };   ///< header and preset strip
    juce::Colour well       { 0xffd6d6d6 };   ///< recessed areas, meter backgrounds
    juce::Colour hairline   { 0xffb4b4b4 };   ///< section rules
    juce::Colour outline    { 0xff9e9e9e };   ///< knob edges, control outlines

    juce::Colour text1      { 0xff6f6f6f };   ///< legends, values
    juce::Colour text2      { 0xff9a9a9a };   ///< secondary, dimmed, disabled

    juce::Colour knobFace   { 0xff97ddff };   ///< utility knob caps
    juce::Colour knobEdge   { 0xffa6a6a6 };   ///< single-element rings, disengaged switches

    /** What a character knob's cap is mixed half way to -- see faceOf.

        White in both appearances today. It is still a per-appearance token
        rather than the literal `Colours::white` it was inside faceOf() until
        0.2.2, because that literal was the one part of the palette a theme
        could not reach, and because the knob treatments are meant to be able
        to differ per appearance even while they happen not to. */
    juce::Colour knobTint   { 0xffffffff };

    // pointer, ringFace and meterInk were one `pointer` token, #ffffff, until
    // 0.2.2. It was doing five jobs at once, and they stopped agreeing the
    // moment the plate was allowed to go dark: a needle wants maximum contrast
    // against its own face, an annulus wants to read as a raised ring on the
    // plate, and a knob's pointer answers to the cap rather than to either.

    /** The pointer on a knob cap, and the one thing that really differs
        between the two appearances.

        White on the pale plate, and 1.39-1.49:1 against the caps it is drawn
        on -- Frosty's call, taken with the number in front of him: it is the
        suite's look and he would rather have it than the contrast. Near-black
        on the dark plate, where the caps are the lightest thing in the window
        and dark is the handsome choice as well as the legible one, at about
        9.5:1. */
    juce::Colour pointer    { 0xffffffff };

    juce::Colour ringFace   { 0xffffffff };   ///< the selector-ring annulus
    juce::Colour meterInk   { 0xffffffff };   ///< VU needle, ticks and printed scale

    /** The plate a needle meter's scale is printed on.

        Lightened from #3a3a3a in 0.2.2. White numbers were never the limit --
        they still read 9.41:1 here, and would survive a face two steps
        lighter again. What stops it is the hot zone: the amber that marks
        0 VU and above is 4.68:1 on this face and 4.51:1 one step lighter, and
        the red washes visibly toward pink as the face comes up. So the meter
        is as light as its own warning colour allows, not as light as its
        numbers allow.

        A token rather than a panel constant since 0.2.2, which is what lets a
        dark theme raise it above the plate -- a meter window that reads as lit
        rather than as a hole. */
    juce::Colour meterFace  { 0xff464649 };

    juce::Colour track      { 0xff4fb8e8 };   ///< dotted gain tracks and their plus/minus
    juce::Colour trackFill  { 0xff7fd0f2 };   ///< highlights derived from the track colour

    // Darkened from #a6a6a6 in 0.2.2: at the old value a disengaged switch
    // put its white label at 2.43:1, which is close enough to the 1.98-2.55:1
    // of an *engaged* one that on and off were told apart by hue alone.
    juce::Colour switchOff  { 0xff6f7076 };
    juce::Colour switchOn   { 0xfff08eb5 };   ///< an engaged switch, when not the accent

    /** Secondary switches: Hi-Q, Auto, Mono. Module-specific functions, but
        none of them is the module's bypass, so none takes the module's colour.

        Was #4cacdc until 0.2.2, which sat 1.13:1 from `track` -- the same blue
        by any measure that matters, arrived at twice. It now carries track's
        value outright. Kept as its own token rather than folded into `track`
        because the two mean different things and a theme may want to separate
        them again; equal here by intent, which is not the case for `switchOn`
        and BMO EQ's accent, still two names for one pink by accident. */
    juce::Colour switchAlt  { 0xff4fb8e8 };

    /** Polarity inversion, wherever it appears.

        **The rule, for any module added later: a polarity switch is this
        colour.** Not the module's accent, not switchAlt. It flips phase and
        nothing else, it means exactly the same thing on every panel in the
        suite, and it is the one control a person hunts for by sight rather
        than by reading -- so it is the one that most has to look identical
        everywhere. Before 0.2.2 it lit in BMO EQ's pink, the Saturator's
        orange and Util's green.

        White, so an engaged polarity switch is the brightest thing in a row
        of switches and the state reads as an inversion: dark fill with a
        light label off, light fill with a dark one on. It is the only lit
        colour in the suite that carries no hue, which suits the only control
        in the suite that is not an amount of anything. */
    juce::Colour polarity   { 0xffffffff };

    juce::Colour accent     { 0xfff08cb4 };   ///< the module's own colour; see ModuleDef

    /** What a module uses in place of its accent when it is deliberately
        showing no colour identity -- BMO Opto's Tele mode, which runs the
        whole panel in greyscale so that Stressed reads as the louder of the
        two by colour alone.

        Not luminance-matched to any accent, and it cannot be: the four
        accents survive pale knob caps at 1.2-1.3:1 against the plate because
        hue separates them from it, and a grey has no hue to spend. Matched
        for lightness this would be #b8b8b8 and its cap would land at 1.19:1
        with nothing else to tell it from the faceplate. This is a step darker
        so the cap reads at 1.28:1, inside the range the coloured caps already
        occupy. */
    juce::Colour neutral    { 0xffababab };

    juce::Colour meterLow   { 0xff6bbf7a };
    juce::Colour meterHigh  { 0xffe0b040 };
    juce::Colour meterClip  { 0xffe0685a };
    juce::Colour meterGr    { 0xff4fb8e8 };   ///< gain reduction, for the modules that show it

    //== Fixed, not themable ===================================================
    static constexpr float corner       = 3.0f;
    static constexpr float hairlineWeight = 1.0f;
    static constexpr float knobStroke   = 2.2f;
    static constexpr float trackGap     = 10.0f;   ///< face edge to the dotted track
    static constexpr float legendGap    = 12.0f;   ///< track to the legend
    static constexpr float filterLegendGap = 20.0f; ///< a filter has no track, so one gap carries two
};

//== Derived colours ==========================================================
//
// A module states one colour, its accent, and everything else it needs is
// computed from that colour and the plate underneath it. BMO Opto is why:
// its lavender is unreadable as ink and unreadable under white text, so the
// panel hardcoded #9c71c3 for its captions -- against the rule in
// core/AGENTS.md that tokens are the only place colours live. It broke the
// rule because the token it needed did not exist. Module six would have
// hand-rolled its own hex for the same reason.
//
// Deriving against the *current* plate rather than a fixed one is also what
// makes a dark theme nearly free: on #efefef the accents have to be darkened
// hard to be legible, and on a dark plate all four already clear 7:1, so the
// same call returns the accent untouched.

/** WCAG 2.x contrast ratio, 1.0 to 21.0. Order does not matter. */
float contrastRatio (juce::Colour, juce::Colour) noexcept;

/** The accent, moved away from `ground` until it clears `minRatio` against
    it -- darkened on a pale plate, lightened on a dark one. Hue is preserved,
    so the result still reads as the module's own colour.

    This is what a caption, a section legend and a selected legend are set in.
    4.5:1 is the floor for text this size. */
juce::Colour accentTextOn (juce::Colour accent, juce::Colour ground,
                           float minRatio = 4.5f) noexcept;

/** Ink for text drawn *on* a filled accent -- an engaged switch. A darkened
    step of the fill's own hue rather than flat black, so the switch stays
    monochromatic. White was 1.98-2.55:1 on the four accents. */
juce::Colour onAccentOf (juce::Colour fill, float minRatio = 4.5f) noexcept;

/** A module's own colour used as ink on the plate: a knob's caption, a section
    legend, the selected position on a band. Paired with `faceOf`, and the two
    trade places between the appearances.

    On the pale plate the cap is a wash of the accent and the ink is the accent
    stepped down until it reads -- the wash would vanish as text there.

    On the dark plate they swap. The cap carries the module's colour at full
    strength, which is what a knob wants on a dark panel, and the ink is the
    wash, which is what text wants: 9.07-9.73:1 against the plate, against the
    5.87-6.84:1 the raw accent managed. Both directions come out better than
    they went in, which is why this is a swap and not a compromise. */
juce::Colour accentInk (juce::Colour accent) noexcept;

/** The current tokens: the built-in set for whichever appearance is chosen,
    with whatever the user's theme file overrides on top. */
const Tokens& tokens() noexcept;

//== Appearance ===============================================================
//
// Light or dark, stored once per machine rather than per plugin instance and
// per project. It is not a parameter: specs() is frozen and append-only, a
// parameter would be automatable and saved into every session, and a look is
// not something a session should carry. See modules/eq/params.h.
//
// Every open editor already polls for theme changes once a second, so the
// choice reaches every instance -- standalone and in a rack, this plugin and
// the one in the next track -- without any of them knowing about each other.

/** The built-in dark palette. The light one is `Tokens {}`. */
Tokens darkTokens() noexcept;

/** Where the appearance is remembered: one small JSON file beside the themes. */
juce::File uiPreferenceFile();

bool isDarkMode() noexcept;

/** Writes the preference and applies it here immediately; other open editors
    pick it up on their next poll. */
void setDarkMode (bool);

/** A character knob's cap -- and see `accentInk`, which is the other half of
    this and trades places with it between the appearances.

    On the pale plate the cap is the accent washed half way to `knobTint`: the
    accent at full strength would be a very loud knob on a near-white panel.
    On the dark plate it is the accent itself, because there it is not loud, it
    is the module's colour reading properly for the first time -- 5.87-6.84:1
    against the plate with the pointer at 6.13-7.14:1 on top of it.

    Mixing the accent toward the *dark plate* was tried first and rejected on
    sight: pink went to a clean maroon and green to a deep green, but the
    Saturator's orange landed on a brown. Lifting the accent's saturation was
    tried second and measured fine, and the caps shouted. Neither number said
    anything was wrong, which is the argument for rendering over computing. */
inline juce::Colour faceOf (juce::Colour accent) noexcept
{
    return isDarkMode() ? accent
                        : accent.interpolatedWith (tokens().knobTint, 0.5f);
}

//== Theming (option A from the plan: a flat JSON file of token -> hex) ========
//
//  ~/Library/Audio/Presets/LT3 Audio/Themes/Default.json
//
//  { "plate": "#efefef", "accent": "#f08cb4", ... }
//
// Any key that is not a token name is ignored, and any token the file leaves
// out keeps the built-in value. The file is polled by editors on a slow timer,
// so editing it while a plugin is open recolours the panel.

/** Where theme files live. */
juce::File themeDirectory();

/** The file an editor watches. */
juce::File themeFile();

/** Re-reads the theme if the file has changed since the last look. True when
    the tokens changed, in which case the caller repaints. */
bool pollTheme();

/** Every token name the theme file may set, for writing a template. */
juce::StringArray tokenNames();

/** Applies a parsed theme object over `base`, which defaults to the light
    built-in set. A theme is an overlay, not a whole palette, so choosing dark
    and then hand-editing two colours works the way it reads. Exposed for
    tests. */
Tokens tokensFromJson (const juce::var& object, Tokens base = Tokens {});

} // namespace bmo::ui

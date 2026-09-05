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

    juce::Colour knobFace   { 0xff97ddff };   ///< utility knob caps, the selected legend
    juce::Colour knobEdge   { 0xffa6a6a6 };   ///< single-element rings, disengaged switches
    juce::Colour pointer    { 0xffffffff };   ///< pointers, rings, unselected legends
    juce::Colour track      { 0xff4fb8e8 };   ///< dotted gain tracks and their plus/minus
    juce::Colour trackFill  { 0xff7fd0f2 };   ///< highlights derived from the track colour

    juce::Colour switchOff  { 0xffa6a6a6 };
    juce::Colour switchOn   { 0xfff08eb5 };   ///< an engaged switch, when not the accent
    juce::Colour switchAlt  { 0xff4cacdc };   ///< the deeper azure of the Hi-Q switch

    juce::Colour accent     { 0xfff08cb4 };   ///< the module's own colour; see ModuleDef

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

/** Pale version of an accent for a knob face: the accent halfway to white. */
inline juce::Colour faceOf (juce::Colour accent) noexcept
{
    return accent.interpolatedWith (juce::Colours::white, 0.5f);
}

/** The current tokens. The built-in set, with whatever the user's theme file
    overrides on top. */
const Tokens& tokens() noexcept;

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

/** Applies a parsed theme object over the built-in tokens. Exposed for tests. */
Tokens tokensFromJson (const juce::var& object);

} // namespace bmo::ui

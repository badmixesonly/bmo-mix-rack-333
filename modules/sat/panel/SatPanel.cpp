#include "SatPanel.h"
#include "modules/sat/params.h"

namespace bmo::sat
{

namespace
{
    /** The middle. Both ends of this panel are ui::ModulePanel's sections.

        This module took both, which left it 34 px more room than it had, spread
        across the two rows in proportion to what they already were -- 210 and
        130 of a 340 px pair, so 18 and 11. Drive keeps the larger share because
        it had it. The knobs grow with their rows: Drive's face goes from 188 to
        206 px and Tone and Mix from 108 to 119.

        Whatever is left after that is *centred* rather than left at the foot.
        It was all at the foot until 0.2.3, which put 5 px between Drive's name
        and the rule under it and 75 between Mix's name and the rule under that
        -- the whole middle jammed against the top of its span with the slack
        pooled underneath. There is no constant for the span: the output section
        comes off the foot of the content area first, so the middle is by
        definition what remains and re-centres itself if either row changes. */
    constexpr int kDriveRow = 228;   // the one control that gets room
    constexpr int kPairRow  = 141;   // Tone and Mix, side by side

    constexpr int kRule      = 20;

    /** DRIVE's caption, in points, against the suite's 15.

        Drive is the plugin and its name was the same size as the two knobs you
        reach for after it. Rendered at 20, 24, 28 and 32; Frosty took 28,
        2026-09-17. At 28 the word is 144 design px of the 240 the panel has, so
        it is nowhere near the width that would shrink it to fit.

        A caption is charged to its knob's row -- captionRow() comes off the top
        of the cell before the knob squares itself in what is left -- so the row
        below grows by what the larger name takes and the face keeps its size.
        Without that, DRIVE's face would have paid for its own caption. */
    constexpr int kDriveCaption = 28;

    /** How far each character knob's caption comes up into the air under its own
        face, in design px, as ui::PlainKnob::setCaptionLift.

        The suite sits at 30 render px from face to caption ink: BMO Util, BMO
        Opto's MAKEUP and LTV Comp all measure exactly 30. This panel was at 74
        on DRIVE and 68 on the pair, the loosest in the rack. INPUT and OUTPUT
        were already at 30 and are not touched.

        The two numbers differ because the lift is applied under captions of two
        different sizes, and a taller caption sits lower in its own box: at 28 pt
        DRIVE needs 25 where the pair's 15 pt needs 19. Both land on 30. Re-measure
        rather than re-derive if either size changes. */
    constexpr int kDriveLift = 25;
    constexpr int kPairLift  = 19;

    /** What each section's content is nudged down by to centre its *ink* rather
        than its boxes, in design px. Frosty's call between the two, 2026-09-17.

        A knob's box carries more air above its face than its lifted caption
        leaves under it, so a section holding centred boxes still reads high:
        centred that way, DRIVE sat 44 px from the rule above and 61 from the one
        below, and the pair 44 and 49. Nudged, they measure 52/53 and 46/47, and
        the panel's largest bare band falls from 66 px to 53.

        Measured constants, so they are only true for the sizes above. */
    constexpr int kDriveNudge = 8;
    constexpr int kPairNudge  = 2;

    constexpr int kSwitchWidth  = ui::Tokens::switchWidth;
    constexpr int kSwitchHeight = ui::Tokens::switchHeight;
}

SatPanel::SatPanel (ui::ModuleContext ctx)
    : ModulePanel (std::move (ctx)),
      inputGain   (context.params.param (Index::inputGain),   "INPUT"),
      drive       (context.params.param (Index::drive),       "DRIVE", ui::Knob::Style::character, 0.66f, context.def.accent),
      tone        (context.params.param (Index::tone),        "TONE",  ui::Knob::Style::character, 0.46f, context.def.accent),
      mix         (context.params.param (Index::mix),         "MIX",   ui::Knob::Style::character, 0.46f, context.def.accent),
      outputLevel (context.params.param (Index::outputLevel), "OUTPUT"),
      satIn    (context.params.param (Index::satIn),    "SAT", context.def.accent),
      phase    (context.params.param (Index::phase),    ui::BmoLookAndFeel::phaseGlyph(), ui::tokens().polarity),
      autoGain (context.params.param (Index::autoGain), "AUTO", ui::tokens().switchAlt)

{
    for (auto* c : std::initializer_list<juce::Component*> {
             &inputGain, &drive, &tone, &mix, &satIn, &phase, &autoGain, &outputLevel })
        addAndMakeVisible (c);

    for (auto* k : { &inputGain, &outputLevel })
        styleTrimKnob (*k);

    // Drive is what this module is, and now its name says so. See kDriveCaption.
    drive.setCaptionSize ((float) kDriveCaption);

    // The names come up off the foot of their knobs' boxes and into the air
    // under each face, so they sit the same distance from it as BMO Util's and
    // the two compressors' do. See kDriveLift.
    drive.setCaptionLift (kDriveLift);

    for (auto* k : { &tone, &mix })
        k->setCaptionLift (kPairLift);

    // Polarity is white in every module; its label is what says which module.
    phase.setActiveInkFrom (context.def.accent);
}

void SatPanel::resized()
{
    clearRules();
    // 4, not 6: the same content inset BMO EQ and Util use, so all three
    // panels measure from the same origin and the matched rows below actually
    // land where the arithmetic says.
    auto area = getLocalBounds().reduced (kPad, 4);

    const auto rule = [&] (const juce::String& text)
    {
        addRule (area.removeFromTop (kRule), text);
    };

    // Both sections, output first off the foot. The 6 px of air that used to
    // sit between the input knob and its rule is gone with them: the section
    // has none, and the rule row's own half-height is the clearance.
    const auto out = takeOutputSection (area);
    const auto in  = takeInputSection (area);

    inputGain.setBounds (in.knob);
    addRule (in.rule, {});

    // The row carries the taller caption, so the face does not pay for it.
    // See kDriveCaption.
    const int driveRow = kDriveRow
                       + juce::roundToInt ((float) kDriveCaption * 1.2f)
                       - juce::roundToInt (15.0f * 1.2f);   // the caption this row was drawn for

    // What is left is the middle, and it is two sections with a rule between
    // them. Each centres its own content in its own span rather than the pair
    // of them centring as one block -- Frosty, 2026-09-17. Centred as one, the
    // slack pooled above DRIVE and under MIX and neither section sat in the
    // middle of anything.
    const int slack      = area.getHeight() - (driveRow + kRule + kPairRow);
    const int driveSpan  = slack / 2;
    const int pairSpan   = slack - driveSpan;

    // Each section centres what you can see rather than what the layout holds.
    // See kDriveNudge.
    area.removeFromTop (driveSpan / 2 + kDriveNudge);
    drive.setBounds (area.removeFromTop (driveRow));
    area.removeFromTop (driveSpan - driveSpan / 2 - kDriveNudge);

    // Tone and Mix share a row: neither is the reason you reached for this,
    // and side by side they read as the two things you adjust after the fact.
    rule ({});

    area.removeFromTop (pairSpan / 2 + kPairNudge);

    {
        auto pair = area.removeFromTop (kPairRow);
        const auto half = pair.getWidth() / 2;
        tone.setBounds (pair.removeFromLeft (half));
        mix.setBounds (pair);
    }

    addRule (out.rule, {});

    {
        constexpr int gap = ui::Tokens::switchGap;

        auto group = out.switches.withSizeKeepingCentre (kSwitchWidth * 3 + gap * 2, kSwitchHeight);
        satIn.setBounds (group.removeFromLeft (kSwitchWidth));
        group.removeFromLeft (gap);
        phase.setBounds (group.removeFromLeft (kSwitchWidth));
        group.removeFromLeft (gap);
        autoGain.setBounds (group);
    }

    // The output meter used to share this row, out at the right margin. It is
    // gone, so the knob has the row to itself.
    outputLevel.setBounds (out.knob);
}

} // namespace bmo::sat

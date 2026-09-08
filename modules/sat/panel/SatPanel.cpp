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

    // Polarity is white in every module; its label is what says which module.
    phase.setActiveInkFrom (context.def.accent);
}

void SatPanel::paintPanel (juce::Graphics& g)
{
    for (const auto& r : rules)
    {
        if (r.text.isEmpty())
            drawRule (g, r.row);
        else
            drawRuleLegend (g, r.row, r.text, context.def.accent);
    }
}

void SatPanel::resized()
{
    rules.clear();
    // 4, not 6: the same content inset BMO EQ and Util use, so all three
    // panels measure from the same origin and the matched rows below actually
    // land where the arithmetic says.
    auto area = getLocalBounds().reduced (kPad, 4);

    const auto rule = [&] (const juce::String& text)
    {
        rules.push_back ({ area.removeFromTop (kRule), text });
    };

    // Both sections, output first off the foot. The 6 px of air that used to
    // sit between the input knob and its rule is gone with them: the section
    // has none, and the rule row's own half-height is the clearance.
    const auto out = takeOutputSection (area);
    const auto in  = takeInputSection (area);

    inputGain.setBounds (in.knob);
    rules.push_back ({ in.rule, {} });

    // What is left is the middle, and it centres in it.
    area.removeFromTop ((area.getHeight() - (kDriveRow + kRule + kPairRow)) / 2);

    drive.setBounds (area.removeFromTop (kDriveRow));

    // Tone and Mix share a row: neither is the reason you reached for this,
    // and side by side they read as the two things you adjust after the fact.
    rule ({});

    {
        auto pair = area.removeFromTop (kPairRow);
        const auto half = pair.getWidth() / 2;
        tone.setBounds (pair.removeFromLeft (half));
        mix.setBounds (pair);
    }

    rules.push_back ({ out.rule, {} });

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

#include "EqPanel.h"
#include "modules/eq/params.h"

namespace bmo::eq
{

namespace
{
    // The column's budget, top to bottom, inside the padding: it adds up to
    // the common content height with a few pixels over. Every module shares
    // the height, so the EQ, the tallest of them, sets the pace.
    // The input and output sections are ui::ModulePanel's now -- the knob rows,
    // the rules and the switch row all come from there. What is left here is
    // what this module actually is.
    constexpr int kBandRow   = 112;   // knob, gap, dotted track, gap, legend
    constexpr int kFilterRow = 76;

    constexpr int kSwitchWidth  = ui::Tokens::switchWidth;
    constexpr int kSwitchHeight = ui::Tokens::switchHeight;
}

EqPanel::EqPanel (ui::ModuleContext ctx)
    : ModulePanel (std::move (ctx)),
      inputGain   (context.params.param (Index::inputGain),   "INPUT"),
      outputLevel (context.params.param (Index::outputLevel), "OUTPUT"),
      // The last argument alternates down the panel: inset, outset, inset. Its
      // job is to stop the three bands lining their end labels up into a
      // column of numbers down the middle -- the high shelf's lowest sits
      // directly above the mid bell's highest, with only a rule between them.
      // A fourth band would carry on alternating, so it takes false.
      high     (context.params.param (Index::hfFreq),  context.params.spec (Index::hfFreq),  &context.params.param (Index::hfGain),  context.def.accent, false),
      mid      (context.params.param (Index::midFreq), context.params.spec (Index::midFreq), &context.params.param (Index::midGain), context.def.accent, true),
      low      (context.params.param (Index::lfFreq),  context.params.spec (Index::lfFreq),  &context.params.param (Index::lfGain),  context.def.accent, false),
      highPass (context.params.param (Index::hpfFreq), context.params.spec (Index::hpfFreq), nullptr, context.def.accent),
      eqIn   (context.params.param (Index::eqIn),   "EQL", context.def.accent),
      phase  (context.params.param (Index::phase),  ui::BmoLookAndFeel::phaseGlyph(), ui::tokens().polarity),
      midHiQ (context.params.param (Index::midHiQ), "HI-Q", ui::tokens().switchAlt)
{
    for (auto* c : std::initializer_list<juce::Component*> {
             &inputGain, &high, &mid, &low, &highPass, &eqIn, &phase, &midHiQ, &outputLevel })
        addAndMakeVisible (c);

    for (auto* k : { &inputGain, &outputLevel })
        styleTrimKnob (*k);

    // Polarity is white in every module; its label is what says which module.
    phase.setActiveInkFrom (context.def.accent);
}

void EqPanel::resized()
{
    clearRules();
    auto area = getLocalBounds().reduced (kPad, 4);

    const auto rule = [&] (const juce::String& text)
    {
        addRule (area.removeFromTop (kRuleRow), text);
    };

    // Both sections, and the output one comes off the foot first so the bands
    // in between get what is left rather than a number that has to be redone
    // every time one of them changes.
    const auto out = takeOutputSection (area);
    const auto in  = takeInputSection (area);

    inputGain.setBounds (in.knob);

    // The input section's rule is this panel's HIGH rule -- the same row, and
    // the only one in the suite that carries a legend. A module with nothing
    // to name there pushes it with an empty string.
    addRule (in.rule, "HIGH");
    high.setBounds (area.removeFromTop (kBandRow));

    rule ("MID");
    mid.setBounds (area.removeFromTop (kBandRow));

    rule ("LOW");
    low.setBounds (area.removeFromTop (kBandRow));

    rule ("LO-CUT");
    highPass.setBounds (area.removeFromTop (kFilterRow));

    addRule (out.rule, {});

    {
        // Hi-Q used to sit with the mid band. At the common height there is
        // no row for it there, so it joins the switches, where its azure
        // still says it is not one of the other two.
        auto group = out.switches.withSizeKeepingCentre (kSwitchWidth * 3 + ui::Tokens::switchGap * 2,
                                                         kSwitchHeight);
        constexpr int gap = ui::Tokens::switchGap;

        eqIn.setBounds (group.removeFromLeft (kSwitchWidth));
        group.removeFromLeft (gap);
        phase.setBounds (group.removeFromLeft (kSwitchWidth));
        group.removeFromLeft (gap);
        midHiQ.setBounds (group);
    }

    // Every knob on the panel shares one centre line, output included. The
    // output meter used to sit out at the right margin of this row; it is
    // gone, so the knob has the row to itself and is centred in it like
    // every other.
    outputLevel.setBounds (out.knob);
}

} // namespace bmo::eq

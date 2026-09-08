#include "EqPanel.h"
#include "modules/eq/params.h"

namespace bmo::eq
{

namespace
{
    // The column's budget, top to bottom, inside the padding: it adds up to
    // the common content height with a few pixels over. Every module shares
    // the height, so the EQ, the tallest of them, sets the pace.
    constexpr int kGainRow   = 78;    // knob plus the name under it
    constexpr int kBandRow   = 112;   // knob, gap, dotted track, gap, legend
    constexpr int kFilterRow = 76;
    constexpr int kSwitchRow = 28;

    constexpr int kSwitchWidth  = ui::Tokens::switchWidth;
    constexpr int kSwitchHeight = ui::Tokens::switchHeight;
}

EqPanel::EqPanel (ui::ModuleContext ctx)
    : ModulePanel (std::move (ctx)),
      inputGain   (context.params.param (Index::inputGain),   "INPUT"),
      outputLevel (context.params.param (Index::outputLevel), "OUTPUT"),
      high     (context.params.param (Index::hfFreq),  context.params.spec (Index::hfFreq),  &context.params.param (Index::hfGain),  context.def.accent),
      mid      (context.params.param (Index::midFreq), context.params.spec (Index::midFreq), &context.params.param (Index::midGain), context.def.accent),
      low      (context.params.param (Index::lfFreq),  context.params.spec (Index::lfFreq),  &context.params.param (Index::lfGain),  context.def.accent),
      highPass (context.params.param (Index::hpfFreq), context.params.spec (Index::hpfFreq), nullptr, context.def.accent),
      eqIn   (context.params.param (Index::eqIn),   "EQL", context.def.accent),
      phase  (context.params.param (Index::phase),  ui::BmoLookAndFeel::phaseGlyph(), ui::tokens().polarity),
      midHiQ (context.params.param (Index::midHiQ), "HI-Q", ui::tokens().switchAlt),
      meter  (context.peak, context.rms)
{
    for (auto* c : std::initializer_list<juce::Component*> {
             &inputGain, &high, &mid, &low, &highPass, &eqIn, &phase, &midHiQ, &outputLevel, &meter })
        addAndMakeVisible (c);

    // Polarity is white in every module; its label is what says which module.
    phase.setActiveInkFrom (context.def.accent);
}

void EqPanel::paintPanel (juce::Graphics& g)
{
    for (const auto& r : rules)
    {
        if (r.text.isEmpty())
            drawRule (g, r.row);
        else
            drawRuleLegend (g, r.row, r.text, context.def.accent);
    }
}

void EqPanel::resized()
{
    rules.clear();
    auto area = getLocalBounds().reduced (kPad, 4);

    const auto rule = [&] (const juce::String& text)
    {
        rules.push_back ({ area.removeFromTop (kRuleRow), text });
    };

    inputGain.setBounds (area.removeFromTop (kGainRow));

    rule ("HIGH");
    high.setBounds (area.removeFromTop (kBandRow));

    rule ("MID");
    mid.setBounds (area.removeFromTop (kBandRow));

    rule ("LOW");
    low.setBounds (area.removeFromTop (kBandRow));

    rule ("LO-CUT");
    highPass.setBounds (area.removeFromTop (kFilterRow));

    rule ({});

    {
        // Hi-Q used to sit with the mid band. At the common height there is
        // no row for it there, so it joins the switches, where its azure
        // still says it is not one of the other two.
        auto switches = area.removeFromTop (kSwitchRow);
        constexpr int gap = ui::Tokens::switchGap;

        auto group = switches.withSizeKeepingCentre (kSwitchWidth * 3 + gap * 2, kSwitchHeight);
        eqIn.setBounds (group.removeFromLeft (kSwitchWidth));
        group.removeFromLeft (gap);
        phase.setBounds (group.removeFromLeft (kSwitchWidth));
        group.removeFromLeft (gap);
        midHiQ.setBounds (group);
    }

    {
        // Every knob on the panel shares one centre line, output included.
        // The meter is not a knob and does not join it: it goes out to the
        // right margin, where it reads as an indicator beside the strip.
        auto bottom = area.removeFromTop (kGainRow);

        meter.setBounds (bottom.withTrimmedTop (2).withTrimmedBottom (20).removeFromRight (44));
        outputLevel.setBounds (bottom);
    }
}

} // namespace bmo::eq

#include "UtilPanel.h"
#include "modules/util/params.h"

namespace bmo::util
{

namespace
{
    constexpr int kKnobRow   = 126;
    constexpr int kSwitchRow = 34;
    constexpr int kSwitchGap = 8;
    constexpr int kMeterRow  = 100;

    constexpr int kSwitchWidth  = ui::Tokens::switchWidth;
    constexpr int kSwitchHeight = ui::Tokens::switchHeight;
}

UtilPanel::UtilPanel (ui::ModuleContext ctx)
    : ModulePanel (std::move (ctx)),
      gain  (context.params.param (Index::gain),  "GAIN"),
      pan   (context.params.param (Index::pan),   "PAN",   ui::Knob::Style::character, 0.5f, context.def.accent),
      width (context.params.param (Index::width), "WIDTH", ui::Knob::Style::character, 0.5f, context.def.accent),
      phaseL (context.params.param (Index::phaseL), ui::BmoLookAndFeel::phaseGlyph() + " L", ui::tokens().polarity),
      phaseR (context.params.param (Index::phaseR), ui::BmoLookAndFeel::phaseGlyph() + " R", ui::tokens().polarity),
      mono   (context.params.param (Index::mono),   "MONO", context.def.accent),
      meter  (context.peak, context.rms)
{
    for (auto* c : std::initializer_list<juce::Component*> {
             &gain, &pan, &width, &phaseL, &phaseR, &mono, &meter })
        addAndMakeVisible (c);

    // Polarity is white in every module; its label is what says which module.
    for (auto* p : { &phaseL, &phaseR })
        p->setActiveInkFrom (context.def.accent);
}

void UtilPanel::paintPanel (juce::Graphics& g)
{
    for (const auto& r : rules)
    {
        if (r.text.isEmpty())
            drawRule (g, r.row);
        else
            drawRuleLegend (g, r.row, r.text, context.def.accent);
    }
}

void UtilPanel::resized()
{
    rules.clear();
    auto area = getLocalBounds().reduced (kPad, 4);

    const auto rule = [&] (const juce::String& text)
    {
        rules.push_back ({ area.removeFromTop (kRuleRow), text });
    };

    gain.setBounds (area.removeFromTop (kKnobRow));

    rule ("PAN");
    pan.setBounds (area.removeFromTop (kKnobRow));

    rule ("WIDTH");
    width.setBounds (area.removeFromTop (kKnobRow));

    rule ("POLARITY");

    for (auto* s : { &phaseL, &phaseR, &mono })
    {
        s->setBounds (area.removeFromTop (kSwitchRow).withSizeKeepingCentre (kSwitchWidth, kSwitchHeight));
        area.removeFromTop (kSwitchGap);
    }

    rule ({});

    const auto meterRow = area.removeFromTop (kMeterRow).reduced (0, 4);
    meter.setBounds (meterRow.withSizeKeepingCentre (44, meterRow.getHeight()));
}

} // namespace bmo::util

#include "OptoPanel.h"
#include "modules/opto/params.h"

namespace bmo::opto
{

namespace
{
    constexpr int kKnobWidth  = 70;
    constexpr int kKnobHeight = 170;
    constexpr int kMeterWidth = 44;
}

OptoPanel::OptoPanel (ui::ModuleContext ctx)
    : ModulePanel (std::move (ctx)),
      crush (context.params.param (Index::crush), "CRUSH <3",
             ui::Knob::Style::character, 0.62f, context.def.accent),
      level (context.params.param (Index::level), "LEVEL",
             ui::Knob::Style::character, 0.62f, context.def.accent),
      meter (context.inputRms, context.rms, context.gainReductionDb)
{
    for (auto* c : std::initializer_list<juce::Component*> { &crush, &meter, &level })
        addAndMakeVisible (c);
}

void OptoPanel::resized()
{
    const auto area = getLocalBounds().reduced (kPad, 4);
    const auto centreY = area.getCentreY();

    crush.setBounds ({ area.getX(), centreY - kKnobHeight / 2, kKnobWidth, kKnobHeight });
    level.setBounds ({ area.getRight() - kKnobWidth, centreY - kKnobHeight / 2, kKnobWidth, kKnobHeight });
    meter.setBounds (area.withSizeKeepingCentre (kMeterWidth, area.getHeight() - 80));
}

} // namespace bmo::opto

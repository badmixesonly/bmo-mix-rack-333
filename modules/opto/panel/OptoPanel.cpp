#include "OptoPanel.h"
#include "modules/opto/params.h"

namespace bmo::opto
{

namespace
{
    constexpr int kKnobWidth   = 70;
    constexpr int kKnobHeight  = 160;
    constexpr int kMeterWidth  = 44;
    constexpr int kSwitchRow   = 28;
}

OptoPanel::OptoPanel (ui::ModuleContext ctx)
    : ModulePanel (std::move (ctx)),
      crush (context.params.param (Index::crush), "CRUSH <3",
             ui::Knob::Style::character, 0.62f, context.def.accent),
      level (context.params.param (Index::level), "LEVEL",
             ui::Knob::Style::character, 0.62f, context.def.accent),
      meter (context.inputRms, context.rms, context.gainReductionDb),
      mode  (context.params.param (Index::mode),  "STRESSED", context.def.accent),
      link  (context.params.param (Index::link),  "LINK",     context.def.accent),
      color (context.params.param (Index::color), "COLOR",    context.def.accent)
{
    for (auto* c : std::initializer_list<juce::Component*> { &crush, &meter, &level, &mode, &link, &color })
        addAndMakeVisible (c);

    lastModeWasStressed = context.params.param (Index::mode).getValue() > 0.5f;
    color.setVisible (lastModeWasStressed);
    color.setSwitchEnabled (lastModeWasStressed);

    startTimerHz (15);
}

OptoPanel::~OptoPanel() { stopTimer(); }

void OptoPanel::timerCallback()
{
    // Color has no off state in Tele mode (DspCore locks it on regardless
    // of the parameter -- see DspCore::process()), so the switch itself is
    // hidden and disabled there rather than left on-screen doing nothing.
    // Polled rather than a parameter listener, same as OutputMeter/
    // DynamicsMeter's own timers -- there's no cross-thread marshaling to
    // get right for a once-in-a-while UI state change like this one.
    const auto stressed = context.params.param (Index::mode).getValue() > 0.5f;

    if (stressed != lastModeWasStressed)
    {
        lastModeWasStressed = stressed;
        color.setVisible (stressed);
        color.setSwitchEnabled (stressed);
    }
}

void OptoPanel::resized()
{
    auto area = getLocalBounds().reduced (kPad, 4);

    auto switchRow = area.removeFromBottom (kSwitchRow);
    const auto switchWidth = switchRow.getWidth() / 3;

    mode.setBounds  (switchRow.removeFromLeft (switchWidth).reduced (4, 2));
    link.setBounds  (switchRow.removeFromLeft (switchWidth).reduced (4, 2));
    color.setBounds (switchRow.reduced (4, 2));

    const auto centreY = area.getCentreY();

    crush.setBounds ({ area.getX(), centreY - kKnobHeight / 2, kKnobWidth, kKnobHeight });
    level.setBounds ({ area.getRight() - kKnobWidth, centreY - kKnobHeight / 2, kKnobWidth, kKnobHeight });
    meter.setBounds (area.withSizeKeepingCentre (kMeterWidth, area.getHeight() - 80));
}

} // namespace bmo::opto

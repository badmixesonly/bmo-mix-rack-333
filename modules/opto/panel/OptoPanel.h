#pragma once

#include "core/product/ModuleDef.h"

namespace bmo::opto
{

/** CRUSH, a centred meter, LEVEL, and a bottom row of three switches --
    Mode, Link, Color. The meter is still the centrepiece; the switches are
    a strip under everything else, the way EqPanel's own switch row sits
    below its bands.

    Color's switch is disabled and hidden in Tele mode, since Tele has no
    off state for it (DspCore locks it on regardless of the parameter) --
    see timerCallback(). Mode/Link/Color's labels ("STRESSED", "LINK",
    "COLOR") are placeholders, same as the parameter names themselves; see
    modules/opto/params.h. */
class OptoPanel final : public ui::ModulePanel,
                        private juce::Timer
{
public:
    explicit OptoPanel (ui::ModuleContext);
    ~OptoPanel() override;

    void resized() override;

private:
    void timerCallback() override;

    ui::PlainKnob crush, level;
    ui::DynamicsMeter meter;
    ui::SwitchButton mode, link, color;

    bool lastModeWasStressed = false;
};

} // namespace bmo::opto

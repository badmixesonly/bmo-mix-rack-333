#pragma once

#include "core/product/ModuleDef.h"

namespace bmo::opto
{

/** Mode on top, then three even rows -- COMP, the VU meter with its own
    IN/GR/OUT row, and MAKEUP -- with LINK and COLOR stacked underneath.

    Mode sits above everything because it is the one control that changes
    what the other two knobs mean; it reads as the module's character
    switch rather than as one more option in a row of them. It is a single
    button that says which mode it is in, not the TELE/ELD pair 0.2.0 had:
    two buttons for one either/or spent twice the width saying it, and the
    pair was the only place in the suite where a choice was drawn that way.

    Every switch here is a juce::ToggleButton so BmoLookAndFeel draws it,
    which is what makes these read as the same control as BMO Util's
    polarity switches -- grey with white text disengaged, the module's
    dark purple with a faint glow engaged. The meter's IN/GR/OUT row is
    the same button in radio behaviour: clicking does not toggle, the
    handlers set the trio's states.

    Color is disabled but left in place in Tele mode, since Tele has no off
    state for it (DspCore locks it on regardless of the parameter) -- see
    timerCallback(). 0.2.0 hid it, which moved LINK every time the mode
    changed. "TELE"/"ELD"/"COLOR" are the module's working names, not
    necessarily final -- see modules/opto/params.h. */
class OptoPanel final : public ui::ModulePanel,
                        private juce::Timer
{
public:
    explicit OptoPanel (ui::ModuleContext);
    ~OptoPanel() override;

    void resized() override;

private:
    void timerCallback() override;

    /** Sets `param` to `normalisedValue` (0 or 1, for a two-choice param)
        via the standard begin/set/end gesture triplet -- there's no
        attachment class for "click sets a specific value" the way
        ButtonParameterAttachment covers "click toggles", so this is done
        directly on the underlying parameter. */
    void setChoice (juce::RangedAudioParameter& param, float normalisedValue);

    ui::PlainKnob crush, level;
    ui::DynamicsMeter meter;

    juce::ToggleButton modeButton;
    juce::ToggleButton meterInButton, meterOutButton, meterGrButton;

    ui::SwitchButton link, color;

    bool lastModeWasStressed = false;
};

} // namespace bmo::opto

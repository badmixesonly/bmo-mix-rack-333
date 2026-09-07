#pragma once

#include "core/product/ModuleDef.h"

namespace bmo::opto
{

/** TELE and ELD stacked at the head, then COMP, the VU meter with its own
    IN/GR/OUT row, and MAKEUP, with LINK and COLOR stacked at the foot.

    Mode sits above everything because it is the one control that changes what
    the other two knobs mean. 0.2.0 drew it as a TELE/ELD pair side by side,
    which spent twice the width saying one either/or; 0.2.1 replaced that with
    a single button carrying its own state. This is the third arrangement and
    it keeps what each was after: the pair is back, so both modes are named on
    the panel whichever one is active, but stacked rather than abreast, so it
    costs height -- which this module has to spare -- instead of width, which
    it does not. It also mirrors the LINK/COLOR stack at the foot, and it
    fills the dead band that sat under a lone mode button.

    The two modes are coloured, not just labelled. Tele runs the whole panel
    in greyscale with a red hot zone on the meter; Stressed is the module's
    lavender with the hot zone in the pale lavender of the knob caps. So which
    circuit is in play reads from across the room, before any label does.

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

    /** The palette for a mode. Tele runs the panel in greyscale with a red
        hot zone on the meter; Stressed is the module's lavender throughout,
        with the hot zone in the pale lavender of the knob caps. */
    juce::Colour accentFor (bool stressed) const;
    juce::Colour hotColourFor (bool stressed) const;

    /** Pushes that palette into every control. Cheap, and only called when the
        mode actually changes. */
    void applyModeColours (bool stressed);

    ui::PlainKnob crush, level;
    ui::DynamicsMeter meter;

    juce::ToggleButton teleButton, eldButton;
    juce::ToggleButton meterInButton, meterOutButton, meterGrButton;

    ui::SwitchButton link, color;

    bool lastModeWasStressed = false;
};

} // namespace bmo::opto

#pragma once

#include "core/product/ModuleDef.h"

namespace bmo::opto
{

/** Laid out this way in 0.2.0, replacing 0.1.0's two knobs squeezed either
    side of the meter with their captions clipped.

    COMP on top, the VU meter (with its own IN/OUT/GR button row) in the
    middle, MAKEUP and the Mode/Link/Color switches on the bottom -- three
    even rows, per Frosty's 2026-09-06 layout note. Mode is TELE/ELD, a
    two-way segmented pair rather than a single on/off switch, so both
    states read on screen at once instead of one label standing in for
    "off" the way a boolean SwitchButton would.

    Color's switch is disabled and hidden in Tele mode, since Tele has no
    off state for it (DspCore locks it on regardless of the parameter) --
    see timerCallback(). "TELE"/"ELD"/"COLOR" are the module's working
    names, not necessarily final -- see modules/opto/params.h. */
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

    juce::TextButton teleButton, eldButton;
    juce::TextButton meterInButton, meterOutButton, meterGrButton;

    ui::SwitchButton link, color;

    bool lastModeWasStressed = false;
};

} // namespace bmo::opto

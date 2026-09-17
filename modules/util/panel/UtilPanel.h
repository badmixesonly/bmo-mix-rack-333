#pragma once

#include "core/product/ModuleDef.h"

namespace bmo::util
{

/** Gain, then the stereo image -- pan, width, mono --
    then the two polarity flips. The narrow one: it goes at the front of a
    chain and stays out of the way.

    No output meter, which is a deliberate exception to the rule in
    modules/AGENTS.md that every module ends with one. */
class UtilPanel final : public ui::ModulePanel,
                        private juce::Timer
{
public:
    explicit UtilPanel (ui::ModuleContext);
    ~UtilPanel() override;

    void resized() override;

private:
    /** Reads MONO back and dims WIDTH while it is on -- see the call site for
        why. Polled at 15 Hz rather than listened for, the same as BMO Opto's
        mode, so host automation and a preset land where a click does. */
    void timerCallback() override;

    ui::PlainKnob gain, pan, width;
    ui::SwitchButton phaseL, phaseR, mono;

    bool lastMonoWasOn = false;
};

} // namespace bmo::util

#pragma once

#include "core/product/ModuleDef.h"

namespace bmo::eq
{

/** A channel strip, not an analyser.

    One narrow column, gain at the top, the bands below it as concentric
    pairs with their frequencies legended around the ring, the low cut under
    those, the switches, and the output at the bottom.

    There is no response curve, no analyser, and no numeric readout on any cut
    or boost -- a gain control is marked with a plus and a minus and nothing
    else. That came from an engineer who has spent years on the hardware: the
    numbers make people mix with their eyes, hunting a tidy figure and
    flinching from a large move. Frequency legends stay, because a switch
    position is not an amount.
*/
class EqPanel final : public ui::ModulePanel
{
public:
    explicit EqPanel (ui::ModuleContext);

    void resized() override;

private:
    void paintPanel (juce::Graphics&) override;

    ui::PlainKnob inputGain, outputLevel;
    ui::ConcentricBand high, mid, low, highPass;
    ui::SwitchButton eqIn, phase, midHiQ;

    struct Rule { juce::Rectangle<int> row; juce::String text; };
    std::vector<Rule> rules;
};

} // namespace bmo::eq

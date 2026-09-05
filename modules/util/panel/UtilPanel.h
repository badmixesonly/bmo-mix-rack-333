#pragma once

#include "core/product/ModuleDef.h"

namespace bmo::util
{

/** Gain, pan, width, two polarity switches, mono, and a meter. The narrow
    one: it goes at the front of a chain and stays out of the way. */
class UtilPanel final : public ui::ModulePanel
{
public:
    explicit UtilPanel (ui::ModuleContext);

    void resized() override;

private:
    void paintPanel (juce::Graphics&) override;

    ui::PlainKnob gain, pan, width;
    ui::SwitchButton phaseL, phaseR, mono;
    ui::OutputMeter meter;

    struct Rule { juce::Rectangle<int> row; juce::String text; };
    std::vector<Rule> rules;
};

} // namespace bmo::util

#pragma once

#include "core/product/ModuleDef.h"

namespace bmo::util
{

/** Gain, then the stereo image -- pan, width, mono --
    then the two polarity flips. The narrow one: it goes at the front of a
    chain and stays out of the way.

    No output meter, which is a deliberate exception to the rule in
    modules/AGENTS.md that every module ends with one. */
class UtilPanel final : public ui::ModulePanel
{
public:
    explicit UtilPanel (ui::ModuleContext);

    void resized() override;

private:
    void paintPanel (juce::Graphics&) override;

    ui::PlainKnob gain, pan, width;
    ui::SwitchButton phaseL, phaseR, mono;

    struct Rule { juce::Rectangle<int> row; juce::String text; };
    std::vector<Rule> rules;
};

} // namespace bmo::util

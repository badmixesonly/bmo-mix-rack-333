#pragma once

#include "core/product/ModuleDef.h"

namespace bmo::sat
{

/** Input, drive, tone and mix, three switches, output. Drive is the plugin:
    it gets the middle of the panel and the largest face. */
class SatPanel final : public ui::ModulePanel
{
public:
    explicit SatPanel (ui::ModuleContext);

    void resized() override;

private:

    ui::PlainKnob inputGain, drive, tone, mix, outputLevel;
    ui::SwitchButton satIn, phase, autoGain;

};

} // namespace bmo::sat

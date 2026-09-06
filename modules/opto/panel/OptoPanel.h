#pragma once

#include "core/product/ModuleDef.h"

namespace bmo::opto
{

/** CRUSH, a centred meter, LEVEL -- the meter is the centrepiece, not a
    corner afterthought, per the brief this module was built from. No rules,
    no sections: there is only the one composition. */
class OptoPanel final : public ui::ModulePanel
{
public:
    explicit OptoPanel (ui::ModuleContext);

    void resized() override;

private:
    ui::PlainKnob crush, level;
    ui::DynamicsMeter meter;
};

} // namespace bmo::opto

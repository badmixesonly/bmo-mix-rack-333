#pragma once

#include "core/product/ModuleDef.h"

namespace bmo::dim
{

/** DETUNE over CENTS and DIFFUSE, then WIDTH on its own, then SHUFFLE with its
    FREQ, and ROTATE and ASYM at the foot.

    **RATE and DEPTH have no controls.** They are the diffuse stage's LFO, and
    the listening pass on 2026-09-09 found neither audible enough to earn the
    space -- so they are fixed at their defaults, 0.40 Hz and 50 %. The
    parameters stay in `params.h`: the IDs are permanent and append-only, and a
    host session that automated them must still load. This is the stronger
    answer to the "three dead knobs on a fresh insert" problem than dimming
    them would have been, since with DIFFUSE at its 0 % default these two did
    nothing until it was raised.

    Built on BMO Opto's panel rather than on BMO EQ's: blocks placed from the
    top on one derived gap, no input or output section reserved, and no section
    rules. The rule about rules is the reason -- a rule is a divider, and a
    panel that is one idea has nothing to divide. This module is one idea, a
    stereo image, taken in three passes; drawing a line between the passes would
    mark a boundary that is not there any more than it is on Opto.

    The order down the panel is signal order -- generate, diffuse, image --
    which is *not* the order in params.h. That one is reach-for-first, because
    it is permanent and because it is what a host's automation list shows. The
    two are independent and each is right for the list it is in.

    WIDTH sits in the middle at full size, where Opto puts its meter. It is the
    control this panel is opened for, it is the only one anybody reaches for
    without thinking, and it is the only knob here that earns 92 px. Everything
    else is paired at 64, which is between BMO EQ's 56 and Opto's 92 and is what
    ten controls in a 220 px column can afford.

    DETUNE is a switch rather than a zero position on the CENTS knob. The stage
    it gates is the only part of the module that manufactures signal rather than
    shaping it, so it is worth being able to take out and put back without
    losing the amount you had set -- and worth reading as off at a glance.

    **A goniometer is the meter this panel wants, and it is deliberately not
    here yet.** Deferred 2026-09-08, on functionality first.

    It is not a panel change. `ui::ModuleContext` hands a panel five
    `std::function<float()>` and nothing else, and `ModuleEngine` fills them
    from `Meter` classes that reduce a block to a scalar -- so there is no path
    from the audio thread to the UI that carries L/R sample *pairs*, which is
    the one thing a Lissajous display needs. Adding one means a lock-free ring
    of pairs in `core/dsp`, a sixth member on `ModuleContext`, and a new
    component in `core/ui`. That is a core change touching every module's
    wiring, and it should be argued for on its own rather than riding in with
    module six.

    Two things to weigh when it is picked up. A **correlation meter** would fit
    the existing contract exactly -- one float in [-1, +1], one more callback,
    no new infrastructure -- and it is the same reading a goniometer is used
    for here, since the S1's "within 45 degrees of vertical" rule is a visual
    reading of correlation. And whatever is built, `tools/snapshot` feeds a
    panel no audio, so the meter renders empty in the review loop the repo
    relies on -- the same gap the Palette Book records against BMO Opto's meter
    modes.
*/
class DimPanel final : public ui::ModulePanel
{
public:
    explicit DimPanel (ui::ModuleContext);

    void resized() override;

private:
    ui::PlainKnob width, shuffle, shuffleFreq, cents, diffuse,
                  rotation, asymmetry;

    ui::SwitchButton detuneOn;
};

} // namespace bmo::dim

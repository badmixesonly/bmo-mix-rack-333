#include "VcompPanel.h"
#include "modules/vcomp/params.h"

namespace bmo::vcomp
{

namespace
{
    // The knob draws at kKnobSide; the control is laid out kKnobWidth wide so
    // the caption underneath has room -- see PlainKnob::setKnobSide for the
    // BMO Opto case that forced the distinction.
    constexpr int kKnobSide   = 92;
    constexpr int kKnobWidth  = 136;
    constexpr int kKnobHeight = 150;

    constexpr int kSwitchWidth  = ui::Tokens::switchWidth;
    constexpr int kSwitchHeight = ui::Tokens::switchHeight;
    constexpr int kSwitchGap    = ui::Tokens::switchGap;

    constexpr int kBarRow   = 22;   ///< one meter row: the bar, with air around it
    constexpr int kBarGap   = 2;
    constexpr int kMeterBlock = kBarRow * 3 + kBarGap * 2;

    // The five detector knobs are trim knobs -- ui::ModulePanel::styleTrimKnob
    // sizes and captions them -- in two rows of three and two.
    constexpr int kDetectorRow = ui::ModulePanel::kTrimKnobRow;
    constexpr int kDetectorBlock = kDetectorRow * 2 + kSwitchGap;

    /** dBFS from a linear peak, floored at the meters' own bottom so a silent
        input parks the bar at the left rather than at minus infinity. */
    float meterDb (float linear)
    {
        return juce::Decibels::gainToDecibels (linear, kGateOffDb);
    }

    /** The GR bar's full-scale reading. 24 dB, which is BMO Opto's
        DynamicsMeter scale -- the two dynamics modules in the suite should not
        disagree about what "a lot of reduction" looks like, and at the top of
        AMOUNT this module reaches about 26. */
    constexpr float kMaxReductionDb = 24.0f;
}

VcompPanel::VcompPanel (ui::ModuleContext ctx)
    : ModulePanel (std::move (ctx)),
      // AMOUNT and MAKEUP are character knobs in the module's accent: they are
      // what this module *is*. The five detector knobs below are utility --
      // the suite's azure, dotted track -- because five more accent knobs
      // would compete with AMOUNT for the eye, and these are controls you set
      // once rather than ride.
      amount (context.params.param (Index::amount), "AMOUNT",
              ui::Knob::Style::character, 0.62f, context.def.accent),
      output (context.params.param (Index::output), "MAKEUP",
              ui::Knob::Style::character, 0.62f, context.def.accent),
      inBar  ("IN",  LevelBar::Grow::rightward, kGateOffDb, 0.0f,
              [this] { return context.inputPeak ? meterDb (context.inputPeak()) : kGateOffDb; }),
      grBar  ("GR",  LevelBar::Grow::leftward, 0.0f, kMaxReductionDb,
              [this] { return context.gainReductionDb ? context.gainReductionDb() : 0.0f; }),
      outBar ("OUT", LevelBar::Grow::rightward, kGateOffDb, 0.0f,
              [this] { return context.peak ? meterDb (context.peak()) : kGateOffDb; }),
      // Neither switch is a bypass, a mono or a polarity, so both take
      // switchAlt -- see the table in modules/AGENTS.md. Not a free choice.
      complexSwitch (context.params.param (Index::complex), "COMPLEX", ui::tokens().switchAlt),
      arcSwitch     (context.params.param (Index::arc),     "ARC",     ui::tokens().switchAlt),
      attackKnob    (context.params.param (Index::attack),    "ATTACK"),
      releaseKnob   (context.params.param (Index::release),   "RELEASE"),
      sidechainKnob (context.params.param (Index::sidechain), "SC HPF"),
      lowThruKnob   (context.params.param (Index::lowThru),   "LOW THRU"),
      highThruKnob  (context.params.param (Index::highThru),  "HIGH THRU")
{
    for (auto* k : { &amount, &output })
        k->setKnobSide (kKnobSide);

    for (auto* k : { &attackKnob, &releaseKnob, &sidechainKnob, &lowThruKnob, &highThruKnob })
        styleTrimKnob (*k);

    // Reduction is not a fault, so it is not painted in the fault colours. The
    // low/high/clip zones say "you are running out of headroom", which is true
    // of a level and false of a compressor working hard -- 24 dB of reduction
    // in the clip red would be the meter telling the user off for using the
    // module. meterGr is the token that exists for exactly this.
    grBar.setFlatColour (ui::tokens().meterGr);

    // The gate, on the meter that shows the level it acts on. The handle takes
    // the module's accent because it is a control, and every other control on
    // this panel that is not a switch is in the accent too.
    inBar.attachThreshold (context.params.param (Index::gate), context.def.accent);

    for (auto* c : std::initializer_list<juce::Component*> {
             &amount, &inBar, &grBar, &outBar, &output, &complexSwitch, &arcSwitch,
             &attackKnob, &releaseKnob, &sidechainKnob, &lowThruKnob, &highThruKnob })
        addAndMakeVisible (c);

    lastComplex = context.params.param (Index::complex).getValue() > 0.5f;
    applyComplex (lastComplex);

    startTimerHz (30);
}

VcompPanel::~VcompPanel() { stopTimer(); }

void VcompPanel::applyComplex (bool on)
{
    for (auto* k : std::initializer_list<juce::Component*> {
             &attackKnob, &releaseKnob, &sidechainKnob, &lowThruKnob, &highThruKnob })
        k->setVisible (on);

    // Standard mode runs ARC whatever the parameter says -- DspCore substitutes
    // kStandardArc -- so the switch is held on and made unclickable rather than
    // disabled or hidden. Locked, not disabled, for the reason BMO Opto's COLOR
    // is: the disabled alpha pulls a switch's fill and its ink toward the plate
    // at the same rate, which says it at about 1.3:1.
    arcSwitch.setLockedOn (! on);

    // Coming out of the lock, hand the switch back what the user actually set.
    // The lock never wrote to the parameter, so the value is still there, but
    // nothing else will push it into the button: the attachment only speaks
    // when the parameter changes, and it has not.
    if (on)
        arcSwitch.setToggleStateSilently (context.params.param (Index::arc).getValue() > 0.5f);
}

void VcompPanel::timerCallback()
{
    inBar.refresh();
    grBar.refresh();
    outBar.refresh();

    // COMPLEX is polled rather than listened to, same as the meters and BMO
    // Opto's mode poll -- there is no cross-thread marshaling to get right for
    // a once-in-a-while UI state change, and polling means host automation of
    // COMPLEX moves the panel exactly as a click does.
    const auto on = context.params.param (Index::complex).getValue() > 0.5f;

    if (on != lastComplex)
    {
        lastComplex = on;
        applyComplex (on);
    }
    else if (! on)
    {
        // Re-asserted while the lock holds: host automation of ARC still
        // reaches the attachment in standard mode and would otherwise put the
        // stored value back on screen under a switch the DSP is holding on.
        arcSwitch.setLockedOn (true);
    }
}

void VcompPanel::resized()
{
    auto area = getLocalBounds().reduced (kPad, 4);

    const auto content = kKnobHeight + kMeterBlock + kKnobHeight
                             + kSwitchHeight + kDetectorBlock;

    // Six divisions -- a margin above the first block and below the last as
    // well as between them -- and the gap is worked out from the *complex-on*
    // content whether or not the detector rows are visible. That is what keeps
    // AMOUNT, the meters, MAKEUP and the switch row at the same pixel in both
    // states; in standard mode the reserved rows and the margin under them are
    // simply empty plate. See the class comment for why that trade was taken.
    const auto gap = juce::jmax (kSwitchGap, (area.getHeight() - content) / 6);

    area.removeFromTop (gap);

    amount.setBounds (area.removeFromTop (kKnobHeight)
                          .withSizeKeepingCentre (kKnobWidth, kKnobHeight));
    area.removeFromTop (gap);

    // IN, GR, OUT top to bottom: what arrived, what was taken off it, what
    // left. Reading order is signal order.
    {
        auto block = area.removeFromTop (kMeterBlock);

        inBar.setBounds (block.removeFromTop (kBarRow));
        block.removeFromTop (kBarGap);
        grBar.setBounds (block.removeFromTop (kBarRow));
        block.removeFromTop (kBarGap);
        outBar.setBounds (block.removeFromTop (kBarRow));
    }
    area.removeFromTop (gap);

    output.setBounds (area.removeFromTop (kKnobHeight)
                          .withSizeKeepingCentre (kKnobWidth, kKnobHeight));
    area.removeFromTop (gap);

    // COMPLEX and ARC abreast: the switch that opens the drawer, and the one
    // thing inside it that is true in both states.
    {
        auto row = area.removeFromTop (kSwitchHeight)
                       .withSizeKeepingCentre (kSwitchWidth * 2 + kSwitchGap, kSwitchHeight);

        complexSwitch.setBounds (row.removeFromLeft (kSwitchWidth));
        row.removeFromLeft (kSwitchGap);
        arcSwitch.setBounds (row.removeFromLeft (kSwitchWidth));
    }
    area.removeFromTop (gap);

    // The reserved rows: the three detector-timing controls above the two that
    // decide which band is being compressed at all. Grouped by what they do
    // rather than packed to fill, so the row break means something.
    {
        auto row = area.removeFromTop (kDetectorRow);
        const auto each = row.getWidth() / 3;

        attackKnob   .setBounds (row.removeFromLeft (each));
        releaseKnob  .setBounds (row.removeFromLeft (each));
        sidechainKnob.setBounds (row.removeFromLeft (each));
    }

    area.removeFromTop (kSwitchGap);

    {
        auto row = area.removeFromTop (kDetectorRow);

        // **Two knobs across the full width, not two thirds of a three-column
        // grid.** These carry the longest captions in the module, and in an
        // 80 px column "LOW THRU" and "HIGH THRU" rendered as "LOW THR" and
        // "HIGH TH": Graphics::drawText curtails what will not fit rather than
        // spilling it, so a caption wider than its own control loses its tail
        // silently. They need 90.7 and 93.3 px at the trim caption size.
        //
        // ui_layout passed with them clipped, and the reason is worth keeping:
        // not because PlainKnob::captionOverflow was wrong -- it reports 10.7
        // and 13.3 px, correctly -- but because this module had not been added
        // to that test's product list, so its panel was never looked at. That
        // list is one of the shared files modules/AGENTS.md tells a new module
        // to edit, and missing it fails exactly this way: silently, with a
        // green suite.
        const auto each = row.getWidth() / 2;

        lowThruKnob .setBounds (row.removeFromLeft (each));
        highThruKnob.setBounds (row.removeFromLeft (each));
    }
}

} // namespace bmo::vcomp

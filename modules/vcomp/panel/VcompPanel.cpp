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

    constexpr int kBarRow   = 22 + LevelBar::kScaleRow;   ///< the bar, its air, and its printed scale
    constexpr int kBarGap   = 2;

    // The IN bar is kTagRow taller than the other two, because it is the one
    // carrying the gate and its flag needs somewhere to stand. It absorbs the
    // difference inside itself -- see LevelBar::wellBounds -- so all three
    // wells stay evenly spaced and the block still reads as one instrument.
    constexpr int kMeterBlock = kBarRow * 3 + kBarGap * 2 + LevelBar::kTagRow;

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
      // COMPLEX is neither a bypass, a mono nor a polarity, so it takes
      // switchAlt -- the table in modules/AGENTS.md, not a free choice.
      complexSwitch (context.params.param (Index::complex), "COMPLEX", ui::tokens().switchAlt),
      // SAUCE takes the engaged red rather than switchAlt, which is an
      // exception to the switch table in modules/AGENTS.md and the second one
      // in the suite. BMO Opto is the first, and for the same reason: a panel
      // with no colour of its own can let the one colour on it mean "on".
      // Frosty, 2026-09-14. The red is tokens().meterClip, the same one Opto
      // lights TELE, LINK and COLOR with -- not a new hex, and themable.
      arcSwitch     (context.params.param (Index::arc),     "SAUCE",   ui::tokens().meterClip),
      attackKnob    (context.params.param (Index::attack),    "ATTACK"),
      releaseKnob   (context.params.param (Index::release),   "RELEASE"),
      sidechainKnob (context.params.param (Index::sidechain), "SC HPF"),
      lowThruKnob   (context.params.param (Index::lowThru),   "LOW THRU"),
      highThruKnob  (context.params.param (Index::highThru),  "HIGH THRU")
{
    // The printed scales -- Frosty, 2026-09-14. Not evenly spaced, and that is
    // the point: the figures crowd toward 0 because that is the end a reader
    // works at. -60 is a floor you need named once; everything between -24 and
    // 0 is where a vocal actually sits and where the gate gets set.
    //
    // GR's positions are amounts of reduction, 0..24 and positive, because
    // that is what the DSP reports. Its *text* is negative, because what the
    // meter means is gain. ScaleMark keeps the two apart for exactly this.
    // **-18 sits at the halfway point and the scale opens out toward 0** --
    // Frosty, 2026-09-14. The bar is no longer linear in dB: the top 18 take
    // half its length and the bottom 42 take the other half, because the top
    // is where a vocal lives and where the gate gets set, and -60 is a floor
    // you need named once.
    //
    // Hand-placed, and it has to be. Sweeping an exponent cannot hold -18 at
    // 0.5 *and* keep opening out above it; BMO Opto's GR scale hit the same
    // wall and stopped pretending to be a power law. Per-dB density across the
    // marks below runs 0.0106, 0.020, 0.025, 0.028, 0.030 -- monotonic toward
    // 0, which is the property to preserve if one is ever moved.
    const std::vector<LevelBar::ScaleMark> levelScale {
        { -60.0f, 0.00f, "-60" }, { -24.0f, 0.38f, "-24" }, { -18.0f, 0.50f, "-18" },
        { -12.0f, 0.66f, "-12" }, {  -6.0f, 0.83f,  "-6" }, {   0.0f, 1.00f,   "0" },
    };

    inBar .setScale (levelScale);
    outBar.setScale (levelScale);

    // GR gets the same treatment about its own zero, which is the *right* end:
    // its positions are amounts of reduction and its fill grows leftward from
    // none. So the first 3 dB of reduction take a fifth of the bar and the
    // last 12 take two fifths -- the difference between 1 and 3 dB is worth
    // seeing and the difference between 20 and 24 is not.
    //
    // The text is negative where the position is positive. ScaleMark keeps the
    // two apart for exactly this: what the DSP reports is reduction, what the
    // meter means is gain.
    grBar .setScale ({ {  0.0f, 0.00f,   "0" }, {  3.0f, 0.20f,  "-3" },
                       {  6.0f, 0.35f,  "-6" }, { 12.0f, 0.60f, "-12" },
                       { 24.0f, 1.00f, "-24" } });

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

    // The gate, on the meter that shows the level it acts on. Red rather than
    // the module accent -- Frosty, 2026-09-14 -- which is the same call as
    // SAUCE above and leaves the panel greyscale but for the two things that
    // act: the switch that is on, and the threshold that is cutting.
    inBar.attachThreshold (context.params.param (Index::gate), ui::tokens().meterClip);

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

    const auto content = kSwitchHeight + kKnobHeight + kMeterBlock + kKnobHeight
                             + kSwitchHeight + kDetectorBlock;

    // Seven divisions -- a margin above the first block and below the last as
    // well as between them -- and the gap is worked out from the *complex-on*
    // content whether or not the detector rows are visible. That is what keeps
    // AMOUNT, the meters, MAKEUP and the switch row at the same pixel in both
    // states; in standard mode the reserved rows and the margin under them are
    // simply empty plate. See the class comment for why that trade was taken.
    const auto gap = juce::jmax (kSwitchGap, (area.getHeight() - content) / 7);

    area.removeFromTop (gap);

    // SAUCE above AMOUNT, centred on it. It is the programme-dependent release
    // and standard mode runs it whatever the parameter says, so it belongs to
    // AMOUNT rather than to the drawer COMPLEX opens -- which is where it sat
    // until 2026-09-14, abreast of COMPLEX, reading as one of the advanced
    // controls it is not.
    arcSwitch.setBounds (area.removeFromTop (kSwitchHeight)
                             .withSizeKeepingCentre (kSwitchWidth, kSwitchHeight));
    area.removeFromTop (gap);

    amount.setBounds (area.removeFromTop (kKnobHeight)
                          .withSizeKeepingCentre (kKnobWidth, kKnobHeight));
    area.removeFromTop (gap);

    // IN, GR, OUT top to bottom: what arrived, what was taken off it, what
    // left. Reading order is signal order.
    {
        auto block = area.removeFromTop (kMeterBlock);

        inBar.setBounds (block.removeFromTop (kBarRow + LevelBar::kTagRow));
        block.removeFromTop (kBarGap);
        grBar.setBounds (block.removeFromTop (kBarRow));
        block.removeFromTop (kBarGap);
        outBar.setBounds (block.removeFromTop (kBarRow));
    }
    area.removeFromTop (gap);

    output.setBounds (area.removeFromTop (kKnobHeight)
                          .withSizeKeepingCentre (kKnobWidth, kKnobHeight));
    area.removeFromTop (gap);

    // COMPLEX alone now, centred: the switch that opens the drawer, directly
    // over what it opens. SAUCE used to sit beside it and is at the head.
    complexSwitch.setBounds (area.removeFromTop (kSwitchHeight)
                                 .withSizeKeepingCentre (kSwitchWidth, kSwitchHeight));
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

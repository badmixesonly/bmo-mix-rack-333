#include "OptoPanel.h"
#include "modules/opto/params.h"

namespace bmo::opto
{

namespace
{
    // The knob draws at kKnobSide; the control is laid out kKnobWidth wide so
    // that the caption underneath has room. "MAKEUP" set at 15 pt is wider
    // than the 92 px the knob itself wants, and clipped to "MAKEU" when the
    // two were the same number.
    constexpr int kKnobSide    = 92;
    constexpr int kKnobWidth   = 136;
    constexpr int kKnobHeight  = 150;
    constexpr int kMeterWidth  = 190;
    constexpr int kMeterButtonRow = 22;
    constexpr int kMeterButtonGap = 4;   ///< meter face to its IN/GR/OUT row

    // A needle meter is a landscape window: at this width the arc stands about
    // 91 px tall, and the 14 px mode caption sits under it. Handing DynamicsMeter
    // the whole 168 px of the middle third, as 0.2.1 did, only bought empty
    // face -- the meter centres its arc in whatever box it is given, so the
    // height it does not need is better spent by the panel.
    constexpr int kMeterHeight = 116;

    // Lifted from BMO Util so the switches are literally the same control at
    // the same size across the suite -- see modules/util/panel/UtilPanel.cpp.
    constexpr int kSwitchWidth  = 70;
    constexpr int kSwitchHeight = 26;
    constexpr int kSwitchGap    = 8;

    // 0.2.1 also carried #9c71c3 here, for the captions and the switches,
    // because the accent is a pale lavender chosen for knob caps and there
    // was no token for "the accent, stepped until it is legible". There is
    // now -- ui::accentTextOn -- so nothing on this panel names a colour any
    // more: every one of them comes from context.def.accent, which means a
    // theme change reaches this module the same way it reaches the others.

    // The meter's hot zone -- 0 VU and above -- is passed at the constructor
    // as faceOf(accent), the accent halfway to white: a classic VU prints it
    // red, and this one prints it in the module's own colour instead. It was
    // #97ddff in 0.2.0, which measured 1.02:1 on the old light face.

    // The face the scale is printed on. Dark, so the white needle and white
    // numbers have something to read against: 11.4:1 for the needle, 8.2:1
    // for the hot zone. Frosty's cream-face/black-needle mockup inverted --
    // same idea, which is that a needle meter needs one very light element
    // and one very dark one, and 0.2.0 had neither.
    const juce::Colour kMeterFaceColour = juce::Colour (0xff3a3a3a);
}

OptoPanel::OptoPanel (ui::ModuleContext ctx)
    : ModulePanel (std::move (ctx)),
      crush (context.params.param (Index::crush), "COMP",
             ui::Knob::Style::character, 0.62f, context.def.accent),
      level (context.params.param (Index::level), "MAKEUP",
             ui::Knob::Style::character, 0.62f, context.def.accent),
      meter (context.inputRms, context.rms, context.gainReductionDb,
             ui::DynamicsMeter::Mode::output, context.def.accent,
             ui::faceOf (context.def.accent), kMeterFaceColour),
      teleButton ("TELE"), eldButton ("ELD"),
      meterInButton ("IN"), meterOutButton ("OUT"), meterGrButton ("GR"),
      link  (context.params.param (Index::link),  "LINK",  context.def.accent),
      color (context.params.param (Index::color), "COLOR", context.def.accent)
{
    // The switches take the raw accent as their fill, the same as every other
    // module's. What made that unreadable before was the ink: white on this
    // lavender is 1.99:1. BmoLookAndFeel now derives the label from whatever
    // fill it is drawing, so the accent can be used here directly.
    for (auto* b : { &teleButton, &eldButton,
                     &meterInButton, &meterOutButton, &meterGrButton })
    {
        b->setClickingTogglesState (false);
        b->setColour (juce::ToggleButton::tickColourId, context.def.accent);
        addAndMakeVisible (b);
    }

    // A pair in radio behaviour, like the meter's IN/GR/OUT row: clicking sets
    // the mode rather than toggling a button, so host automation and a click
    // land in the same place. timerCallback() is what reads the parameter back
    // into the two states.
    teleButton.onClick = [this] { setChoice (context.params.param (Index::mode), 0.0f); };
    eldButton .onClick = [this] { setChoice (context.params.param (Index::mode), 1.0f); };

    meterInButton.onClick = [this]
    {
        meter.setMode (ui::DynamicsMeter::Mode::input);
        meterInButton.setToggleState (true, juce::dontSendNotification);
        meterOutButton.setToggleState (false, juce::dontSendNotification);
        meterGrButton.setToggleState (false, juce::dontSendNotification);
    };
    meterOutButton.onClick = [this]
    {
        meter.setMode (ui::DynamicsMeter::Mode::output);
        meterInButton.setToggleState (false, juce::dontSendNotification);
        meterOutButton.setToggleState (true, juce::dontSendNotification);
        meterGrButton.setToggleState (false, juce::dontSendNotification);
    };
    meterGrButton.onClick = [this]
    {
        meter.setMode (ui::DynamicsMeter::Mode::reduction);
        meterInButton.setToggleState (false, juce::dontSendNotification);
        meterOutButton.setToggleState (false, juce::dontSendNotification);
        meterGrButton.setToggleState (true, juce::dontSendNotification);
    };
    meterOutButton.setToggleState (true, juce::dontSendNotification);

    for (auto* k : { &crush, &level })
        k->setKnobSide (kKnobSide);

    for (auto* c : std::initializer_list<juce::Component*> { &crush, &meter, &level, &link, &color })
        addAndMakeVisible (c);

    lastModeWasStressed = context.params.param (Index::mode).getValue() > 0.5f;
    teleButton.setToggleState (! lastModeWasStressed, juce::dontSendNotification);
    eldButton .setToggleState (  lastModeWasStressed, juce::dontSendNotification);
    color.setSwitchEnabled (lastModeWasStressed);
    applyModeColours (lastModeWasStressed);

    startTimerHz (15);
}

OptoPanel::~OptoPanel() { stopTimer(); }

//==============================================================================
juce::Colour OptoPanel::accentFor (bool stressed) const
{
    // Greyscale in Tele. `neutral` rather than a hex of its own: see the token
    // for why it is not simply the accent's lightness in grey.
    return stressed ? context.def.accent : ui::tokens().neutral;
}

juce::Colour OptoPanel::hotColourFor (bool stressed) const
{
    // Stressed prints 0 VU and above in the pale lavender of the knob caps --
    // a classic VU's red zone, in the module's own colour. Tele prints it in
    // an actual red, stepped off the suite's own meterClip until it clears
    // 4.5:1 on this dark face rather than being typed in: meterClip as it
    // stands is 3.41:1 there, which is the same mistake the 0.2.0 hot zone
    // made at 1.02:1, only smaller.
    return stressed ? ui::faceOf (context.def.accent)
                    : ui::accentTextOn (ui::tokens().meterClip, kMeterFaceColour);
}

void OptoPanel::applyModeColours (bool stressed)
{
    const auto accent = accentFor (stressed);

    // Every derived colour on the panel -- knob caps, captions, dotted tracks,
    // the plus and minus, switch fills and their ink -- comes off this one
    // value, so a mode change is four calls rather than a second palette.
    for (auto* k : { &crush, &level })
        k->setAccent (accent);

    for (auto* s : { &link, &color })
        s->setTint (accent);

    for (auto* b : { &teleButton, &eldButton,
                     &meterInButton, &meterOutButton, &meterGrButton })
    {
        b->setColour (juce::ToggleButton::tickColourId, accent);
        b->repaint();
    }

    meter.setColours (accent, hotColourFor (stressed));
}

void OptoPanel::setChoice (juce::RangedAudioParameter& param, float normalisedValue)
{
    param.beginChangeGesture();
    param.setValueNotifyingHost (normalisedValue);
    param.endChangeGesture();
}

void OptoPanel::timerCallback()
{
    // Color has no off state in Tele mode (DspCore locks it on regardless of
    // the parameter -- see DspCore::process()), so the switch is disabled
    // there. Disabled and not hidden: hiding it moved LINK up the panel every
    // time the mode changed, and a control that jumps around is worse than a
    // dimmed one that stays put.
    //
    // Polled rather than a parameter listener, same as OutputMeter/
    // DynamicsMeter's own timers -- there's no cross-thread marshaling to get
    // right for a once-in-a-while UI state change like this one. The mode
    // button's own label and glow are polled the same way, so host automation
    // of Mode (not just a click) still updates it.
    const auto stressed = context.params.param (Index::mode).getValue() > 0.5f;

    teleButton.setToggleState (! stressed, juce::dontSendNotification);
    eldButton .setToggleState (  stressed, juce::dontSendNotification);

    if (stressed != lastModeWasStressed)
    {
        lastModeWasStressed = stressed;
        color.setSwitchEnabled (stressed);
        applyModeColours (stressed);
    }
}

void OptoPanel::resized()
{
    auto area = getLocalBounds().reduced (kPad, 4);

    const auto centredSwitch = [] (juce::Rectangle<int> row)
    {
        return row.withSizeKeepingCentre (kSwitchWidth, kSwitchHeight);
    };

    // One rhythm down the panel, rather than three equal thirds with each
    // block centred inside its own. The blocks are 150, 142 and 150 tall in
    // rows of 190, so the thirds spent 132 px of slack as uneven centring: a
    // 36 px band under TELE, 46 px above the meter, 28 px above LINK -- and
    // nothing at all under COLOR, which sat on the bottom edge of the panel.
    //
    // Every block is placed from the top and the remainder falls below COLOR
    // as the bottom margin, so the spacing stays even if a block's height
    // changes later.
    const auto meterBlock  = kMeterHeight + kMeterButtonGap + kMeterButtonRow;
    const auto stackBlock  = kSwitchHeight * 2 + kSwitchGap;   // TELE/ELD, and LINK/COLOR
    const auto content     = stackBlock + kKnobHeight + meterBlock + kKnobHeight + stackBlock;

    // Six divisions, not four: a margin above the first block and below the
    // last one as well as between them. A lone mode button used to sit 7 px
    // off the top of the panel with a 58 px band under it.
    const auto gap = juce::jmax (kSwitchGap, (area.getHeight() - content) / 6);

    area.removeFromTop (gap);

    // Mode above everything: it decides what COMP and MAKEUP mean. Stacked
    // rather than abreast, so naming both modes costs height instead of the
    // width this module has none of -- and it mirrors LINK/COLOR at the foot.
    {
        auto head = area.removeFromTop (stackBlock);
        teleButton.setBounds (centredSwitch (head.removeFromTop (kSwitchHeight)));
        head.removeFromTop (kSwitchGap);
        eldButton.setBounds  (centredSwitch (head.removeFromTop (kSwitchHeight)));
    }
    area.removeFromTop (gap);

    crush.setBounds (area.removeFromTop (kKnobHeight)
                         .withSizeKeepingCentre (kKnobWidth, kKnobHeight));
    area.removeFromTop (gap);

    // The VU meter over its IN/GR/OUT row -- GR in the middle because it is
    // the reading this module is actually for, and IN/OUT then read
    // left-to-right as signal flow either side of it. Frosty asked for this
    // order specifically, 2026-09-06.
    {
        auto block = area.removeFromTop (meterBlock);
        const auto meterWidth = juce::jmin (block.getWidth(), kMeterWidth);

        meter.setBounds (block.removeFromTop (kMeterHeight)
                              .withSizeKeepingCentre (meterWidth, kMeterHeight));
        block.removeFromTop (kMeterButtonGap);

        auto buttons = block.withSizeKeepingCentre (meterWidth, block.getHeight());
        const auto buttonWidth = buttons.getWidth() / 3;

        meterInButton.setBounds  (buttons.removeFromLeft (buttonWidth).reduced (3, 1));
        meterGrButton.setBounds  (buttons.removeFromLeft (buttonWidth).reduced (3, 1));
        meterOutButton.setBounds (buttons.reduced (3, 1));
    }
    area.removeFromTop (gap);

    level.setBounds (area.removeFromTop (kKnobHeight)
                         .withSizeKeepingCentre (kKnobWidth, kKnobHeight));
    area.removeFromTop (gap);

    // LINK and COLOR at the foot, under the MAKEUP caption. Placed from the
    // top like everything else, so what is left over stays underneath them.
    auto footer = area.removeFromTop (stackBlock);
    link.setBounds  (centredSwitch (footer.removeFromTop (kSwitchHeight)));
    footer.removeFromTop (kSwitchGap);
    color.setBounds (centredSwitch (footer.removeFromTop (kSwitchHeight)));
}

} // namespace bmo::opto

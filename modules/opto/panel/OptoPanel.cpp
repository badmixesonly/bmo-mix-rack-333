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

    // Lifted from BMO Util so the switches are literally the same control at
    // the same size across the suite -- see modules/util/panel/UtilPanel.cpp.
    constexpr int kSwitchWidth  = 70;
    constexpr int kSwitchHeight = 26;
    constexpr int kSwitchGap    = 8;

    // The module's own accent (see modules/opto/Module.cpp) is used for
    // knob faces and switch glows; labels want a darker, higher-contrast
    // step of the same hue rather than the shared suite-wide track colour
    // PlainKnob otherwise defaults to -- per Frosty's 2026-09-06 note.
    const juce::Colour kLabelColour { 0xff9c71c3 };

    // The meter's 0 VU-and-above zone: a classic VU meter prints this in
    // red, but the meter reads as part of the module's own palette instead,
    // so the zone is the lavender of the knob caps -- faceOf(accent), the
    // accent halfway to white. It was #97ddff in 0.2.0, which measured
    // 1.02:1 on the old light face and could not be seen at all.
    const juce::Colour kMeterHotColour = ui::faceOf (juce::Colour (0xffd4a4ff));

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
             ui::Knob::Style::character, 0.62f, context.def.accent, kLabelColour),
      level (context.params.param (Index::level), "MAKEUP",
             ui::Knob::Style::character, 0.62f, context.def.accent, kLabelColour),
      meter (context.inputRms, context.rms, context.gainReductionDb,
             ui::DynamicsMeter::Mode::output, context.def.accent, kMeterHotColour,
             kMeterFaceColour),
      modeButton ("TELE"),
      meterInButton ("IN"), meterOutButton ("OUT"), meterGrButton ("GR"),
      link  (context.params.param (Index::link),  "LINK",  kLabelColour),
      color (context.params.param (Index::color), "COLOR", kLabelColour)
{
    // Every switch lights in kLabelColour rather than the raw accent: the
    // accent is a pale lavender chosen for knob caps, and white text on it
    // is unreadable. This is the same darker step COMP and MAKEUP are set
    // in, so an engaged switch matches the captions above it.
    for (auto* b : { &modeButton, &meterInButton, &meterOutButton, &meterGrButton })
    {
        b->setClickingTogglesState (false);
        b->setColour (juce::ToggleButton::tickColourId, kLabelColour);
        addAndMakeVisible (b);
    }

    // One button, carrying whichever mode it is in. Toggling is done through
    // the parameter rather than the button's own state so host automation and
    // a click land in the same place; timerCallback() is what reads it back.
    modeButton.onClick = [this]
    {
        const auto stressed = context.params.param (Index::mode).getValue() > 0.5f;
        setChoice (context.params.param (Index::mode), stressed ? 0.0f : 1.0f);
    };

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
    modeButton.setButtonText (lastModeWasStressed ? "ELD" : "TELE");
    modeButton.setToggleState (lastModeWasStressed, juce::dontSendNotification);
    color.setSwitchEnabled (lastModeWasStressed);

    startTimerHz (15);
}

OptoPanel::~OptoPanel() { stopTimer(); }

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

    modeButton.setToggleState (stressed, juce::dontSendNotification);

    if (stressed != lastModeWasStressed)
    {
        lastModeWasStressed = stressed;
        modeButton.setButtonText (stressed ? "ELD" : "TELE");
        color.setSwitchEnabled (stressed);
    }
}

void OptoPanel::resized()
{
    auto area = getLocalBounds().reduced (kPad, 4);

    const auto centredSwitch = [] (juce::Rectangle<int> row)
    {
        return row.withSizeKeepingCentre (kSwitchWidth, kSwitchHeight);
    };

    // Mode above everything: it decides what COMP and MAKEUP mean.
    modeButton.setBounds (centredSwitch (area.removeFromTop (kSwitchHeight)));
    area.removeFromTop (kSwitchGap * 2);

    // LINK and COLOR stacked at the foot, under the MAKEUP caption. Taken off
    // the bottom before the three rows are measured so the rows stay even.
    auto footer = area.removeFromBottom (kSwitchHeight * 2 + kSwitchGap);
    link.setBounds  (centredSwitch (footer.removeFromTop (kSwitchHeight)));
    footer.removeFromTop (kSwitchGap);
    color.setBounds (centredSwitch (footer.removeFromTop (kSwitchHeight)));
    area.removeFromBottom (kSwitchGap);

    const auto rowHeight = area.getHeight() / 3;

    auto topRow    = area.removeFromTop (rowHeight);
    auto middleRow = area.removeFromTop (rowHeight);
    auto bottomRow = area;

    // Top third: COMP, large and alone.
    crush.setBounds (topRow.withSizeKeepingCentre (kKnobWidth, kKnobHeight));

    // Middle third: the VU meter over its IN/GR/OUT row -- GR in the
    // middle because it is the reading this module is actually for, and
    // IN/OUT then read left-to-right as signal flow either side of it.
    // Frosty asked for this order specifically, 2026-09-06.
    auto meterButtonRow = middleRow.removeFromBottom (kMeterButtonRow);
    meter.setBounds (middleRow.withSizeKeepingCentre (juce::jmin (middleRow.getWidth(), kMeterWidth),
                                                       middleRow.getHeight()));

    const auto meterButtons = meterButtonRow.withSizeKeepingCentre (
        juce::jmin (meterButtonRow.getWidth(), kMeterWidth), meterButtonRow.getHeight());
    const auto meterButtonWidth = meterButtons.getWidth() / 3;
    auto meterButtonArea = meterButtons;
    meterInButton.setBounds  (meterButtonArea.removeFromLeft (meterButtonWidth).reduced (3, 1));
    meterGrButton.setBounds  (meterButtonArea.removeFromLeft (meterButtonWidth).reduced (3, 1));
    meterOutButton.setBounds (meterButtonArea.reduced (3, 1));

    // Bottom third: MAKEUP. Its LINK/COLOR stack was placed above, before the
    // rows were measured.
    level.setBounds (bottomRow.withSizeKeepingCentre (kKnobWidth, kKnobHeight));
}

} // namespace bmo::opto

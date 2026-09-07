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
    constexpr int kSwitchRow   = 26;
    constexpr int kMeterButtonRow = 22;

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

    // The face the scale is printed on. Dark, matching Util's disengaged
    // switches, so the white needle and white numbers have something to
    // read against -- Frosty's 2026-09-06 direction.
    const juce::Colour kMeterFaceColour = juce::Colour (0xffa6a6a6);
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
      teleButton ("TELE"), eldButton ("ELD"),
      meterInButton ("IN"), meterOutButton ("OUT"), meterGrButton ("GR"),
      link  (context.params.param (Index::link),  "LINK",  context.def.accent),
      color (context.params.param (Index::color), "COLOR", context.def.accent)
{
    for (auto* b : { &teleButton, &eldButton, &meterInButton, &meterOutButton, &meterGrButton })
    {
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonOnColourId, context.def.accent);
        addAndMakeVisible (b);
    }

    teleButton.onClick = [this] { setChoice (context.params.param (Index::mode), 0.0f); };
    eldButton.onClick  = [this] { setChoice (context.params.param (Index::mode), 1.0f); };

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
    eldButton.setToggleState  (lastModeWasStressed,   juce::dontSendNotification);
    color.setVisible (lastModeWasStressed);
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
    // Color has no off state in Tele mode (DspCore locks it on regardless
    // of the parameter -- see DspCore::process()), so the switch itself is
    // hidden and disabled there rather than left on-screen doing nothing.
    // Polled rather than a parameter listener, same as OutputMeter/
    // DynamicsMeter's own timers -- there's no cross-thread marshaling to
    // get right for a once-in-a-while UI state change like this one. The
    // TELE/ELD highlight is polled the same way so host automation of Mode
    // (not just a click on these buttons) still updates which one glows.
    const auto stressed = context.params.param (Index::mode).getValue() > 0.5f;

    teleButton.setToggleState (! stressed, juce::dontSendNotification);
    eldButton.setToggleState  (stressed,   juce::dontSendNotification);

    if (stressed != lastModeWasStressed)
    {
        lastModeWasStressed = stressed;
        color.setVisible (stressed);
        color.setSwitchEnabled (stressed);
    }
}

void OptoPanel::resized()
{
    auto area = getLocalBounds().reduced (kPad, 4);
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

    // Bottom third: MAKEUP, then the Mode/Link/Color row underneath it.
    auto switchRow = bottomRow.removeFromBottom (kSwitchRow);
    level.setBounds (bottomRow.withSizeKeepingCentre (kKnobWidth, kKnobHeight));

    const auto switchWidth = switchRow.getWidth() / 4; // TELE, ELD, LINK, COLOR
    teleButton.setBounds (switchRow.removeFromLeft (switchWidth).reduced (3, 2));
    eldButton.setBounds  (switchRow.removeFromLeft (switchWidth).reduced (3, 2));
    link.setBounds       (switchRow.removeFromLeft (switchWidth).reduced (3, 2));
    color.setBounds      (switchRow.reduced (3, 2));
}

} // namespace bmo::opto

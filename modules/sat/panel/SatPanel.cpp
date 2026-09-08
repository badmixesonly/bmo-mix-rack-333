#include "SatPanel.h"
#include "modules/sat/params.h"

namespace bmo::sat
{

namespace
{
    constexpr int kGainRow   = 100;   // knob plus the name under it
    constexpr int kPairRow   = 130;   // Tone and Mix, side by side
    constexpr int kDriveRow  = 210;   // the one control that gets room
    constexpr int kSwitchRow = 40;
    constexpr int kOutputRow = 120;
    constexpr int kRule      = 20;

    constexpr int kSwitchWidth  = ui::Tokens::switchWidth;
    constexpr int kSwitchHeight = ui::Tokens::switchHeight;
}

SatPanel::SatPanel (ui::ModuleContext ctx)
    : ModulePanel (std::move (ctx)),
      inputGain   (context.params.param (Index::inputGain),   "INPUT"),
      drive       (context.params.param (Index::drive),       "DRIVE", ui::Knob::Style::character, 0.66f, context.def.accent),
      tone        (context.params.param (Index::tone),        "TONE",  ui::Knob::Style::character, 0.46f, context.def.accent),
      mix         (context.params.param (Index::mix),         "MIX",   ui::Knob::Style::character, 0.46f, context.def.accent),
      outputLevel (context.params.param (Index::outputLevel), "OUTPUT"),
      satIn    (context.params.param (Index::satIn),    "SAT", context.def.accent),
      phase    (context.params.param (Index::phase),    ui::BmoLookAndFeel::phaseGlyph(), ui::tokens().polarity),
      autoGain (context.params.param (Index::autoGain), "AUTO", ui::tokens().switchAlt),
      meter    (context.peak, context.rms)
{
    for (auto* c : std::initializer_list<juce::Component*> {
             &inputGain, &drive, &tone, &mix, &satIn, &phase, &autoGain, &outputLevel, &meter })
        addAndMakeVisible (c);

    // Polarity is white in every module; its label is what says which module.
    phase.setActiveInkFrom (context.def.accent);
}

void SatPanel::paintPanel (juce::Graphics& g)
{
    for (const auto& r : rules)
    {
        if (r.text.isEmpty())
            drawRule (g, r.row);
        else
            drawRuleLegend (g, r.row, r.text, context.def.accent);
    }
}

void SatPanel::resized()
{
    rules.clear();
    auto area = getLocalBounds().reduced (kPad, 6);

    const auto rule = [&] (const juce::String& text)
    {
        rules.push_back ({ area.removeFromTop (kRule), text });
    };

    inputGain.setBounds (area.removeFromTop (kGainRow));

    // A little air between a control's name and the legend of the section
    // below it, or the two read as one block of text.
    area.removeFromTop (6);

    rule ("SATURATION");
    drive.setBounds (area.removeFromTop (kDriveRow));

    // Tone and Mix share a row: neither is the reason you reached for this,
    // and side by side they read as the two things you adjust after the fact.
    rule ("TONE   /   BLEND");

    {
        auto pair = area.removeFromTop (kPairRow);
        const auto half = pair.getWidth() / 2;
        tone.setBounds (pair.removeFromLeft (half));
        mix.setBounds (pair);
    }

    rule ({});

    {
        auto switches = area.removeFromTop (kSwitchRow);
        constexpr int gap = 6;

        auto group = switches.withSizeKeepingCentre (kSwitchWidth * 3 + gap * 2, kSwitchHeight);
        satIn.setBounds (group.removeFromLeft (kSwitchWidth));
        group.removeFromLeft (gap);
        phase.setBounds (group.removeFromLeft (kSwitchWidth));
        group.removeFromLeft (gap);
        autoGain.setBounds (group);
    }

    {
        auto bottom = area.removeFromTop (kOutputRow);

        meter.setBounds (bottom.withTrimmedTop (4).withTrimmedBottom (22).removeFromRight (44));
        outputLevel.setBounds (bottom);
    }
}

} // namespace bmo::sat

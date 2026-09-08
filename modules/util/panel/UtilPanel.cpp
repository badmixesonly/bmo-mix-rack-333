#include "UtilPanel.h"
#include "modules/util/params.h"

namespace bmo::util
{

namespace
{
    constexpr int kKnobRow   = 126;
    constexpr int kSwitchRow = 34;
    constexpr int kSwitchGap = ui::Tokens::switchGap;
    /// The image section -- named in the code, not on the panel -- carries two
    /// knobs and a switch, and the space the meter and two rules used to take
    /// is spent on them: 126 before, which is what GAIN still gets.
    constexpr int kImageKnobRow = 150;

    /** Air above and below the image section, and nothing more than that.

        These two were 62 and 0 until 0.2.3, and the 62 was an alignment
        constant: it existed to push this panel's lower rule down onto the line
        BMO EQ draws its own at, worked out by summing EQ's entire column. This
        panel now takes `ui::ModulePanel`'s output-section reservation like the
        other two, so the rule lands there structurally and these are free to be
        what they look like -- the same gap either side of the section between
        them. */
    constexpr int kGainToRule = 31;
    constexpr int kMonoToRule = 31;

    constexpr int kSwitchWidth  = ui::Tokens::switchWidth;
    constexpr int kSwitchHeight = ui::Tokens::switchHeight;
}

UtilPanel::UtilPanel (ui::ModuleContext ctx)
    : ModulePanel (std::move (ctx)),
      // Character, not utility. Utility is INPUT and OUTPUT -- the trim pair
      // every module carries, drawn the same pale blue everywhere so they read
      // as the same control wherever you meet them. This one has their
      // behaviour and a different job, so it takes its own colour.
      gain  (context.params.param (Index::gain),  "VOLUME", ui::Knob::Style::character, 0.5f,
             ui::tokens().utilGain),
      pan   (context.params.param (Index::pan),   "PAN",   ui::Knob::Style::character, 0.5f, context.def.accent),
      width (context.params.param (Index::width), "WIDTH", ui::Knob::Style::character, 0.5f, context.def.accent),
      phaseL (context.params.param (Index::phaseL), ui::BmoLookAndFeel::phaseGlyph() + " L", ui::tokens().polarity),
      phaseR (context.params.param (Index::phaseR), ui::BmoLookAndFeel::phaseGlyph() + " R", ui::tokens().polarity),
      mono   (context.params.param (Index::mono),   "MONO", context.def.accent)
{
    for (auto* c : std::initializer_list<juce::Component*> {
             &gain, &pan, &width, &phaseL, &phaseR, &mono })
        addAndMakeVisible (c);

    // No setKnobSide here. Tokens::gainKnobSide caps INPUT and OUTPUT at the
    // one size BMO EQ's column can afford; this knob is not one of those and
    // takes the room its own row gives it.

    // Polarity is white in every module; its label is what says which module.
    for (auto* p : { &phaseL, &phaseR })
        p->setActiveInkFrom (context.def.accent);
}

void UtilPanel::paintPanel (juce::Graphics& g)
{
    for (const auto& r : rules)
    {
        if (r.text.isEmpty())
            drawRule (g, r.row);
        else
            drawRuleLegend (g, r.row, r.text, context.def.accent);
    }
}

void UtilPanel::resized()
{
    rules.clear();
    auto area = getLocalBounds().reduced (kPad, 4);

    const auto rule = [&] (const juce::String& text)
    {
        rules.push_back ({ area.removeFromTop (kRuleRow), text });
    };

    // Three sections instead of five.
    //
    // PAN and WIDTH each had a rule of their own carrying the same word the
    // knob's caption said ten pixels below it. They are one idea -- what the
    // module does to the stereo image -- so they are one section, and MONO
    // belongs in it: it is the far end of the same control WIDTH is the middle
    // of. POLARITY loses its rule because two switches with a phase glyph on
    // them do not need to be told apart from anything.
    //
    // The meter goes. Its rule stays, and the polarity pair moves into the
    // space under it, which is the one place on this panel that was doing
    // nothing.
    const auto centred = [] (juce::Rectangle<int> row)
    {
        return row.withSizeKeepingCentre (kSwitchWidth, kSwitchHeight);
    };

    // No input section: VOLUME is what this module does, not a trim either side
    // of it. No output section either -- there is no output stage and the meter
    // is gone. But the *reservation* is taken, because that is what puts this
    // panel's lower rule on the same line as the other two, and the polarity
    // pair then centres in the body the section would have used.
    const auto out = takeOutputSection (area);

    // The knob and its name centre in the section as one object, rather than
    // sitting at the top of it with the whole gap underneath. The section is
    // everything above the first rule, so the 31 px splits either side.
    {
        auto section = area.removeFromTop (kKnobRow + kGainToRule);
        gain.setBounds (section.withSizeKeepingCentre (section.getWidth(), kKnobRow));
    }

    // A bare rule. Only BMO EQ names its sections -- see modules/AGENTS.md.
    rule ({});
    pan.setBounds   (area.removeFromTop (kImageKnobRow));
    width.setBounds (area.removeFromTop (kImageKnobRow));

    // A knob pins its caption to the foot of its row, so MONO needs a gap put
    // in by hand or it sits against the word WIDTH and reads as a second line
    // of it.
    area.removeFromTop (kSwitchGap * 2);
    mono.setBounds  (centred (area.removeFromTop (kSwitchRow)));

    area.removeFromTop (kMonoToRule);
    rules.push_back ({ out.rule, {} });

    // The pair centres in the body the output section would have used, so the
    // rule above them reads as the top of a section rather than as a line ruled
    // under the one before it.
    auto polarity = out.body.withSizeKeepingCentre (kSwitchWidth, kSwitchRow * 2 + kSwitchGap);

    phaseL.setBounds (centred (polarity.removeFromTop (kSwitchRow)));
    polarity.removeFromTop (kSwitchGap);
    phaseR.setBounds (centred (polarity.removeFromTop (kSwitchRow)));
}

} // namespace bmo::util

#include "DimPanel.h"
#include "modules/dim/params.h"

namespace bmo::dim
{

namespace
{
    // WIDTH gets Opto's knob size; it is the one control on this panel anybody
    // reaches for without thinking, and it sits where Opto puts its meter.
    constexpr int kBigKnobSide   = 92;
    constexpr int kBigKnobHeight = 150;

    // Everything else is paired two to a row. 64 is between BMO EQ's 56 and
    // Opto's 92, and it is what ten controls in a 220 px column can afford --
    // the alternative was nine knobs at full size in a panel 300 px too short.
    constexpr int kPairKnobSide   = 64;
    constexpr int kPairKnobHeight = 104;

    // SHUFFLE's share of its row, against the 100 a straight half would give
    // it. See the `pair` lambda in resized() for why it is not half.
    constexpr int kShuffleShare = 110;

    constexpr int kSwitchWidth  = ui::Tokens::switchWidth;
    constexpr int kSwitchHeight = ui::Tokens::switchHeight;
    constexpr int kSwitchGap    = ui::Tokens::switchGap;

    // Captions are the short forms on purpose. PlainKnob draws its caption
    // inside its own width, and half of a 200 px row is 100 px: "ASYMMETRY" at
    // 15 pt does not fit that and would clip the way BMO Opto's MAKEUP did for
    // a whole release. ui_layout_tests asserts the overflow rather than
    // trusting this comment.
}

DimPanel::DimPanel (ui::ModuleContext ctx)
    : ModulePanel (std::move (ctx)),
      width       (context.params.param (Index::width),       "WIDTH",
                   ui::Knob::Style::character, 0.62f, context.def.accent),
      shuffle     (context.params.param (Index::shuffle),     "SHUFFLE",
                   ui::Knob::Style::character, 0.62f, context.def.accent),
      shuffleFreq (context.params.param (Index::shuffleFreq), "FREQ",
                   ui::Knob::Style::character, 0.62f, context.def.accent),
      cents       (context.params.param (Index::detune),      "CENTS",
                   ui::Knob::Style::character, 0.62f, context.def.accent),
      diffuse     (context.params.param (Index::diffuse),     "DIFFUSE",
                   ui::Knob::Style::character, 0.62f, context.def.accent),
      rate        (context.params.param (Index::rate),        "RATE",
                   ui::Knob::Style::character, 0.62f, context.def.accent),
      depth       (context.params.param (Index::depth),       "DEPTH",
                   ui::Knob::Style::character, 0.62f, context.def.accent),
      rotation    (context.params.param (Index::rotation),    "ROTATE",
                   ui::Knob::Style::character, 0.62f, context.def.accent),
      asymmetry   (context.params.param (Index::asymmetry),   "ASYM",
                   ui::Knob::Style::character, 0.62f, context.def.accent),

      // Not a bypass, not mono, not polarity -- so `switchAlt`, per the table
      // in modules/AGENTS.md.
      detuneOn (context.params.param (Index::detuneOn), "DETUNE", ui::tokens().switchAlt)
{
    width.setKnobSide (kBigKnobSide);

    for (auto* k : { &shuffle, &shuffleFreq, &cents, &diffuse, &rate, &depth,
                     &rotation, &asymmetry })
        k->setKnobSide (kPairKnobSide);

    for (auto* c : std::initializer_list<juce::Component*> {
             &detuneOn, &cents, &diffuse, &rate, &depth, &width,
             &shuffle, &shuffleFreq, &rotation, &asymmetry })
        addAndMakeVisible (c);
}

void DimPanel::resized()
{
    auto area = getLocalBounds().reduced (kPad, 4);

    // Two knobs abreast, each centred in its own share so the pair reads as a
    // pair rather than as two controls that happen to be adjacent.
    //
    // `leftShare` is not always half. A caption is drawn inside its own knob's
    // box, and at 15 pt "SHUFFLE" is 106 px against the 100 a straight half
    // gives it -- it clipped to "SHUFFL" on the first render, which is BMO
    // Opto's MAKEUP bug over again. Giving that row 110/90 fixes it without
    // abbreviating the word or widening the panel. The knobs are capped at
    // kPairKnobSide and centred in whatever they are handed, so the only thing
    // this moves is the caption box; the two knob centres shift 5 px, which is
    // less than the dot on their own tracks.
    const auto pair = [] (juce::Rectangle<int> row, ui::PlainKnob& a, ui::PlainKnob& b,
                          int leftShare = 0)
    {
        const auto left = leftShare > 0 ? leftShare : row.getWidth() / 2;
        a.setBounds (row.removeFromLeft (left));
        b.setBounds (row);
    };

    // Opto's rhythm: every block placed from the top on one derived gap, with a
    // margin above the first and below the last, so the spacing stays even if a
    // block's height changes later. Six blocks, seven divisions.
    const auto content = kSwitchHeight + kPairKnobHeight * 4 + kBigKnobHeight;
    const auto gap     = juce::jmax (kSwitchGap, (area.getHeight() - content) / 7);

    area.removeFromTop (gap);

    // -- Generate --------------------------------------------------------
    // The switch first, because it decides whether the two knobs under it do
    // anything at all.
    detuneOn.setBounds (area.removeFromTop (kSwitchHeight)
                            .withSizeKeepingCentre (kSwitchWidth, kSwitchHeight));
    area.removeFromTop (gap);

    pair (area.removeFromTop (kPairKnobHeight), cents, diffuse);
    area.removeFromTop (gap);

    // -- Diffuse ---------------------------------------------------------
    pair (area.removeFromTop (kPairKnobHeight), rate, depth);
    area.removeFromTop (gap);

    // -- Image -----------------------------------------------------------
    width.setBounds (area.removeFromTop (kBigKnobHeight));
    area.removeFromTop (gap);

    pair (area.removeFromTop (kPairKnobHeight), shuffle, shuffleFreq, kShuffleShare);
    area.removeFromTop (gap);

    pair (area.removeFromTop (kPairKnobHeight), rotation, asymmetry);
}

} // namespace bmo::dim

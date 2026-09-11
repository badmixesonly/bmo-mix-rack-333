// Layout assertions on the real panels.
//
// Nine suites existed before this one and not one of them touched the UI.
// Everything in testing-notes/ui-editor-handoff.md §6 was checked by hand,
// which is why two layout faults shipped in 0.2.1 and why 0.2.3 left three
// hand-matched alignments holding the rack together with nothing watching
// them.
//
// The house rule, from tests/dsp/OptoDspTests.cpp: **assert absolutes, not
// comparisons.** A release test passed for a whole release while both modes
// were broken because it only compared them to each other. Every number below
// is a fixed pixel row named in ui::ModulePanel, not "the same as last time"
// and not "inside the panel somewhere".
//
// No rendering happens here. A panel is laid out by its editor's constructor
// at design size regardless of what the editor is later scaled to, so
// constructing the editor is enough to ask where everything landed.

#include "products/deq/Product.h"
#include "modules/deq/panel/Widgets.h"
#include "products/dim/Product.h"
#include "products/eq/Product.h"
#include "products/opto/Product.h"
#include "products/sat/Product.h"
#include "products/util/Product.h"
#include "products/rack/Product.h"

#include "core/ui/ModulePanel.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <iostream>
#include <vector>

namespace
{

int failures = 0;

void check (bool condition, const juce::String& what)
{
    if (! condition)
    {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

void checkEquals (int actual, int expected, const juce::String& what)
{
    if (actual != expected)
    {
        std::cerr << "FAIL: " << what << " -- expected " << expected
                  << ", got " << actual << '\n';
        ++failures;
    }
}

//== Getting at the panels =====================================================

/** Every ModulePanel under `root`, at any depth.
    A product editor has one; the rack editor has one per slot. */
void collectPanels (juce::Component& root, std::vector<bmo::ui::ModulePanel*>& out)
{
    for (auto* child : root.getChildren())
    {
        if (auto* panel = dynamic_cast<bmo::ui::ModulePanel*> (child))
            out.push_back (panel);

        collectPanels (*child, out);
    }
}

/** A named descendant, or null. Controls name themselves after the caption a
    reader sees -- see PlainKnob's constructor -- so "OUTPUT" here is the same
    OUTPUT that is printed on the panel. */
juce::Component* findNamed (juce::Component& root, const juce::String& name)
{
    for (auto* child : root.getChildren())
    {
        if (child->getName() == name)
            return child;

        if (auto* found = findNamed (*child, name))
            return found;
    }

    return nullptr;
}

/** Every PlainKnob under `root`. */
void collectKnobs (juce::Component& root, std::vector<bmo::ui::PlainKnob*>& out)
{
    for (auto* child : root.getChildren())
    {
        if (auto* knob = dynamic_cast<bmo::ui::PlainKnob*> (child))
            out.push_back (knob);

        collectKnobs (*child, out);
    }
}

/** Every SwitchButton under `root`. */
void collectSwitches (juce::Component& root, std::vector<bmo::ui::SwitchButton*>& out)
{
    for (auto* child : root.getChildren())
    {
        if (auto* sw = dynamic_cast<bmo::ui::SwitchButton*> (child))
            out.push_back (sw);

        collectSwitches (*child, out);
    }
}

//== The numbers ===============================================================
//
// ui::ModulePanel's own constants, written out as the literals its header
// comment states. Deriving them from the constants would make this test agree
// with any value they took, including a wrong one; the point is to pin the
// panels to the documented rows.

constexpr int kInputKnobTop    = 4;
constexpr int kInputKnobBottom = 82;    ///< exclusive: knob occupies 4..81
constexpr int kInputRuleCentre = 90;

constexpr int kOutputRuleCentre = 566;
constexpr int kSwitchRowTop     = 574;
constexpr int kSwitchRowBottom  = 602;  ///< exclusive: switches occupy 574..601
constexpr int kOutputKnobTop    = 602;
constexpr int kOutputKnobBottom = 680;  ///< exclusive: knob occupies 602..679

/** Where a panel's rules landed, as centre rows, in the order laid out. */
std::vector<int> ruleCentres (const bmo::ui::ModulePanel& panel)
{
    std::vector<int> out;

    for (const auto& r : panel.getRules())
        out.push_back (r.row.getCentreY());

    return out;
}

//== The assertions ============================================================

/** Asserts a rule landed on `centre`, and says where they all are if not. */
void checkHasRuleAt (const bmo::ui::ModulePanel& panel, int centre, const juce::String& who)
{
    const auto rules = ruleCentres (panel);

    if (std::find (rules.begin(), rules.end(), centre) != rules.end())
        return;

    juce::String where;

    for (auto r : rules)
        where << (where.isEmpty() ? "" : ", ") << r;

    check (false, who + " has no rule centred on row " + juce::String (centre)
                      + " -- its rules are at " + (where.isEmpty() ? "no rows at all" : where));
}

/** A panel that takes the input section puts its trim knob on rows 4..81 and
    its rule's centre on row 90. */
void checkInputSection (bmo::ui::ModulePanel& panel, const juce::String& who)
{
    auto* input = findNamed (panel, "INPUT");

    if (input == nullptr)
    {
        check (false, who + " has no INPUT knob");
        return;
    }

    checkEquals (input->getY(), kInputKnobTop, who + " INPUT knob top");
    checkEquals (input->getBottom(), kInputKnobBottom, who + " INPUT knob bottom");

    checkHasRuleAt (panel, kInputRuleCentre, who);
}

/** Every panel that takes *or reserves* the output section puts a rule's
    centre on row 566.

    Util is the one that proves the reservation works: it has no output knob
    and no switch row down there, and its lower rule still has to land on the
    same line as EQ's and the Saturator's. That alignment is one of the three
    hand-matched ones 0.2.3 left holding the rack together. */
void checkOutputRule (bmo::ui::ModulePanel& panel, const juce::String& who)
{
    checkHasRuleAt (panel, kOutputRuleCentre, who);
}

/** A panel that adopts the output section puts its trim knob on rows 602..679
    and its switches on 574..601. */
void checkOutputSection (bmo::ui::ModulePanel& panel, const juce::String& who)
{
    auto* output = findNamed (panel, "OUTPUT");

    if (output == nullptr)
    {
        check (false, who + " has no OUTPUT knob");
        return;
    }

    checkEquals (output->getY(), kOutputKnobTop, who + " OUTPUT knob top");
    checkEquals (output->getBottom(), kOutputKnobBottom, who + " OUTPUT knob bottom");
}

/** The switch named `name` sits in the output section's switch row. */
void checkOutputSwitch (bmo::ui::ModulePanel& panel, const juce::String& name,
                        const juce::String& who)
{
    auto* sw = findNamed (panel, name);

    if (sw == nullptr)
    {
        check (false, who + " has no " + name + " switch");
        return;
    }

    check (sw->getY() >= kSwitchRowTop && sw->getBottom() <= kSwitchRowBottom,
           who + " " + name + " should sit within rows " + juce::String (kSwitchRowTop)
               + ".." + juce::String (kSwitchRowBottom - 1) + ", is "
               + juce::String (sw->getY()) + ".." + juce::String (sw->getBottom() - 1));
}

/** BMO EQ's band column, pinned to absolute rows.

    This is the assertion the suite mainly exists for, and the one that would
    have caught the regression the repository is most exposed to.

    The three bands and the low cut are taken off the top in sequence, between
    the two shared sections. Change kBandRow by one pixel and every row below
    it moves, the column comes up short of the output rule, and *nothing else
    in this file notices*: the input knob, the output knob, the switch row and
    both shared rules are all still exactly where they were, because the
    sections are taken off the two ends before the bands get what is left.

    So the rows are written out. `ui_layout_tests --dump` prints them.

    The flush check at the end is the one that carries the most: BMO EQ has no
    vertical slack anywhere -- no empty band over 16 px on the whole panel --
    so its column ending exactly on the output rule is a real property of this
    layout rather than a coincidence worth asserting loosely. */
void checkEqBandColumn (bmo::ui::ModulePanel& panel)
{
    struct Row { const char* name; int top, height; };

    // 98 is the first row under the input section's rule; 558 is the top of
    // the output section's.
    constexpr Row rows[] = {
        { "HIGH",   98, 112 },
        { "MID",   226, 112 },
        { "LOW",   354, 112 },
        { "LO-CUT", 482, 76 },
    };

    constexpr int kOutputRuleTop = 558;

    for (const auto& row : rows)
    {
        auto* band = findNamed (panel, row.name);

        if (band == nullptr)
        {
            check (false, juce::String ("eq has no ") + row.name + " band");
            continue;
        }

        checkEquals (band->getY(), row.top,
                     juce::String ("eq ") + row.name + " band top");
        checkEquals (band->getHeight(), row.height,
                     juce::String ("eq ") + row.name + " band height");
    }

    if (auto* lowCut = findNamed (panel, "LO-CUT"))
        checkEquals (lowCut->getBottom(), kOutputRuleTop,
                     "eq band column should end flush against the output rule, and its foot");
}

/** Util reserves the output section without adopting it: the rule is on the
    shared line and there is nothing below it that belongs to an output stage. */
void checkReservesWithoutAdopting (bmo::ui::ModulePanel& panel, const juce::String& who)
{
    check (findNamed (panel, "OUTPUT") == nullptr,
           who + " should have no OUTPUT knob -- it reserves the section, it does not take it");
}

/** No control escapes its panel. */
void checkWithinPanel (bmo::ui::ModulePanel& panel, const juce::String& who)
{
    const auto bounds = panel.getLocalBounds();

    for (auto* child : panel.getChildren())
        check (bounds.contains (child->getBounds()),
               who + " control '" + child->getName() + "' at " + child->getBounds().toString()
                   + " is outside the panel " + bounds.toString());
}

/** No two of a panel's controls overlap.

    They are laid out in a single column in every module, so an overlap is
    always a mistake rather than a design. */
void checkNoOverlap (bmo::ui::ModulePanel& panel, const juce::String& who)
{
    const auto& children = panel.getChildren();

    for (int i = 0; i < children.size(); ++i)
        for (int j = i + 1; j < children.size(); ++j)
        {
            const auto a = children[i]->getBounds();
            const auto b = children[j]->getBounds();

            check (! a.intersects (b),
                   who + " controls '" + children[i]->getName() + "' " + a.toString()
                       + " and '" + children[j]->getName() + "' " + b.toString() + " overlap");
        }
}

/** Every knob caption fits the box it is drawn in.

    This is the MAKEUP -> MAKEU bug, which was a five-character overflow that
    survived a full release because nobody measured it. The measurement lives
    on PlainKnob so it uses the same font and the same box paint does. */
void checkCaptionsFit (bmo::ui::ModulePanel& panel, const juce::String& who)
{
    std::vector<bmo::ui::PlainKnob*> knobs;
    collectKnobs (panel, knobs);

    check (! knobs.empty(), who + " has no knobs, which cannot be right");

    for (auto* knob : knobs)
    {
        const auto overflow = knob->captionOverflow();

        check (overflow <= 0.0f,
               who + " caption '" + knob->getName() + "' overflows its box by "
                   + juce::String (overflow, 1) + " px");
    }

    // A stepped dial's legend too. BMO EQ's are frequencies ("1k6"); BMO DEQ's
    // shape dial is the first to carry words (setLegend), and words are what
    // outgrow a 38 px box.
    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (auto* band = dynamic_cast<bmo::ui::ConcentricBand*> (child))
                check (band->legendOverflow() <= 0.0f,
                       who + " a legend on '" + c.getName() + "' overflows its box by "
                           + juce::String (band->legendOverflow(), 1) + " px");

            walk (*child);
        }
    };
    walk (panel);
}

/** BMO DEQ's own: the band's knobs print their values, the shape is the
    stepped dial with its name fitting under it, and AUTO shares the output
    row with DEQ. Frosty's three calls on the first build (2026-09-11), so a
    later layout pass cannot quietly undo one. */
void checkDeqPanel (bmo::ui::ModulePanel& panel, const juce::String& who)
{
    std::vector<bmo::ui::PlainKnob*> knobs;
    collectKnobs (panel, knobs);

    auto band = 0;
    for (auto* knob : knobs)
        if (knob->getName() != "OUTPUT")
        {
            ++band;
            check (knob->isShowingValue(), who + " knob '" + knob->getName() + "' shows no value");
        }

    checkEquals (band, 8, who + " band knobs (FREQ GAIN Q THRESH RANGE RATIO ATTACK RELEASE)");

    auto* shape = dynamic_cast<bmo::deq::ShapeDial*> (findNamed (panel, "SHAPE"));
    check (shape != nullptr, who + " has no SHAPE dial");

    if (shape != nullptr)
        check (shape->captionOverflow() <= 0.0f,
               who + " SHAPE or its legend overflows by " + juce::String (shape->captionOverflow(), 1) + " px");

    checkOutputSwitch (panel, "AUTO", who);
}

/** Every switch label fits its switch.

    Switches are one size across the whole suite -- Tokens::switchWidth -- so a
    label is only ever as wide as the word someone chose. Nothing measured
    that until now; the knob captions were covered and the switches were not,
    which is half the text on a panel. */
void checkSwitchLabelsFit (bmo::ui::ModulePanel& panel, const juce::String& who)
{
    std::vector<bmo::ui::SwitchButton*> switches;
    collectSwitches (panel, switches);

    for (auto* sw : switches)
    {
        const auto overflow = sw->labelOverflow();

        check (overflow <= 0.0f,
               who + " switch '" + sw->getName() + "' label overflows its box by "
                   + juce::String (overflow, 1) + " px");
    }

    // And every toggle that is *not* inside a SwitchButton: BMO DEQ's rows of
    // switches for a choice (STEREO / MID / SIDE, the five shapes) are plain
    // ToggleButtons drawn by the same look and feel. They clipped to "BEL" and
    // "LO CU" on DEQ's first render while this check still only looked for
    // SwitchButtons -- the same blind spot MAKEUP fell through, one class over.
    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (dynamic_cast<bmo::ui::SwitchButton*> (child) != nullptr)
                continue;

            if (auto* toggle = dynamic_cast<juce::ToggleButton*> (child))
            {
                const auto overflow = bmo::ui::BmoLookAndFeel::toggleLabelOverflow (*toggle);

                check (overflow <= 0.0f,
                       who + " switch '" + toggle->getButtonText() + "' label overflows its box by "
                           + juce::String (overflow, 1) + " px");
            }

            walk (*child);
        }
    };
    walk (panel);
}

//== The meter scales ==========================================================

/** A printed scale is well formed.

    These are the cheap invariants — no rendering, no component, just the
    table. They exist because the reduction scale's fractions stopped being
    computed on 8 Sep and became thirteen numbers typed by hand, and a
    transposed pair in that list is invisible: the needle would simply run
    backwards over a stretch of the dial and every other test would pass.

    What is *not* checked here is crowding, which is the thing that actually
    decides whether a figure can be added. That is a pixel question — it
    depends on the label ring radius and on how many digits each figure has —
    so `kMinInkedGap` below is a floor to catch someone jamming figures in, not
    the real limit. Measure a new figure with `tools/inspect` before trusting
    it; `testing-notes/ui-editor-handoff.md` carries the numbers. */
void checkScale (const std::vector<bmo::ui::DynamicsMeter::ScalePoint>& scale,
                 const juce::String& who, float lastValue)
{
    if (scale.size() < 2)
    {
        check (false, who + " scale needs at least two points");
        return;
    }

    // The ends are the sweep. A scale that did not start at 0 or reach 1 would
    // leave part of the dial unreachable by the needle.
    check (scale.front().fraction == 0.0f,
           who + " scale should start at fraction 0, starts at "
               + juce::String (scale.front().fraction, 3));
    check (scale.back().fraction == 1.0f,
           who + " scale should end at fraction 1, ends at "
               + juce::String (scale.back().fraction, 3));
    check (scale.back().value == lastValue,
           who + " scale should end at " + juce::String (lastValue, 1)
               + ", ends at " + juce::String (scale.back().value, 1));

    // Both axes strictly increasing. This is the one that catches a typo.
    for (size_t i = 1; i < scale.size(); ++i)
    {
        check (scale[i].value > scale[i - 1].value,
               who + " scale values must increase: " + juce::String (scale[i - 1].value, 1)
                   + " then " + juce::String (scale[i].value, 1));

        check (scale[i].fraction > scale[i - 1].fraction,
               who + " scale fractions must increase: " + juce::String (scale[i - 1].fraction, 3)
                   + " then " + juce::String (scale[i].fraction, 3)
                   + " (at " + juce::String (scale[i].value, 1) + ")");
    }

    // A floor, not the real limit. See the note above.
    constexpr float kMinInkedGap = 0.10f;

    const bmo::ui::DynamicsMeter::ScalePoint* previousInked = nullptr;

    for (const auto& p : scale)
    {
        if (! p.numbered)
            continue;

        if (previousInked != nullptr)
            check (p.fraction - previousInked->fraction >= kMinInkedGap,
                   who + " printed figures " + juce::String (previousInked->value, 1)
                       + " and " + juce::String (p.value, 1) + " are only "
                       + juce::String (p.fraction - previousInked->fraction, 3)
                       + " of the sweep apart, under the " + juce::String (kMinInkedGap, 2)
                       + " floor -- measure it before printing both");

        previousInked = &p;
    }
}

//== Driving it ================================================================

struct Product
{
    const char* who;
    std::unique_ptr<juce::AudioProcessor> (*create)();
};

/** Runs `body` against the panel of a single-module product. */
template <typename Fn>
void withPanel (const Product& product, Fn&& body)
{
    auto processor = product.create();
    processor->prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorAndMakeActive());

    if (editor == nullptr)
    {
        check (false, juce::String (product.who) + " has no editor");
        return;
    }

    std::vector<bmo::ui::ModulePanel*> panels;
    collectPanels (*editor, panels);

    if (panels.size() != 1)
        check (false, juce::String (product.who) + " should have exactly one panel, has "
                          + juce::String ((int) panels.size()));
    else
        body (*panels.front());

    // The editor goes before its processor.
    processor->editorBeingDeleted (editor.get());
    editor.reset();
}

} // namespace

//== The rack ==================================================================

/** The rack is why the shared rows matter at all.

    Each panel being internally correct is not the same as a rack reading as
    one surface: put one slot a few pixels lower and every shared rule
    misaligns while every per-panel assertion above still passes. So this
    asserts the two things that turn panel-local rows into rack-wide
    alignment -- that the slots sit on one baseline, and that the rules which
    are supposed to line up actually land on the same absolute row.

    testing-notes/ui-editor-handoff.md calls these hand-matched alignments
    load-bearing for how a rack reads, and until now nothing watched them. */
void checkRack (juce::Component& editor)
{
    std::vector<bmo::ui::ModulePanel*> panels;
    collectPanels (editor, panels);

    if (panels.size() != 4)
    {
        check (false, "rack should have four panels, has " + juce::String ((int) panels.size()));
        return;
    }

    // One baseline. Every slot's panel is placed at the same y and the same
    // height, which is what makes a panel-local row a rack-wide row.
    const auto top = panels.front()->getY();

    for (auto* panel : panels)
    {
        checkEquals (panel->getY(), top, "rack slot top");
        checkEquals (panel->getHeight(), bmo::ui::ModulePanel::kContentHeight, "rack slot height");
    }

    // The panels tile without a gap or an overlap, so the plate reads as one
    // surface rather than four cards.
    auto ordered = panels;
    std::sort (ordered.begin(), ordered.end(),
               [] (auto* a, auto* b) { return a->getX() < b->getX(); });

    for (size_t i = 1; i < ordered.size(); ++i)
        checkEquals (ordered[i]->getX(), ordered[i - 1]->getRight(),
                     "rack slot " + juce::String ((int) i) + " should start where the one before ends");

    // The shared rules, as absolute rack rows rather than as "the same as each
    // other" -- comparing them to one another would pass on a rack that put
    // every slot equally wrong, which is the trap tests/dsp/OptoDspTests.cpp
    // was written to avoid.
    //
    // A slot's panel starts under the product header and the slot bar:
    // 28 + 24 = 52. So the output line is 52 + 566 and the input line 52 + 90.
    constexpr int kSlotTop = 52;

    checkEquals (top, kSlotTop, "rack slot top, absolutely");

    int withOutputRule = 0, withInputRule = 0;

    for (auto* panel : ordered)
    {
        const auto rules = ruleCentres (*panel);

        for (auto centre : rules)
        {
            if (centre == kOutputRuleCentre)
            {
                ++withOutputRule;
                checkEquals (panel->getY() + centre, kSlotTop + kOutputRuleCentre,
                             "rack output rule row");
            }
            else if (centre == kInputRuleCentre)
            {
                ++withInputRule;
                checkEquals (panel->getY() + centre, kSlotTop + kInputRuleCentre,
                             "rack input rule row");
            }
        }
    }

    // util, eq and sat all put a rule on the output line; opto takes neither
    // section and reserves nothing. eq and sat take the input section, util
    // does not. If a module changes its mind about that, this is where it
    // shows up rather than in a screenshot.
    checkEquals (withOutputRule, 3, "rack panels sharing the output line");
    checkEquals (withInputRule,  2, "rack panels sharing the input line");
}

/** Prints a panel's controls and rules. `ui_layout_tests --dump` is how you
    find out what a panel actually does before writing a number down about it,
    rather than deriving one from the constants and asserting the derivation. */
void dump (bmo::ui::ModulePanel& panel, const juce::String& who)
{
    std::cout << "== " << who << "  " << panel.getWidth() << "x" << panel.getHeight() << '\n';

    for (auto* child : panel.getChildren())
        std::cout << "   " << child->getBounds().toString()
                  << "   y " << child->getY() << ".." << (child->getBottom() - 1)
                  << "   " << child->getName() << '\n';

    for (const auto& r : panel.getRules())
        std::cout << "   rule  y " << r.row.getY() << ".." << (r.row.getBottom() - 1)
                  << "  centre " << r.row.getCentreY()
                  << (r.text.isEmpty() ? "" : "  \"" + r.text + "\"") << '\n';
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const auto dumping = argc > 1 && juce::String (argv[1]) == "--dump";

    using namespace bmo::products;

    // Every panel, whatever it opts into: nothing escapes, nothing overlaps,
    // every caption fits.
    const Product all[] = {
        { "eq",   +[] () -> std::unique_ptr<juce::AudioProcessor> { return createEq(); } },
        { "sat",  +[] () -> std::unique_ptr<juce::AudioProcessor> { return createSat(); } },
        { "util", +[] () -> std::unique_ptr<juce::AudioProcessor> { return createUtil(); } },
        { "opto", +[] () -> std::unique_ptr<juce::AudioProcessor> { return createOpto(); } },
        { "dim",  +[] () -> std::unique_ptr<juce::AudioProcessor> { return createDim(); } },

        // BMO DEQ twice, once per width: standalone opens it full, and the
        // compact one is what a rack shows. Both are the same panel laid out
        // from the width it is given, so both are held to everything above.
        { "deq",  +[] () -> std::unique_ptr<juce::AudioProcessor> { return createDeq(); } },
        { "deq compact", +[] () -> std::unique_ptr<juce::AudioProcessor>
                         {
                             auto p = createDeq();
                             p->setExpanded (false);
                             return std::unique_ptr<juce::AudioProcessor> (p.release());
                         } },
    };

    if (dumping)
    {
        for (const auto& product : all)
            withPanel (product, [&] (bmo::ui::ModulePanel& panel) { dump (panel, product.who); });

        return 0;
    }

    for (const auto& product : all)
        withPanel (product, [&] (bmo::ui::ModulePanel& panel)
        {
            checkWithinPanel     (panel, product.who);
            checkNoOverlap       (panel, product.who);
            checkCaptionsFit     (panel, product.who);
            checkSwitchLabelsFit (panel, product.who);

            checkEquals (panel.getHeight(), bmo::ui::ModulePanel::kContentHeight,
                         juce::String (product.who) + " panel height");
        });

    // BMO EQ and the Saturator take both sections.
    for (const auto& product : { all[0], all[1] })
        withPanel (product, [&] (bmo::ui::ModulePanel& panel)
        {
            checkInputSection  (panel, product.who);
            checkOutputRule    (panel, product.who);
            checkOutputSection (panel, product.who);
        });

    // BMO EQ: Hi-Q joined the output switch row in 0.2.3, and the band column
    // between the two shared sections is pinned row by row.
    withPanel (all[0], [] (bmo::ui::ModulePanel& panel)
    {
        checkOutputSwitch (panel, "HI-Q", "eq");
        checkEqBandColumn (panel);
    });

    // BMO DEQ takes the output section at both widths, so its OUTPUT knob and
    // its DEQ switch sit on the same lines as every other module's in a rack.
    for (const auto& product : { all[5], all[6] })
        withPanel (product, [&] (bmo::ui::ModulePanel& panel)
        {
            checkOutputRule    (panel, product.who);
            checkOutputSection (panel, product.who);
            checkOutputSwitch  (panel, "DEQ", product.who);
            checkDeqPanel      (panel, product.who);
        });

    withPanel (all[5], [] (bmo::ui::ModulePanel& panel) { checkEquals (panel.getWidth(), 600, "deq opens full standalone"); });
    withPanel (all[6], [] (bmo::ui::ModulePanel& panel) { checkEquals (panel.getWidth(), 320, "deq compact width"); });

    // BMO Util reserves the output section and adopts neither half of it. This
    // is the case that proves a reservation is worth anything.
    withPanel (all[2], [] (bmo::ui::ModulePanel& panel)
    {
        checkOutputRule             (panel, "util");
        checkReservesWithoutAdopting (panel, "util");
    });

    // The meter scales. No component and no rendering: these are pure tables,
    // and the reduction one is hand-placed, which is why it is worth checking.
    checkScale (bmo::ui::DynamicsMeter::vuScale(), "VU", 3.0f);
    checkScale (bmo::ui::DynamicsMeter::reductionScale(), "reduction", 24.0f);

    // And the rack, which is the reason any of the shared rows exist.
    {
        auto rack = createRack();
        rack->prepareToPlay (48000.0, 512);

        rack->clearChain();

        for (const auto* id : { "util", "eq", "sat", "opto" })
            if (auto* def = rack->findModule (id))
                rack->addModule (*def);
            else
                check (false, juce::String ("rack has no module ") + id);

        std::unique_ptr<juce::AudioProcessorEditor> editor (rack->createEditorAndMakeActive());

        if (editor == nullptr)
            check (false, "rack has no editor");
        else
        {
            checkRack (*editor);

            std::vector<bmo::ui::ModulePanel*> panels;
            collectPanels (*editor, panels);

            for (auto* panel : panels)
            {
                checkWithinPanel     (*panel, "rack slot");
                checkNoOverlap       (*panel, "rack slot");
                checkCaptionsFit     (*panel, "rack slot");
                checkSwitchLabelsFit (*panel, "rack slot");
            }

            rack->editorBeingDeleted (editor.get());
            editor.reset();
        }
    }

    // BMO DEQ in a rack: compact, where a rack opens it, and on the rack-wide
    // output row like everything beside it. Its own block rather than a fifth
    // module in the one above, whose four-panel expectations are written out.
    {
        auto rack = createRack();
        rack->prepareToPlay (48000.0, 512);
        rack->clearChain();
        rack->addModule (*rack->findModule ("util"));
        rack->addModule (*rack->findModule ("deq"));

        std::unique_ptr<juce::AudioProcessorEditor> editor (rack->createEditorAndMakeActive());
        std::vector<bmo::ui::ModulePanel*> panels;
        collectPanels (*editor, panels);

        check (panels.size() == 2, "a util-and-deq rack has two panels");

        for (auto* panel : panels)
        {
            checkWithinPanel     (*panel, "rack deq slot");
            checkNoOverlap       (*panel, "rack deq slot");
            checkCaptionsFit     (*panel, "rack deq slot");
            checkSwitchLabelsFit (*panel, "rack deq slot");
        }

        if (panels.size() == 2)
        {
            auto* deq = panels[0]->getX() > panels[1]->getX() ? panels[0] : panels[1];
            checkEquals (deq->getWidth(), 320, "deq arrives in a rack compact");
            checkOutputRule (*deq, "rack deq");
            checkDeqPanel (*deq, "rack deq");
        }

        rack->editorBeingDeleted (editor.get());
        editor.reset();
    }

    if (failures == 0)
        std::cout << "All ui layout tests passed.\n";

    return failures == 0 ? 0 : 1;
}

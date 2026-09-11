/*
    The real panel, laid out at design size with no display: the hide rule,
    and that everything on it fits.

      - Every parameter has a control on the panel. On Hybrid every control
        is shown; on Classic, exactly the controls of the isHybridOnly()
        parameters are hidden. This is the panel half of the rule --
        tests/dsp/ModeTests.cpp is the DSP half -- so the two cannot drift.
      - Every switch label fits its switch, measured the way the suite's look
        and feel draws it.
      - Every value a box can show fits the box: all seventeen keys, every
        scale and range, every listed reference and a custom one.
      - No two visible controls overlap, and all of them are on the panel.

    The rack's tests/plugin/*LayoutTests.cpp are the model.
*/

#include "products/tune/Product.h"
#include "modules/tune/panel/TunePanel.h"
#include "tests/TestUtil.h"

#include <juce_gui_basics/juce_gui_basics.h>

using namespace bmo::tune;
using namespace bmo::tune::test;

namespace
{
    TunePanel* findPanel (juce::Component& root)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* panel = dynamic_cast<TunePanel*> (child))
                return panel;

            if (auto* found = findPanel (*child))
                return found;
        }

        return nullptr;
    }

    std::string nameOf (int index) { return specs()[(size_t) index].id; }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    auto processor = bmo::products::createTune();
    processor->prepareToPlay (48000.0, 512);
    auto& params = processor->getEngine().params();

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorAndMakeActive());
    auto* panel = editor != nullptr ? findPanel (*editor) : nullptr;
    check (panel != nullptr, "the editor has a TunePanel");

    if (panel == nullptr)
        return finish ("panel");

    check (panel->getWidth() == TunePanel::kDesignWidth
           && panel->getHeight() == bmo::ui::ModulePanel::kContentHeight,
           "the panel is laid out at 360 x 688");

    //== The hide rule ==========================================================
    for (const auto engine : { Engine::classic, Engine::hybrid })
    {
        params.setReal (Index::engine, (float) engine);
        panel->syncNow();
        const auto mode = std::string (engine == Engine::hybrid ? "Hybrid" : "Classic");

        int hidden = 0;

        for (int i = 0; i < Index::count; ++i)
        {
            const auto controls = panel->controlsFor (i);
            check (! controls.empty(), nameOf (i) + " has a control on the panel");

            const bool shouldShow = engine == Engine::hybrid || ! isHybridOnly (i);

            for (auto* c : controls)
            {
                check (c->isVisible() == shouldShow,
                       mode + ": " + nameOf (i) + "'s " + c->getName().toStdString()
                           + (shouldShow ? " is shown" : " is hidden"));
                hidden += c->isVisible() ? 0 : 1;
            }
        }

        report (mode + ": controls hidden", hidden);
    }

    //== Everything fits ========================================================
    params.setReal (Index::engine, (float) Engine::hybrid);
    panel->syncNow();

    for (auto* s : panel->switches())
    {
        const auto overflow = s->getButtonText().isEmpty() ? 0.0f : bmo::ui::BmoLookAndFeel::toggleLabelOverflow (*s);
        check (overflow <= 0.0f, "the " + s->getName().toStdString() + " switch's label fits: " + std::to_string (overflow) + " px over");
    }

    auto worstBox = [&] (int index, int choices)
    {
        float worst = -1.0e9f;
        std::string worstText;

        for (int c = 0; c < choices; ++c)
        {
            params.setReal (index, (float) c);
            panel->syncNow();

            for (const auto& b : panel->boxTexts())
                if (b.overflow > worst) { worst = b.overflow; worstText = b.id.toStdString() + " \"" + b.text.toStdString() + "\""; }
        }

        params.setReal (index, 0.0f);
        report ("widest text for " + nameOf (index) + ", px to spare", -worst);
        check (worst <= 0.0f, "every value of " + nameOf (index) + " fits its box; worst is " + worstText);
    };

    worstBox (Index::key, kNumKeySpellings);
    worstBox (Index::scale, 3);
    worstBox (Index::range, 5);

    for (const auto hz : { 440.0f, 432.0f, 415.0f, 444.0f, 467.3f, 380.0f })
    {
        params.setReal (Index::refA, hz);
        panel->syncNow();

        for (const auto& b : panel->boxTexts())
            if (b.id == "Ref A")
                check (b.overflow <= 0.0f, "Ref A \"" + b.text.toStdString() + "\" fits its box");
    }

    params.setReal (Index::refA, 440.0f);

    //== Nothing overlaps, and all of it is on the panel ========================
    std::vector<juce::Component*> visible;
    for (int i = 0; i < Index::count; ++i)
        for (auto* c : panel->controlsFor (i))
            if (c->isVisible() && std::find (visible.begin(), visible.end(), c) == visible.end())
                visible.push_back (c);

    for (auto* c : visible)
        check (panel->getLocalBounds().contains (c->getBounds()), c->getName().toStdString() + " is inside the panel");

    for (size_t a = 0; a < visible.size(); ++a)
        for (size_t b = a + 1; b < visible.size(); ++b)
            check (! visible[a]->getBounds().intersects (visible[b]->getBounds()),
                   visible[a]->getName().toStdString() + " and " + visible[b]->getName().toStdString() + " do not overlap");

    processor->editorBeingDeleted (editor.get());
    editor.reset();
    return finish ("panel");
}

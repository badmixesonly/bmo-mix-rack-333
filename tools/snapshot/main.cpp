// Renders a product's editor to a PNG without a display, so a layout change
// can be reviewed in a pull request rather than described in one.
//
//   snapshot <eq|sat|util|opto|dim|rack> out.png [width height] [param=value ...]
//
// For the rack, "chain=util,eq,sat,opto" sets the modules and "N.id=value"
// sets a parameter of the module in slot N (1-based), e.g. 2.mid_gain=4.
//
// "appearance=dark|light" renders the other palette. Set for this process
// only: it neither writes nor reads the machine-wide preference, so it cannot
// flip the look of plugins that happen to be open.
//
// "ui.<key>=<value>" sets panel state that has no parameter behind it. BMO
// Opto takes "ui.meter=IN|GR|OUT", which is the only way to render its VU in
// anything but OUT. Offered to every panel; refused by all of them is fatal.

#include "products/dim/Product.h"
#include "products/eq/Product.h"
#include "products/opto/Product.h"
#include "products/sat/Product.h"
#include "products/util/Product.h"
#include "products/rack/Product.h"

#include "core/ui/ModulePanel.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

namespace
{
    std::unique_ptr<juce::AudioProcessor> create (const juce::String& product)
    {
        using namespace bmo::products;

        if (product == "eq")   return createEq();
        if (product == "sat")  return createSat();
        if (product == "util") return createUtil();
        if (product == "opto") return createOpto();
        if (product == "dim")  return createDim();
        if (product == "rack") return createRack();
        return nullptr;
    }

    /** The real value `text` asks for, or nothing if it names neither a number
        nor one of the parameter's own choices.

        A choice may be given by name -- `mode=Stressed` as well as `mode=1` --
        because that is what anyone reading params.h will type. Before this,
        every non-numeric value went through getFloatValue() and came out 0.0,
        so a choice name, or a typo, silently set the parameter to its *first*
        value and rendered a panel that looked entirely plausible and was of
        the wrong thing. That cost a debugging round trip; a snapshot that
        quietly answers a different question than the one asked is worse than
        one that refuses. */
    std::optional<float> realValueFor (const bmo::ParamSet& params, int index,
                                       const juce::String& text)
    {
        if (text.containsOnly ("0123456789.-+"))
            return text.getFloatValue();

        const auto& spec = params.spec (index);

        for (int i = 0; i < spec.numChoices(); ++i)
            if (text.equalsIgnoreCase (juce::String (spec.choices[(size_t) i])))
                return (float) i;

        return {};
    }

    bool set (juce::AudioProcessor& processor, const juce::String& id, const juce::String& text)
    {
        if (auto* rack = dynamic_cast<bmo::RackProcessor*> (&processor))
        {
            if (id == "chain")
            {
                rack->clearChain();

                for (const auto& m : juce::StringArray::fromTokens (text, ",", {}))
                    if (auto* def = rack->findModule (m.trim()))
                        rack->addModule (*def);
                    else
                        return false;

                return true;
            }

            const auto dot = id.indexOfChar ('.');

            if (dot < 0)
                return false;

            auto* engine = rack->getEngineAt (id.substring (0, dot).getIntValue() - 1);

            if (engine == nullptr)
                return false;

            const auto param = id.substring (dot + 1);

            const auto i = engine->params().indexOf (param.toRawUTF8());

            if (i < 0)
                return false;

            const auto value = realValueFor (engine->params(), i, text);

            if (! value.has_value())
            {
                std::cerr << "not a value for " << param << ": " << text << '\n';
                return false;
            }

            engine->params().setReal (i, *value);
            return true;
        }

        if (auto* single = dynamic_cast<bmo::SingleModuleProcessor*> (&processor))
        {
            const auto i = single->getEngine().params().indexOf (id.toRawUTF8());

            if (i < 0)
                return false;

            const auto value = realValueFor (single->getEngine().params(), i, text);

            if (! value.has_value())
            {
                std::cerr << "not a value for " << id << ": " << text << '\n';
                return false;
            }

            single->getEngine().params().setReal (i, *value);
            return true;
        }

        return false;
    }

    /** Every ModulePanel under `root`: one for a product, one per slot for the
        rack. */
    void collectPanels (juce::Component& root, std::vector<bmo::ui::ModulePanel*>& out)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* panel = dynamic_cast<bmo::ui::ModulePanel*> (child))
                out.push_back (panel);

            collectPanels (*child, out);
        }
    }

    /** Offers `key=value` to every panel and reports whether any took it.

        Offered to all of them rather than addressed to one, because in a rack
        only the module that has the state knows the key. Accepted by none is
        an error, not a no-op: the whole reason this exists is that a render
        which quietly ignores the mode it was asked for is a picture of the
        wrong thing that nothing downstream can tell apart from the right one. */
    bool setUiState (juce::AudioProcessorEditor& editor,
                     const juce::String& key, const juce::String& value)
    {
        std::vector<bmo::ui::ModulePanel*> panels;
        collectPanels (editor, panels);

        bool accepted = false;

        for (auto* panel : panels)
            accepted |= panel->setUiState (key, value);

        return accepted;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc < 3)
    {
        std::cerr << "usage: snapshot <eq|sat|util|opto|dim|rack> out.png [width height] [param=value ...]\n";
        return 2;
    }

    auto processor = create (argv[1]);

    if (processor == nullptr)
    {
        std::cerr << "unknown product: " << argv[1] << '\n';
        return 2;
    }

    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]);

    processor->prepareToPlay (48000.0, 512);

    // Parameters first, editor second. Attachments read the current value in
    // their constructors, synchronously; setting parameters afterwards relies
    // on the message queue, which is not running here.
    int first = 3;
    int width = 0, height = 0;

    if (argc > 4 && juce::String (argv[3]).containsOnly ("0123456789")
                 && juce::String (argv[4]).containsOnly ("0123456789"))
    {
        width  = std::atoi (argv[3]);
        height = std::atoi (argv[4]);
        first  = 5;
    }

    // UI state is held back: it lives on the panel, which does not exist until
    // the editor does.
    std::vector<std::pair<juce::String, juce::String>> uiState;

    for (int i = first; i < argc; ++i)
    {
        const juce::String arg { argv[i] };
        const auto split = arg.indexOfChar ('=');

        if (split < 0)
            continue;

        const auto key = arg.substring (0, split);
        const auto value = arg.substring (split + 1);

        // Appearance is not a panel's state and not a parameter: it is the
        // whole palette. Set for this process only -- rendering the dark set
        // must not flip every plugin open on the machine, which is what
        // ui::setDarkMode would do.
        if (key == "appearance")
        {
            if (value.equalsIgnoreCase ("dark"))       bmo::ui::overrideAppearance (true);
            else if (value.equalsIgnoreCase ("light")) bmo::ui::overrideAppearance (false);
            else
            {
                std::cerr << "appearance is dark or light, got " << value << '\n';
                return 2;
            }

            continue;
        }

        if (key.startsWith ("ui."))
        {
            uiState.emplace_back (key.substring (3), value);
            continue;
        }

        if (! set (*processor, key, value))
            std::cerr << "unknown parameter: " << key << '\n';
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorAndMakeActive());

    if (editor == nullptr)
    {
        std::cerr << "no editor\n";
        return 1;
    }

    // Fatal rather than a warning, unlike an unknown parameter above. A render
    // that silently ignored the mode it was asked for would be a picture of
    // the wrong thing, and nothing downstream could tell it from the right one.
    for (const auto& [key, value] : uiState)
        if (! setUiState (*editor, key, value))
        {
            std::cerr << "no panel here takes ui." << key << "=" << value << '\n';
            processor->editorBeingDeleted (editor.get());
            editor.reset();
            return 2;
        }

    if (width > 0 && height > 0)
        editor->setSize (width, height);

    // Parts of the panel refresh on timers, so let those fire before capturing.
    for (int i = 0; i < 8; ++i)
    {
        juce::Thread::sleep (40);
        juce::Timer::callPendingTimersSynchronously();
    }

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), false, 2.0f);

    juce::PNGImageFormat png;
    std::unique_ptr<juce::FileOutputStream> stream (out.createOutputStream());

    if (stream == nullptr || ! png.writeImageToStream (image, *stream))
    {
        std::cerr << "could not write " << out.getFullPathName() << '\n';
        return 1;
    }

    std::cout << "wrote " << out.getFullPathName()
              << " (" << image.getWidth() << "x" << image.getHeight() << ")\n";

    // The editor goes before its processor.
    processor->editorBeingDeleted (editor.get());
    editor.reset();
    return 0;
}

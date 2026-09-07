// Renders a product's editor to a PNG without a display, so a layout change
// can be reviewed in a pull request rather than described in one.
//
//   snapshot <eq|sat|util|opto|rack> out.png [width height] [param=value ...]
//
// For the rack, "chain=util,eq,sat,opto" sets the modules and "N.id=value"
// sets a parameter of the module in slot N (1-based), e.g. 2.mid_gain=4.

#include "products/eq/Product.h"
#include "products/opto/Product.h"
#include "products/sat/Product.h"
#include "products/util/Product.h"
#include "products/rack/Product.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <optional>

namespace
{
    std::unique_ptr<juce::AudioProcessor> create (const juce::String& product)
    {
        using namespace bmo::products;

        if (product == "eq")   return createEq();
        if (product == "sat")  return createSat();
        if (product == "util") return createUtil();
        if (product == "opto") return createOpto();
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
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc < 3)
    {
        std::cerr << "usage: snapshot <eq|sat|util|opto|rack> out.png [width height] [param=value ...]\n";
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

    for (int i = first; i < argc; ++i)
    {
        const juce::String arg { argv[i] };
        const auto split = arg.indexOfChar ('=');

        if (split < 0)
            continue;

        if (! set (*processor, arg.substring (0, split), arg.substring (split + 1)))
            std::cerr << "unknown parameter: " << arg.substring (0, split) << '\n';
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorAndMakeActive());

    if (editor == nullptr)
    {
        std::cerr << "no editor\n";
        return 1;
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

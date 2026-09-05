// Renders a product's editor to a PNG without a display, so a layout change
// can be reviewed in a pull request rather than described in one.
//
//   snapshot <eq|sat|util|rack> out.png [width height] [param=value ...]
//
// For the rack, "chain=util,eq,sat" sets the modules and "N.id=value" sets a
// parameter of the module in slot N (1-based), e.g. 2.mid_gain=4.

#include "products/eq/Product.h"
#include "products/sat/Product.h"
#include "products/util/Product.h"
#include "products/rack/Product.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

namespace
{
    std::unique_ptr<juce::AudioProcessor> create (const juce::String& product)
    {
        using namespace bmo::products;

        if (product == "eq")   return createEq();
        if (product == "sat")  return createSat();
        if (product == "util") return createUtil();
        if (product == "rack") return createRack();
        return nullptr;
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

            if (engine->params().indexOf (param.toRawUTF8()) < 0)
                return false;

            engine->params().setReal (param.toRawUTF8(), text.getFloatValue());
            return true;
        }

        if (auto* single = dynamic_cast<bmo::SingleModuleProcessor*> (&processor))
        {
            if (single->getEngine().params().indexOf (id.toRawUTF8()) < 0)
                return false;

            single->getEngine().params().setReal (id.toRawUTF8(), text.getFloatValue());
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
        std::cerr << "usage: snapshot <eq|sat|util|rack> out.png [width height] [param=value ...]\n";
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

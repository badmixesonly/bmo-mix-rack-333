/*
    BMO Opto as a host sees it: schema, presets, level. See EqTests.cpp for
    why the schema table is written out in full.
*/

#include "TestUtil.h"
#include "modules/opto/presets/FactoryPresets.h"
#include "products/opto/Product.h"

using namespace test;
namespace P = bmo::opto;

namespace
{
    const Expected kSchema[]
    {
        { P::kCrush, "Crush",   0.0f, 100.0f,   0.0f, 0 },
        { P::kLevel, "Level", -24.0f,  24.0f,   0.0f, 0 },
        { P::kMode,  "Mode",    0.0f,   1.0f,   0.0f, 2 },
        { P::kLink,  "Link",    0.0f,   1.0f,   1.0f, 2 },
        { P::kColor, "Color",   0.0f,   1.0f,   0.0f, 2 },
    };
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    using bmo::products::createOpto;

    //== The golden schema =====================================================
    {
        auto proc = createOpto();
        checkSchema (*proc, kSchema);
        check (P::specs().size() == (size_t) P::Index::count, "the Index enum matches specs()");
    }

    //== Displayed values ======================================================
    {
        auto proc = createOpto();

        setValue (*proc, P::kCrush, 50.0f);
        check (param (*proc, P::kCrush).getCurrentValueAsText() == "50 %",
               "Crush should read '50 %', got '" + param (*proc, P::kCrush).getCurrentValueAsText() + "'");

        setValue (*proc, P::kLevel, -3.0f);
        check (param (*proc, P::kLevel).getCurrentValueAsText() == "-3.0 dB",
               "Level should read '-3.0 dB', got '" + param (*proc, P::kLevel).getCurrentValueAsText() + "'");
    }

    //== Latency: always zero, whatever Crush is set to =========================
    {
        auto proc = createOpto();
        proc->setPlayConfigDetails (2, 2, 48000.0, 512);

        setValue (*proc, P::kCrush, 0.0f);
        proc->prepareToPlay (48000.0, 512);
        check (proc->getLatencySamples() == 0, "Crush 0 reports zero latency");

        setValue (*proc, P::kCrush, 100.0f);
        proc->prepareToPlay (48000.0, 512);
        check (proc->getLatencySamples() == 0, "Crush 100 also reports zero latency");
    }

    //== State round-trip ======================================================
    {
        juce::MemoryBlock state;

        const bmo::Setting settings[] {
            { P::kCrush, 72.5f },
            { P::kLevel, -4.5f },
        };

        {
            auto proc = createOpto();
            for (const auto& s : settings) setValue (*proc, s.id, s.value);
            proc->getStateInformation (state);
        }

        auto restored = createOpto();
        restored->setStateInformation (state.getData(), (int) state.getSize());

        for (const auto& s : settings)
            checkClose (getValue (*restored, s.id), s.value, 0.01,
                        juce::String ("state round-trip of '") + s.id + "'");

        auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
        check (xml != nullptr && xml->hasTagName ("PARAMS") && xml->hasAttribute ("stateVersion"),
               "saved state is <PARAMS stateVersion=..>");
    }

    //== Presets ==============================================================
    {
        const auto sandbox = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("bmo-opto-preset-tests");
        sandbox.deleteRecursively();
        bmo::PresetManager::setDirectoryForTesting (sandbox);

        auto proc = createOpto();
        auto& presets = proc->getPresets();

        check (presets.getFactory().size() == P::factory().size(), "every factory preset is listed");
        check (presets.getCurrentName() == "Init", "the plugin starts on Init");
        check (presets.extension() == ".bmoopto", "presets are .bmoopto files");

        for (int i = 0; i < (int) P::factory().size(); ++i)
        {
            const auto& preset = P::factory()[(size_t) i];
            presets.loadFactory (i);
            check (presets.getCurrentName() == preset.name, "loading names the preset");

            for (const auto& s : preset.settings)
                checkClose (getValue (*proc, s.id), s.value, 0.01,
                            juce::String ("preset \"") + preset.name + "\" sets " + s.id);
        }

        presets.loadFactory (0);
        setValue (*proc, P::kCrush, 88.0f);
        check (presets.isEdited(), "moving a control marks the preset edited");
        check (presets.saveUser ("Round Trip"), "a user preset saves");
        presets.loadFactory (0);
        presets.loadUser ("Round Trip");
        checkClose (getValue (*proc, P::kCrush), 88.0, 0.01, "user preset restores crush");
        check (presets.deleteUser ("Round Trip"), "a user preset deletes");

        sandbox.deleteRecursively();
        bmo::PresetManager::setDirectoryForTesting ({});
    }

    //== Every preset comes out near the level it went in ======================
    // LEVEL is a hand-picked makeup figure per preset, not an auto-gain
    // detector -- this is the check that the hand-picking was close.
    {
        auto proc = createOpto();
        proc->setPlayConfigDetails (2, 2, 48000.0, 512);
        proc->prepareToPlay (48000.0, 512);

        const auto source = voice (512 * 300);
        const auto sourceDb = rmsDb (source);
        const auto& factory = proc->getPresets().getFactory();
        const bool print = std::getenv ("BMO_PRINT_PRESET_LEVELS") != nullptr;

        for (int index = 1; index < (int) factory.size(); ++index)
        {
            proc->getPresets().loadFactory (index);
            proc->reset();

            const auto outDb = outputDb (*proc, source, 512, 20);

            if (print)
                std::cout << factory[(size_t) index].name << ": " << (outDb - sourceDb) << " dB\n";

            checkClose (outDb - sourceDb, 0.0, 3.0,
                        juce::String ("preset '") + factory[(size_t) index].name + "' comes out near the level it went in");
        }
    }

    return finish ("BMO Opto");
}

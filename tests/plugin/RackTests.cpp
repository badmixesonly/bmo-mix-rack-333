/*
    BMO Mix Rack: the generic parameter grid and what it maps to.

    The rack shows a host 8 x 32 parameters, slotN_pMM, and remaps them live
    to whatever module is in the slot. The mapping is the contract: a user's
    automation lane on slot2_p03 has to mean the same thing after an update.
    It is spec order -- parameter i of a module lands on p(i+1) -- and the
    bank tables here spell that out per module so that reordering a specs()
    list turns into a failing build.

    A module may have more parameters than a slot has lanes. The grid does
    not grow for it; the ones past p32 are held off the grid, and a synthetic
    40-parameter module below checks everything still reaches them.
*/

#include "TestUtil.h"
#include "products/rack/Product.h"
#include "products/rack/Registry.h"
#include "products/eq/Product.h"
#include "products/sat/Product.h"
#include "modules/eq/params.h"
#include "modules/opto/params.h"
#include "modules/sat/params.h"
#include "modules/util/Module.h"
#include "modules/util/params.h"

using namespace test;
using bmo::RackProcessor;

namespace
{
    struct Bank
    {
        const char* moduleId;
        std::vector<const char*> ids;    // ids in slot-parameter order
    };

    const Bank kBanks[]
    {
        { "util", { "gain", "pan", "width", "phase_l", "phase_r", "mono" } },
        { "eq",   { "hf_freq", "hf_gain", "mid_freq", "mid_gain", "mid_hiq",
                    "lf_freq", "lf_gain", "hpf_freq", "lpf_freq",
                    "input_gain", "output_level", "eq_in", "phase", "mix",
                    "auto_gain", "oversampling" } },
        { "sat",  { "input_gain", "drive", "mix", "output_level",
                    "sat_in", "phase", "auto_gain", "oversampling", "tone" } },
        { "opto", { "crush", "level", "mode", "link", "color" } },
        { "dim",  { "width", "shuffle", "shuffle_freq", "detune", "detune_on",
                    "diffuse", "rate", "depth", "rotation", "asymmetry" } },
    };

    std::vector<juce::String> chainIds (RackProcessor& rack)
    {
        std::vector<juce::String> out;
        for (int s = 0; s < rack.getNumModules(); ++s)
            out.push_back (rack.getModuleAt (s)->id);
        return out;
    }

    //== A module with more parameters than a slot has lanes ==================
    // Nothing shipping has one yet. Forty plain 0..1 parameters, the last of
    // which is the module's gain, so a test can hear whether a parameter past
    // the grid reaches the DSP.
    constexpr int kWideParams = 40;

    struct WideDsp final : bmo::ModuleDsp
    {
        void prepare (double, int, int) override {}
        void reset() override {}
        void setParams (const float* v, int count) override { gain = count == kWideParams ? v[kWideParams - 1] : 0.0f; }
        int latencyForParams (const float*, int) const override { return 0; }

        void process (float* const* channels, int numChannels, int numSamples) override
        {
            for (int ch = 0; ch < numChannels; ++ch)
                juce::FloatVectorOperations::multiply (channels[ch], gain, numSamples);
        }

        float gain = 1.0f;
    };

    const bmo::ModuleDef& wideModule()
    {
        static const std::vector<std::string> ids = []
        {
            std::vector<std::string> out;
            for (int i = 1; i <= kWideParams; ++i)
                out.push_back ("p" + std::string (i < 10 ? "0" : "") + std::to_string (i));
            return out;
        }();

        static const bmo::ParamSpecs specs = []
        {
            bmo::ParamSpecs out;
            for (const auto& id : ids)
                out.push_back (bmo::ParamSpec::floatParam (id.c_str(), id.c_str(), 0.0f, 1.0f, 0.0f, 1.0f));
            return out;
        }();

        static const std::vector<bmo::FactoryPreset> presets { { "Init", {} } };

        static const bmo::ModuleDef def {
            "wide", "Wide", 1, 160, juce::Colours::grey, specs, presets,
            [] { return std::make_unique<WideDsp>(); },
            {} };

        return def;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    using bmo::products::createRack;

    //== The grid ==============================================================
    {
        auto rack = createRack();
        const auto& params = rack->getParameters();

        check (params.size() == RackProcessor::kSlots * RackProcessor::kParamsPerSlot,
               "the rack shows 256 parameters, got " + juce::String (params.size()));

        check (rack->getSlotParameter (0, 0).paramID == "slot1_p01", "the first is slot1_p01");
        check (rack->getSlotParameter (7, 31).paramID == "slot8_p32", "the last is slot8_p32");
        check (rack->getSlotParameter (1, 2).getName (64) == "Slot 2 P03", "an empty slot parameter has its generic name");
        check (rack->getNumModules() == 0, "the rack starts empty");
    }

    //== Every module in the registry fits and is pinned ======================
    {
        auto rack = createRack();
        const auto& registry = rack->getRegistry();

        check (registry.size() == 5, "the registry holds util, eq, sat, opto and dim");

        // A bank is a module's host lanes, so it stops at 32 even if the
        // module does not. Past that, its golden schema test pins the order.
        for (auto* def : registry)
        {
            const auto lanes = juce::jmin (def->specs.size(), (size_t) RackProcessor::kParamsPerSlot);

            const Bank* bank = nullptr;
            for (const auto& b : kBanks)
                if (juce::String (b.moduleId) == def->id)
                    bank = &b;

            if (bank == nullptr)
            {
                check (false, juce::String ("no bank table for module '") + def->id + "': add one here");
                continue;
            }

            check (bank->ids.size() == lanes,
                   juce::String (def->id) + " has " + juce::String ((int) lanes)
                       + " host lanes; the bank table lists " + juce::String ((int) bank->ids.size()));

            for (size_t i = 0; i < juce::jmin (bank->ids.size(), lanes); ++i)
                check (juce::String (def->specs[i].id) == bank->ids[i],
                       juce::String (def->id) + " p" + juce::String ((int) i + 1) + " should be '"
                           + bank->ids[i] + "', is '" + def->specs[i].id + "'");
        }
    }

    //== Assignment: names, ranges, text follow the module =====================
    {
        auto rack = createRack();
        rack->addModule (*rack->findModule ("eq"));

        auto& hfGain = rack->getSlotParameter (0, bmo::eq::Index::hfGain);
        check (hfGain.getName (64) == "1: HF Gain", "an assigned parameter is named after its slot and control, got '" + hfGain.getName (64) + "'");
        checkClose (hfGain.getNormalisableRange().start, -16.0, 1.0e-6, "range start follows the spec");
        checkClose (hfGain.getNormalisableRange().end,    16.0, 1.0e-6, "range end follows the spec");
        checkClose (hfGain.convertFrom0to1 (hfGain.getValue()), 0.0, 1.0e-4, "it starts at the default");
        check (hfGain.getText (hfGain.convertTo0to1 (-3.0f), 0) == "-3.0 dB", "text follows the spec");

        auto& hpf = rack->getSlotParameter (0, bmo::eq::Index::hpfFreq);
        check (hpf.getNumSteps() == 5, "a choice reports its detents");
        check (hpf.getText (hpf.convertTo0to1 (1.0f), 0) == "45 Hz", "choice text is the choice");
        checkClose (hpf.getValueForText ("70 Hz"), hpf.convertTo0to1 (2.0f), 1.0e-6, "choice text parses back");

        auto& unused = rack->getSlotParameter (0, 31);
        check (unused.getName (64) == "Slot 1 P32", "a parameter past the module's count stays generic");
        check (! rack->getSlotParameter (0, 31).isDiscrete(), "and continuous");
    }

    //== The spec's normalisation agrees with JUCE's ==========================
    // Standalone products use juce::NormalisableRange; the rack uses
    // ParamSpec::toNormalised. A preset saved in one and loaded in the other
    // must land on the same value.
    {
        auto rack = createRack();

        for (auto* def : rack->getRegistry())
        {
            for (const auto& spec : def->specs)
            {
                const juce::NormalisableRange<float> range (spec.min, spec.max, spec.step);

                for (int k = 0; k <= 10; ++k)
                {
                    const auto real = spec.min + (spec.max - spec.min) * (float) k / 10.0f;
                    const auto snapped = range.snapToLegalValue (real);

                    checkClose (spec.toNormalised (snapped), range.convertTo0to1 (snapped), 1.0e-5,
                                juce::String (def->id) + "/" + spec.id + " toNormalised at " + juce::String (snapped));
                    checkClose (spec.fromNormalised (range.convertTo0to1 (snapped)), snapped, 1.0e-3,
                                juce::String (def->id) + "/" + spec.id + " fromNormalised at " + juce::String (snapped));
                }
            }
        }
    }

    //== Chain edits ===========================================================
    {
        auto rack = createRack();
        auto& util = *rack->findModule ("util");
        auto& eq   = *rack->findModule ("eq");
        auto& sat  = *rack->findModule ("sat");

        rack->addModule (util);
        rack->addModule (eq);
        rack->addModule (sat);
        check (chainIds (*rack) == std::vector<juce::String> { "util", "eq", "sat" }, "add appends");

        // A value set in a slot follows its module when the chain moves.
        rack->getEngineAt (1)->params().setReal (bmo::eq::kMidGain, 5.0f);
        rack->moveModule (1, 0);
        check (chainIds (*rack) == std::vector<juce::String> { "eq", "util", "sat" }, "move shuffles");
        checkClose (rack->getEngineAt (0)->params().getReal (bmo::eq::kMidGain), 5.0, 0.01, "settings travel with the module");
        check (rack->getSlotParameter (0, bmo::eq::Index::midGain).getName (64) == "1: Mid Gain", "the lane is renamed for the new occupant");
        check (rack->getSlotParameter (1, 0).getName (64) == "2: Gain", "and the next slot is util's");

        rack->removeModule (1);
        check (chainIds (*rack) == std::vector<juce::String> { "eq", "sat" }, "remove closes the gap");
        check (rack->getSlotParameter (2, 0).getName (64) == "Slot 3 P01", "the vacated slot goes generic");

        rack->setModule (0, util);
        check (chainIds (*rack) == std::vector<juce::String> { "util", "sat" }, "set replaces");
        checkClose (rack->getSlotParameter (0, 0).convertFrom0to1 (rack->getSlotParameter (0, 0).getValue()), 0.0, 1.0e-4,
                    "a module arriving in a slot starts from its defaults");

        for (int i = 0; i < 10; ++i)
            rack->addModule (util);
        check (rack->getNumModules() == RackProcessor::kSlots, "the rack holds eight and no more");
        check (! rack->addModule (util), "a ninth is refused");

        rack->clearChain();
        check (rack->getNumModules() == 0, "clear empties");
    }

    //== State round-trip ======================================================
    {
        juce::MemoryBlock state;

        {
            auto rack = createRack();
            rack->addModule (*rack->findModule ("util"));
            rack->addModule (*rack->findModule ("eq"));
            rack->addModule (*rack->findModule ("sat"));
            rack->getEngineAt (0)->params().setReal (bmo::util::kWidth, 140.0f);
            rack->getEngineAt (1)->params().setReal (bmo::eq::kHpfFreq, 2.0f);
            rack->getEngineAt (2)->params().setReal (bmo::sat::kDrive, 66.0f);
            rack->getStateInformation (state);
        }

        auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
        check (xml != nullptr && xml->hasTagName ("RACK") && xml->getNumChildElements() == 3,
               "state is <RACK> with one <SLOT> per module");

        auto rack = createRack();
        rack->setStateInformation (state.getData(), (int) state.getSize());
        check (chainIds (*rack) == std::vector<juce::String> { "util", "eq", "sat" }, "order restores");
        checkClose (rack->getEngineAt (0)->params().getReal (bmo::util::kWidth), 140.0, 0.01, "util restores");
        checkClose (rack->getEngineAt (1)->params().getReal (bmo::eq::kHpfFreq),   2.0, 0.01, "eq restores");
        checkClose (rack->getEngineAt (2)->params().getReal (bmo::sat::kDrive),   66.0, 0.01, "sat restores");
        check (rack->getSlotParameter (1, bmo::eq::Index::hpfFreq).getCurrentValueAsText() == "70 Hz",
               "the host lane reads the restored value");

        // A module this build does not know is dropped and the chain closes.
        {
            juce::XmlElement future ("RACK");
            future.setAttribute ("stateVersion", 1);

            auto* a = future.createNewChildElement ("SLOT");
            a->setAttribute ("index", 0); a->setAttribute ("module", "comp"); a->setAttribute ("schema", 1);
            auto* b = future.createNewChildElement ("SLOT");
            b->setAttribute ("index", 1); b->setAttribute ("module", "util"); b->setAttribute ("schema", 1);

            juce::MemoryBlock block;
            juce::AudioProcessor::copyXmlToBinary (future, block);

            auto r = createRack();
            r->setStateInformation (block.getData(), (int) block.getSize());
            check (chainIds (*r) == std::vector<juce::String> { "util" }, "an unknown module is dropped");
        }
    }

    //== Latency is the sum ====================================================
    {
        auto rack = createRack();
        rack->setPlayConfigDetails (2, 2, 48000.0, 512);
        rack->addModule (*rack->findModule ("eq"));
        rack->addModule (*rack->findModule ("sat"));
        rack->getEngineAt (0)->params().setReal (bmo::eq::kOversampling, 1.0f);
        rack->getEngineAt (1)->params().setReal (bmo::sat::kOversampling, 1.0f);
        rack->prepareToPlay (48000.0, 512);

        const auto eqAlone = [&]
        {
            auto e = bmo::products::createEq();
            e->setPlayConfigDetails (2, 2, 48000.0, 512);
            setValue (*e, bmo::eq::kOversampling, 1.0f);
            e->prepareToPlay (48000.0, 512);
            return e->getLatencySamples();
        }();

        const auto satAlone = [&]
        {
            auto s = bmo::products::createSat();
            s->setPlayConfigDetails (2, 2, 48000.0, 512);
            setValue (*s, bmo::sat::kOversampling, 1.0f);
            s->prepareToPlay (48000.0, 512);
            return s->getLatencySamples();
        }();

        check (eqAlone > 0 && satAlone > 0, "both modules have latency at 2x");
        check (rack->getLatencySamples() == eqAlone + satAlone,
               "rack latency is the sum: " + juce::String (rack->getLatencySamples())
                   + " vs " + juce::String (eqAlone + satAlone));
    }

    //== Audio: a chain of utils multiplies ====================================
    {
        auto rack = createRack();
        rack->setPlayConfigDetails (2, 2, 48000.0, 512);
        rack->addModule (*rack->findModule ("util"));
        rack->addModule (*rack->findModule ("util"));
        rack->getEngineAt (0)->params().setReal (bmo::util::kGain, -6.0206f);
        rack->getEngineAt (1)->params().setReal (bmo::util::kGain, -6.0206f);
        rack->prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;

        for (int b = 0; b < 200; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), 1.0f, 512);
            rack->processBlock (buffer, midi);
        }

        checkClose (buffer.getSample (0, 511), 0.25, 0.005, "two -6 dB utils in series give -12 dB");

        // An empty rack is a wire.
        rack->clearChain();
        juce::FloatVectorOperations::fill (buffer.getWritePointer (0), 0.7f, 512);
        rack->processBlock (buffer, midi);
        checkClose (buffer.getSample (0, 100), 0.7, 1.0e-6, "an empty rack passes audio");
    }

    //== More than 32 parameters: the grid stays, the rest go off it ===========
    // The first 32 take the slot's lanes as any module's do. The rest are the
    // module's in every way but one -- panel, DSP, presets, saved state and
    // chain edits all reach them -- and the host never sees them.
    {
        const auto sandbox = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("bmo-rack-wide-tests");
        sandbox.deleteRecursively();
        bmo::PresetManager::setDirectoryForTesting (sandbox);

        auto& wide = wideModule();
        auto& util = bmo::util::module();
        const auto last = kWideParams - 1;

        const auto makeRack = [&]
        {
            return std::make_unique<RackProcessor> (std::vector<const bmo::ModuleDef*> { &util, &wide },
                                                    bmo::products::rackInfo(),
                                                    std::vector<bmo::RackPreset> { { "Wide", { { "wide", {} } } } });
        };

        auto rack = makeRack();
        rack->addModule (wide);
        auto& params = rack->getEngineAt (0)->params();

        check (rack->getParameters().size() == RackProcessor::kSlots * RackProcessor::kParamsPerSlot,
               "the grid does not grow for a wide module");
        check (params.size() == kWideParams, "the module sees all of its parameters, got " + juce::String (params.size()));
        check (rack->getSlotParameter (0, 31).getName (64) == "1: p32", "its 32nd takes the slot's last lane");
        check (rack->getSlotParameter (1, 0).getName (64) == "Slot 2 P01", "its 33rd does not spill into the next slot");

        auto& past = params.param (last);
        check (! rack->getParameters().contains (static_cast<juce::AudioProcessorParameter*> (&past)),
               "a parameter past the grid is not the host's");
        check (past.getParameterIndex() >= 0, "but it belongs to a processor, so a knob's gesture on it is legal");
        check (past.getName (64) == "1: p40", "and it is named like a lane");

        past.beginChangeGesture();
        params.setReal (last, 0.5f);
        past.endChangeGesture();

        rack->setPlayConfigDetails (2, 2, 48000.0, 512);
        rack->prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        juce::FloatVectorOperations::fill (buffer.getWritePointer (0), 1.0f, 512);
        juce::FloatVectorOperations::fill (buffer.getWritePointer (1), 1.0f, 512);
        rack->processBlock (buffer, midi);
        checkClose (buffer.getSample (0, 100), 0.5, 1.0e-6, "a parameter past the grid reaches the DSP");

        // Chain edits carry it with the module, and the lanes follow the slot.
        rack->addModule (util);
        rack->moveModule (0, 1);
        check (chainIds (*rack) == std::vector<juce::String> { "util", "wide" }, "a wide module moves");
        checkClose (rack->getEngineAt (1)->params().getReal (last), 0.5, 1.0e-4, "and takes its settings past the grid with it");
        check (rack->getEngineAt (1)->params().param (last).getName (64) == "2: p40", "which are renamed for their new slot");
        check (rack->getSlotParameter (1, 31).getName (64) == "2: p32", "its lanes move to the new slot");

        // Saved state carries it.
        juce::MemoryBlock state;
        rack->getStateInformation (state);

        auto restored = makeRack();
        restored->setStateInformation (state.getData(), (int) state.getSize());
        check (chainIds (*restored) == std::vector<juce::String> { "util", "wide" }, "a wide chain restores");
        checkClose (restored->getEngineAt (1)->params().getReal (last), 0.5, 1.0e-4, "with its settings past the grid");

        // So do presets, and moving one marks the preset edited.
        auto& presets = restored->getPresets();
        presets.loadFactory (0);
        check (chainIds (*restored) == std::vector<juce::String> { "wide" }, "a rack preset loads a wide module");
        checkClose (restored->getEngineAt (0)->params().getReal (last), 1.0, 1.0e-4, "at its defaults");
        check (! presets.isEdited(), "unedited");
        restored->getEngineAt (0)->params().setReal (last, 0.25f);
        check (presets.isEdited(), "a parameter past the grid marks the preset edited");

        // A module arriving in a slot starts from Init, past the grid too.
        restored->setModule (0, util);
        restored->setModule (0, wide);
        checkClose (restored->getEngineAt (0)->params().getReal (last), 1.0, 1.0e-4, "a replaced wide module starts from its defaults");

        sandbox.deleteRecursively();
        bmo::PresetManager::setDirectoryForTesting ({});
    }

    //== Rack presets define order and settings ================================
    {
        const auto sandbox = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("bmo-rack-preset-tests");
        sandbox.deleteRecursively();
        bmo::PresetManager::setDirectoryForTesting (sandbox);

        auto rack = createRack();
        auto& presets = rack->getPresets();
        const auto& factory = bmo::products::rackPresets();

        check (presets.getFactory().size() == factory.size(), "every rack preset is listed");
        check (presets.extension() == ".bmorack", "rack presets are .bmorack files");

        for (int i = 0; i < (int) factory.size(); ++i)
        {
            presets.loadFactory (i);
            const auto& preset = factory[(size_t) i];

            std::vector<juce::String> expected;
            for (const auto& e : preset.chain)
                expected.push_back (e.moduleId);

            check (chainIds (*rack) == expected, juce::String ("preset '") + preset.name + "' sets the chain");

            for (size_t s = 0; s < preset.chain.size(); ++s)
                for (const auto& setting : preset.chain[s].settings)
                    checkClose (rack->getEngineAt ((int) s)->params().getReal (setting.id), setting.value, 0.01,
                                juce::String ("preset '") + preset.name + "' sets " + setting.id);
        }

        presets.loadFactory (0);
        rack->addModule (*rack->findModule ("sat"));
        rack->addModule (*rack->findModule ("util"));
        rack->getEngineAt (1)->params().setReal (bmo::util::kPan, -25.0f);
        check (presets.isEdited(), "editing the chain marks the preset edited");
        check (presets.saveUser ("Mine"), "a rack preset saves");
        presets.loadFactory (0);
        presets.loadUser ("Mine");
        check (chainIds (*rack) == std::vector<juce::String> { "sat", "util" }, "a user rack preset restores the order");
        checkClose (rack->getEngineAt (1)->params().getReal (bmo::util::kPan), -25.0, 0.01, "and the settings");

        sandbox.deleteRecursively();
        bmo::PresetManager::setDirectoryForTesting ({});
    }

    return finish ("BMO Mix Rack");
}

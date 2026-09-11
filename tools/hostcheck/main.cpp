// Loads the built VST3 the way a host does, and checks what a host would see:
//
//   bmo-tune-hostcheck "<path to BMO Tune RT.vst3>"
//
// Not pluginval, which is not on this machine and is a download. This covers
// the part of it that a first build most needs: the binary loads, says who it
// is, carries the frozen parameter list, corrects pitch through the real
// wrapper, reports Studio latency, and brings a saved state back. Pitch is
// measured with the offline ruler (tools/common/Analysis.h), never with the
// plugin's own detector.

#include "modules/tune/dsp/TuneCore.h"
#include "modules/tune/params.h"
#include "tests/TestUtil.h"
#include "tools/common/Analysis.h"
#include "tools/common/Signals.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

using namespace bmo::tune;
using namespace bmo::tune::test;

namespace
{
    constexpr double fs = 48000.0;
    constexpr int block = 512;

    std::unique_ptr<juce::AudioPluginInstance> load (juce::VST3PluginFormat& format, const juce::String& path)
    {
        juce::OwnedArray<juce::PluginDescription> found;
        format.findAllTypesForFile (found, path);

        if (found.isEmpty())
            return nullptr;

        juce::String error;
        auto instance = format.createInstanceFromDescription (*found[0], fs, block, error);

        if (instance == nullptr)
            std::cerr << "could not instantiate: " << error << '\n';

        return instance;
    }

    juce::AudioProcessorParameter* find (juce::AudioPluginInstance& p, const juce::String& name)
    {
        for (auto* param : p.getParameters())
            if (param->getName (64) == name)
                return param;

        return nullptr;
    }

    /** Sets a parameter to a real value through its normalised range, the
        way host automation arrives. */
    void set (juce::AudioProcessorParameter& param, const bmo::ParamSpec& spec, float real)
    {
        param.setValueNotifyingHost (spec.toNormalised (real));
    }

    /** Runs `in` through the plugin in host-sized stereo blocks. */
    std::vector<float> run (juce::AudioPluginInstance& p, const std::vector<float>& in)
    {
        std::vector<float> out (in.size());
        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;

        for (size_t at = 0; at < in.size(); at += block)
        {
            const auto n = (int) std::min<size_t> (block, in.size() - at);
            buffer.setSize (2, n, false, false, true);

            for (int ch = 0; ch < 2; ++ch)
                std::copy (in.begin() + (long) at, in.begin() + (long) at + n, buffer.getWritePointer (ch));

            p.processBlock (buffer, midi);
            std::copy (buffer.getReadPointer (0), buffer.getReadPointer (0) + n, out.begin() + (long) at);
        }

        return out;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc < 2)
    {
        std::cerr << "usage: bmo-tune-hostcheck <path to BMO Tune RT.vst3>\n";
        return 2;
    }

    juce::VST3PluginFormat format;
    auto plugin = load (format, juce::String (juce::CharPointer_UTF8 (argv[1])));
    check (plugin != nullptr, "the VST3 loads and instantiates");

    if (plugin == nullptr)
        return finish ("hostcheck");

    const auto desc = plugin->getPluginDescription();
    report ("plugin: " + desc.name.toStdString() + " by " + desc.manufacturerName.toStdString(), 0);
    check (desc.name == "BMO Tune RT", "it is called BMO Tune RT");
    check (desc.manufacturerName == "LT3 Audio", "by LT3 Audio");
    check (! plugin->acceptsMidi() && ! plugin->producesMidi(), "and neither takes nor makes MIDI");

    //== The frozen list, as the host sees it ===================================
    const auto& all = specs();
    int matched = 0;

    for (int i = 0; i < Index::count; ++i)
        matched += find (*plugin, all[(size_t) i].name) != nullptr ? 1 : 0;

    report ("parameters the host sees", (double) plugin->getParameters().size());
    check (matched == Index::count, "every one of the 24 parameters is there by its name ("
                                     + std::to_string (matched) + " found)");

    auto* key = find (*plugin, "Key");
    check (key != nullptr && key->getNumSteps() == kNumKeySpellings, "Key has seventeen spellings");
    if (key != nullptr)
    {
        set (*key, all[(size_t) Index::key], 15.0f);
        check (key->getCurrentValueAsText() == "Bb", "and the host shows Bb as Bb, not A#: got "
                                                     + key->getCurrentValueAsText().toStdString());
        set (*key, all[(size_t) Index::key], 0.0f);
    }

    //== Correction through the real wrapper ====================================
    plugin->setPlayConfigDetails (2, 2, fs, block);
    plugin->prepareToPlay (fs, block);

    check (plugin->getLatencySamples() == 0, "Live, the default, reports 0 samples to the host");

    {
        // A voice 30 cents sharp of A3, into the defaults: chromatic, hard.
        const auto sharp = 220.0 * std::exp2 (30.0 / 1200.0);
        const auto in = signals::voice (signals::steady (sharp, 1.5, fs), fs).samples;
        const auto out = run (*plugin, in);

        const auto start = (size_t) (0.6 * fs), length = (size_t) (0.6 * fs);
        const auto before = analysis::measureHz (in, start, length, fs);
        const auto after = analysis::measureHz (out, start, length, fs);

        report ("in, cents from A3", analysis::cents (before, 220.0), "c");
        report ("out, cents from A3", analysis::cents (after, 220.0), "c");
        check (std::abs (analysis::cents (after, 220.0)) < 3.0, "a voice 30 cents sharp comes out on A3 through the VST3");

        bool finite = true;
        for (auto s : out)
            finite = finite && std::isfinite (s);
        check (finite, "and every sample out is finite");
    }

    //== Studio latency =========================================================
    if (auto* latency = find (*plugin, "Latency"))
    {
        // A VST3 host delivers a parameter change inside the next process
        // call, so audio has to run before the plugin knows; it then reports
        // the new latency from the message thread, and a host hears about it
        // through restartComponent. Both have to happen before it shows.
        set (*latency, all[(size_t) Index::latency], 1.0f);
        run (*plugin, std::vector<float> ((size_t) block * 4, 0.0f));
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);

        TuneParams studio;
        studio.latency = LatencyMode::studio;
        const auto expected = TuneCore::latencyFor (studio, fs);

        report ("Studio latency reported, Auto range", plugin->getLatencySamples(), "samples");
        check (plugin->getLatencySamples() == expected, "Studio reports the contract's figure: "
                                                         + std::to_string (expected) + " samples");
        set (*latency, all[(size_t) Index::latency], 0.0f);
    }

    //== A saved state comes back ===============================================
    {
        auto* engine = find (*plugin, "Engine");
        auto* retune = find (*plugin, "Retune Speed");
        check (engine != nullptr && retune != nullptr && key != nullptr, "Engine, Retune Speed and Key exist to save");

        if (engine != nullptr && retune != nullptr && key != nullptr)
        {
            set (*engine, all[(size_t) Index::engine], 1.0f);
            set (*retune, all[(size_t) Index::retune], 37.5f);
            set (*key, all[(size_t) Index::key], 15.0f);

            juce::MemoryBlock state;
            plugin->getStateInformation (state);

            auto second = load (format, juce::String (juce::CharPointer_UTF8 (argv[1])));
            check (second != nullptr, "a second instance loads");

            if (second != nullptr)
            {
                second->setStateInformation (state.getData(), (int) state.getSize());

                const auto same = [&] (const char* name)
                {
                    auto* a = find (*plugin, name);
                    auto* b = find (*second, name);
                    return a != nullptr && b != nullptr && std::abs (a->getValue() - b->getValue()) < 1.0e-4f;
                };

                check (same ("Engine") && same ("Retune Speed") && same ("Key"),
                       "Hybrid, Retune 37.5 and Key Bb survive a save and reload");
            }
        }
    }

    plugin->releaseResources();
    return finish ("hostcheck");
}

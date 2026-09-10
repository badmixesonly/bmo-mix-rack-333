/*
    BMO Dimension as a host sees it. The schema is new in 1.0, and from here on
    it is frozen the same way the others are.

    Note the order: `width` is index 0 and `detune` is index 3, which is
    reach-for-first rather than signal order. The DSP runs detune first and the
    panel lays out that way; this list is what a host's automation lane shows
    and what a saved session is keyed by, and it is the only one of the three
    that can never change.
*/

#include "TestUtil.h"
#include "modules/dim/presets/FactoryPresets.h"
#include "products/dim/Product.h"

using namespace test;
namespace P = bmo::dim;

namespace
{
    const Expected kSchema[]
    {
        { P::kWidth,       "Width",          0.0f,  200.0f, 100.0f, 0 },
        { P::kShuffle,     "Shuffle",        1.0f,    3.0f,   1.0f, 0 },
        { P::kShuffleFreq, "Shuffle Freq", 350.0f, 1400.0f, 700.0f, 0 },
        { P::kDetune,      "Detune",         0.0f,   25.0f,  10.0f, 0 },
        { P::kDetuneOn,    "Detune On",      0.0f,    1.0f,   0.0f, 2 },
        { P::kDiffuse,     "Diffuse",        0.0f,  100.0f,   0.0f, 0 },
        { P::kRate,        "Rate",           0.05f,   5.0f,   0.40f, 0 },
        { P::kDepth,       "Depth",          0.0f,  100.0f,  50.0f, 0 },
        { P::kRotation,    "Rotation",     -45.0f,   45.0f,   0.0f, 0 },
        { P::kAsymmetry,   "Asymmetry",   -100.0f,  100.0f,   0.0f, 0 },
    };
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    using bmo::products::createDim;

    {
        auto proc = createDim();
        checkSchema (*proc, kSchema);
        check (P::specs().size() == (size_t) P::Index::count, "the Index enum matches specs()");
    }

    //== Displayed values ======================================================
    {
        auto proc = createDim();

        setValue (*proc, P::kWidth, 150.0f);
        check (param (*proc, P::kWidth).getCurrentValueAsText() == "150 %",
               "Width reads as a percentage");

        setValue (*proc, P::kDiffuse, 40.0f);
        check (param (*proc, P::kDiffuse).getCurrentValueAsText() == "40 %",
               "Diffuse reads as a percentage");

        setValue (*proc, P::kAsymmetry, -25.0f);
        check (param (*proc, P::kAsymmetry).getCurrentValueAsText() == "-25 %",
               "Asymmetry reads as a signed percentage");
    }

    //== Latency ==============================================================
    // Zero in every configuration, including with the detune stage running --
    // the mid path is a wire and the detune voices only add to the side signal,
    // so nothing the host gets back is a delayed copy of what it sent. See
    // DimDsp::latencyForParams.
    {
        auto proc = createDim();
        proc->setPlayConfigDetails (2, 2, 48000.0, 512);
        proc->prepareToPlay (48000.0, 512);
        check (proc->getLatencySamples() == 0, "dimension has no latency with detune off");

        setValue (*proc, P::kDetuneOn, 1.0f);
        setValue (*proc, P::kDetune, 25.0f);
        proc->prepareToPlay (48000.0, 512);
        check (proc->getLatencySamples() == 0, "and none with it on at full depth");
    }

    //== State round-trip ======================================================
    {
        juce::MemoryBlock state;

        const bmo::Setting settings[] {
            { P::kWidth,       165.0f },
            { P::kShuffle,       2.2f },
            { P::kShuffleFreq, 520.0f },
            { P::kDetune,       17.5f },
            { P::kDetuneOn,      1.0f },
            { P::kDiffuse,      65.0f },
            { P::kRate,          1.25f },
            { P::kDepth,        80.0f },
            { P::kRotation,    -12.5f },
            { P::kAsymmetry,    35.0f },
        };

        {
            auto proc = createDim();
            for (const auto& s : settings) setValue (*proc, s.id, s.value);
            proc->getStateInformation (state);
        }

        auto restored = createDim();
        restored->setStateInformation (state.getData(), (int) state.getSize());

        for (const auto& s : settings)
            checkClose (getValue (*restored, s.id), s.value, 0.01,
                        juce::String ("state round-trip of '") + s.id + "'");
    }

    //== Factory presets ======================================================
    {
        check (! P::factory().empty(), "there are factory presets");
        check (juce::String (P::factory().front().name) == "Init", "Init is first");
        check (P::factory().front().settings.empty(), "Init is every default");

        // RATE and DEPTH have no controls, so a preset that moves them leaves
        // a value on the instance the panel cannot show or put back.
        for (const auto& preset : P::factory())
            for (const auto& s : preset.settings)
                check (juce::String (s.id) != P::kRate && juce::String (s.id) != P::kDepth,
                       juce::String ("factory preset '") + preset.name
                           + "' does not set " + s.id + ", which has no control");
    }

    return finish ("BMO Dimension");
}

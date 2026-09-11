/*
    The parameter schema, written out in full (BMO Mix Rack's
    tests/plugin/*Tests.cpp are the model).

    Nothing here is frozen until 0.1 ships -- but from then on a saved
    session references every id, its position, its range and its default,
    and this table is where a change has to be argued for. It is written now
    so that the day it freezes is a one-line decision rather than a
    reconstruction.

    Also checked: that the ModuleDsp adapter maps every value the way the
    spec list says, and that the schema fits a rack slot's 32 parameters
    (not a requirement for this product, but a free option on the future).
*/

#include "modules/tune/dsp/TuneDsp.h"
#include "tests/TestUtil.h"
#include "tools/common/Signals.h"

#include <cstring>
#include <string>

using namespace bmo::tune;
using namespace bmo::tune::test;

namespace
{
    struct Row { const char* id; bmo::ParamKind kind; float min, max, def; };

    const Row kGolden[] = {
        { "retune",        bmo::ParamKind::Float,  0.0f,   100.0f, 0.0f },
        { "key",           bmo::ParamKind::Choice, 0.0f,   11.0f,  0.0f },
        { "scale",         bmo::ParamKind::Choice, 0.0f,   9.0f,   0.0f },
        { "engine",        bmo::ParamKind::Choice, 0.0f,   1.0f,   0.0f },
        { "range",         bmo::ParamKind::Choice, 0.0f,   4.0f,   0.0f },
        { "vibrato",       bmo::ParamKind::Float,  0.0f,   150.0f, 0.0f },
        { "flex",          bmo::ParamKind::Float,  0.0f,   100.0f, 0.0f },
        { "glide",         bmo::ParamKind::Float,  0.0f,   200.0f, 0.0f },
        { "formant",       bmo::ParamKind::Bool,   0.0f,   1.0f,   1.0f },
        { "formant_shift", bmo::ParamKind::Float, -600.0f, 600.0f, 0.0f },
        { "midi_mode",     bmo::ParamKind::Choice, 0.0f,   2.0f,   0.0f },
        { "midi_latch",    bmo::ParamKind::Bool,   0.0f,   1.0f,   0.0f },
        { "midi_required", bmo::ParamKind::Bool,   0.0f,   1.0f,   0.0f },
        { "latency",       bmo::ParamKind::Choice, 0.0f,   1.0f,   0.0f },
        { "ref_a",         bmo::ParamKind::Float,  380.0f, 480.0f, 440.0f },
        { "note_c",  bmo::ParamKind::Bool, 0, 1, 1 }, { "note_cs", bmo::ParamKind::Bool, 0, 1, 1 },
        { "note_d",  bmo::ParamKind::Bool, 0, 1, 1 }, { "note_ds", bmo::ParamKind::Bool, 0, 1, 1 },
        { "note_e",  bmo::ParamKind::Bool, 0, 1, 1 }, { "note_f",  bmo::ParamKind::Bool, 0, 1, 1 },
        { "note_fs", bmo::ParamKind::Bool, 0, 1, 1 }, { "note_g",  bmo::ParamKind::Bool, 0, 1, 1 },
        { "note_gs", bmo::ParamKind::Bool, 0, 1, 1 }, { "note_a",  bmo::ParamKind::Bool, 0, 1, 1 },
        { "note_as", bmo::ParamKind::Bool, 0, 1, 1 }, { "note_b",  bmo::ParamKind::Bool, 0, 1, 1 },
    };
}

int main()
{
    const auto& s = specs();
    constexpr auto golden = sizeof (kGolden) / sizeof (kGolden[0]);

    check (s.size() == golden, "the schema has exactly the golden table's parameters");
    check ((int) s.size() == Index::count, "enum Index and specs() agree on the count");
    check (s.size() <= 32, "it fits a BMO Mix Rack slot's 32 parameters");

    for (size_t i = 0; i < std::min (s.size(), golden); ++i)
    {
        const auto& g = kGolden[i];
        const auto& p = s[i];
        const auto where = std::string (g.id) + " at position " + std::to_string (i);
        check (std::strcmp (p.id, g.id) == 0, "id " + where);
        check (p.kind == g.kind, "kind of " + where);
        check (p.min == g.min && p.max == g.max, "range of " + where);
        check (p.def == g.def, "default of " + where);
    }

    // Defaults are the hard-tune, Live, chromatic, CLASSIC instance.
    std::vector<float> v;
    for (const auto& p : s)
        v.push_back (p.def);
    const auto d = TuneParams::fromValues (v.data(), (int) v.size());
    check (d.retune == 0.0 && d.engine == Engine::classic && d.latency == LatencyMode::live
           && d.scale == ScaleType::chromatic && d.allowed == kAllNotes && d.refA == 440.0,
           "the defaults are a hard-tune, Live, chromatic CLASSIC instance");

    // The adapter: values in, through ModuleDsp, sound out.
    {
        TuneDsp dsp;
        dsp.setSampleRate (48000.0);
        dsp.setParams (v.data(), (int) v.size());
        dsp.prepare (48000.0, 256, 2);

        auto left = signals::voice (signals::steady (220.0, 0.2, 48000.0), 48000.0).samples;
        auto right = std::vector<float> (left.size(), 0.0f);
        for (size_t at = 0; at < left.size(); at += 256)
        {
            const auto n = (int) std::min<size_t> (256, left.size() - at);
            float* ch[2] { left.data() + at, right.data() + at };
            dsp.setParams (v.data(), (int) v.size());
            dsp.process (ch, 2, n);
        }

        check (left == right, "the adapter processes mono and copies it to every channel");
        check (dsp.latencyForParams (v.data(), (int) v.size()) == 0, "and reports Live's 0 by default");

        v[(size_t) Index::latency] = 1.0f;
        check (dsp.latencyForParams (v.data(), (int) v.size()) == TuneCore::latencyFor (TuneParams::fromValues (v.data(), (int) v.size()), 48000.0),
               "and Studio's figure when asked, from the values alone");
    }

    return finish ("schema");
}

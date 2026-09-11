#pragma once

#include "core/dsp/ModuleDsp.h"
#include "modules/tune/dsp/TuneCore.h"
#include "modules/tune/params.h"
#include <algorithm>
#include <array>
#include <memory>

namespace bmo::tune
{

/** BMO Tune RT behind the suite's ModuleDsp interface -- the same adapter
    shape as modules/dim/dsp/DimDsp.h in BMO Mix Rack, so a wrapper built on
    the rack's core/product drives it exactly as it drives any module.

    What ModuleDsp cannot carry, this adds: MIDI notes, queued from the host's
    buffer before process() with their sample offsets. The core is mono, so
    the first channel is processed and copied to the rest; a tuner in the
    middle of a stereo channel is a mono voice either way.
*/
class TuneDsp final : public ModuleDsp
{
public:
    void prepare (double sampleRate, int maxBlockSize, int) override
    {
        rate = sampleRate;
        core.prepare (sampleRate, maxBlockSize);
    }

    /** A host can ask for latency before prepare(), from the message thread;
        latencyForParams() needs the rate to answer, so a wrapper that knows
        it early can say so. */
    void setSampleRate (double r) noexcept { rate = r; }

    void reset() override
    {
        core.reset();
        pendingCount = 0;
    }

    void setParams (const float* v, int count) override
    {
        if (count < Index::count)
            return;

        core.setParams (TuneParams::fromValues (v, count));
    }

    /** Before process(), from the host's MIDI buffer. Real-time safe: a
        fixed queue, and a block with more than 256 note events keeps the
        first 256 (which is more than a monophonic target can use). */
    void addNote (int sampleOffset, int note, bool on) noexcept
    {
        if (pendingCount < (int) pending.size())
            pending[(size_t) pendingCount++] = { sampleOffset, note, on };
    }

    void allNotesOff (int sampleOffset) noexcept { addNote (sampleOffset, -1, false); }

    void process (float* const* channels, int numChannels, int numSamples) override
    {
        if (numChannels <= 0 || numSamples <= 0)
            return;

        std::stable_sort (pending.begin(), pending.begin() + pendingCount,
                          [] (const NoteEvent& a, const NoteEvent& b) { return a.offset < b.offset; });

        core.process (channels[0], numSamples, pending.data(), pendingCount);
        pendingCount = 0;

        for (int ch = 1; ch < numChannels; ++ch)
            std::copy (channels[0], channels[0] + numSamples, channels[ch]);
    }

    int latencyForParams (const float* v, int count) const override
    {
        return count >= Index::count ? TuneCore::latencyFor (TuneParams::fromValues (v, count), rate) : 0;
    }

    TuneCore& getCore() noexcept { return core; }

private:
    TuneCore core;
    std::array<NoteEvent, 256> pending {};
    int pendingCount = 0;
    double rate = 48000.0;
};

inline std::unique_ptr<ModuleDsp> createDsp() { return std::make_unique<TuneDsp>(); }

} // namespace bmo::tune

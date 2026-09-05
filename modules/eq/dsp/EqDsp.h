#pragma once

#include "core/dsp/ModuleDsp.h"
#include "modules/eq/dsp/DspCore.h"
#include "modules/eq/params.h"

namespace bmo::eq
{

/** Oversampling choice index to factor: Off, 2x, 4x, HQ. */
inline int oversamplingFactor (int index) noexcept
{
    constexpr int factors[] { 1, 2, 4, 8 };
    return factors[index < 0 ? 0 : (index > 3 ? 3 : index)];
}

/** The module contract's audio side: values in specs() order into DspCore. */
class EqDsp final : public ModuleDsp
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels) override
    {
        // setParams() runs before this, so the factor is the parameter's.
        core.prepare (sampleRate, maxBlockSize, numChannels, params.oversampling);
    }

    void reset() override { core.reset(); }

    void setParams (const float* v, int count) override
    {
        if (count < Index::count)
            return;

        DspCore::Params p;
        p.hfFreqIndex   = (int) v[hfFreq];
        p.midFreqIndex  = (int) v[midFreq];
        p.lfFreqIndex   = (int) v[lfFreq];
        p.hpfIndex      = (int) v[hpfFreq];
        p.lpfIndex      = (int) v[lpfFreq];
        p.hfGainDb      = v[hfGain];
        p.midGainDb     = v[midGain];
        p.lfGainDb      = v[lfGain];
        p.midHiQ        = v[midHiQ] > 0.5f;
        p.inputGainDb   = v[inputGain];
        p.outputLevelDb = v[outputLevel];
        p.mixPercent    = v[mix];
        p.eqIn          = v[eqIn] > 0.5f;
        p.phaseInvert   = v[phase] > 0.5f;
        p.autoGain      = v[autoGain] > 0.5f;
        p.oversampling  = oversamplingFactor ((int) v[oversampling]);

        params = p;
        core.setParams (p);
    }

    void process (float* const* channels, int numChannels, int numSamples) override
    {
        core.process (channels, numChannels, numSamples);
    }

    int latencyForParams (const float* v, int count) const override
    {
        return count > oversampling
                 ? Oversampler::latencyForFactor (oversamplingFactor ((int) v[oversampling]))
                 : 0;
    }

    DspCore& getCore() noexcept { return core; }

private:
    DspCore core;
    DspCore::Params params;
};

inline std::unique_ptr<ModuleDsp> createDsp() { return std::make_unique<EqDsp>(); }

} // namespace bmo::eq

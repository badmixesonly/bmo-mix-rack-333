#pragma once

#include "core/dsp/ModuleDsp.h"
#include "modules/sat/dsp/DspCore.h"
#include "modules/sat/params.h"

namespace bmo::sat
{

inline int oversamplingFactor (int index) noexcept
{
    constexpr int factors[] { 1, 2, 4, 8 };
    return factors[index < 0 ? 0 : (index > 3 ? 3 : index)];
}

class SatDsp final : public ModuleDsp
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels) override
    {
        core.prepare (sampleRate, maxBlockSize, numChannels, params.oversampling);
    }

    void reset() override { core.reset(); }

    void setParams (const float* v, int count) override
    {
        if (count < Index::count)
            return;

        DspCore::Params p;
        p.inputGainDb   = v[inputGain];
        p.driveAmount   = v[drive];
        p.toneAmount    = v[tone];
        p.mixPercent    = v[mix];
        p.outputLevelDb = v[outputLevel];
        p.saturationIn  = v[satIn] > 0.5f;
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

inline std::unique_ptr<ModuleDsp> createDsp() { return std::make_unique<SatDsp>(); }

} // namespace bmo::sat

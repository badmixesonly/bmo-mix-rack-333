#pragma once

#include "ModuleDef.h"
#include "core/dsp/Meter.h"
#include "core/state/ParamSet.h"

namespace bmo
{

/** A module running: its DSP, the parameters it reads, and its output meter.

    The standalone product owns one; the rack owns one per occupied slot.
    Parameter values are read once per block from the ParamSet, in spec
    order, into a pre-sized array, so the audio thread does no allocation and
    does not care whose parameter objects they are.
*/
class ModuleEngine
{
public:
    ModuleEngine (const ModuleDef& d, ParamSet p)
        : moduleDef (d), paramSet (std::move (p)), dsp (d.createDsp()),
          values ((size_t) d.numParams(), 0.0f)
    {
        jassert (paramSet.size() == moduleDef.numParams());
    }

    const ModuleDef& def() const noexcept    { return moduleDef; }
    ParamSet& params() noexcept              { return paramSet; }
    const ParamSet& params() const noexcept  { return paramSet; }
    const Meter& meter() const noexcept      { return outputMeter; }

    void prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        // Parameters go in first: an oversampling choice sizes the buffers.
        read();
        dsp->setParams (values.data(), (int) values.size());
        dsp->prepare (sampleRate, maxBlockSize, numChannels);
        outputMeter.reset();
    }

    void reset()
    {
        dsp->reset();
        outputMeter.reset();
    }

    void process (float* const* channels, int numChannels, int numSamples)
    {
        read();
        dsp->setParams (values.data(), (int) values.size());
        dsp->process (channels, numChannels, numSamples);
        outputMeter.measure (channels, numChannels, numSamples);
    }

    /** Latency for the parameters as they are now. Safe from any thread. */
    int latency() const
    {
        std::vector<float> now ((size_t) paramSet.size());
        paramSet.readAll (now.data());
        return dsp->latencyForParams (now.data(), (int) now.size());
    }

private:
    void read() noexcept { paramSet.readAll (values.data()); }

    const ModuleDef& moduleDef;
    ParamSet paramSet;
    std::unique_ptr<ModuleDsp> dsp;
    std::vector<float> values;
    Meter outputMeter;
};

} // namespace bmo

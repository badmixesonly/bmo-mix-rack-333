#pragma once

#include <memory>

namespace bmo
{

/** The audio side of a module, with no dependency on JUCE or on a host.

    A module's DSP is driven by an array of real-unit parameter values in the
    order of its ParamSpecs -- a dB figure, a detent index, 0 or 1 for a
    switch. The same adapter serves the standalone product, the rack, the
    tests and the measurement harness, which is the reason it takes an array
    rather than a host's parameter objects.
*/
class ModuleDsp
{
public:
    virtual ~ModuleDsp() = default;

    virtual void prepare (double sampleRate, int maxBlockSize, int numChannels) = 0;
    virtual void reset() = 0;

    /** Once per block, before process(). Cheap: stores targets only. */
    virtual void setParams (const float* values, int count) = 0;

    virtual void process (float* const* channels, int numChannels, int numSamples) = 0;

    /** Delay the module adds at the host's rate, for the current parameters.
        Computed from the values rather than from the DSP's state, so the host
        can be told about a change before the audio thread has picked it up. */
    virtual int latencyForParams (const float* values, int count) const = 0;
};

} // namespace bmo

#pragma once

#include "modules/tune/dsp/ClassicEngine.h"
#include "modules/tune/dsp/CorrectionLaw.h"
#include "modules/tune/dsp/Detector.h"
#include "modules/tune/dsp/MidiTarget.h"
#include "modules/tune/params.h"

namespace bmo::tune
{

/** The whole of BMO Tune RT's parameters in real units -- what setParams()
    takes, and what the adapter fills from the host's values by Index. */
struct TuneParams
{
    double retune = 0.0;                 ///< 0-100 knob
    int key = 0;
    ScaleType scale = ScaleType::chromatic;
    Engine engine = Engine::classic;
    Range range = Range::autoRange;
    double vibratoPercent = 0.0;
    double flexPercent = 0.0;
    double glideMs = 0.0;
    bool formant = true;
    double formantShiftCents = 0.0;
    MidiTarget::Mode midiMode = MidiTarget::Mode::off;
    bool midiLatch = false, midiRequired = false;
    LatencyMode latency = LatencyMode::live;
    double refA = 440.0;
    NoteMask allowed = kAllNotes;

    /** From an array of values in spec order, as ModuleDsp::setParams gets. */
    static TuneParams fromValues (const float* v, int count) noexcept;
};

/** One MIDI note event, stamped with its sample offset in the block. */
struct NoteEvent
{
    int offset = 0;
    int note = 60;
    bool on = true;        ///< false = off; note < 0 with on = false = all notes off
};

/** Everything --dump-analysis writes for one sample (spec T-1). */
struct AnalysisFrame
{
    long long sample = 0;
    double f0 = 0.0, clarity = 0.0;
    bool voiced = false, evaluated = false;
    double pitchIn = 0.0, target = 0.0;
    int note = -1;
    double appliedCents = 0.0, ratio = 1.0, lag = 0.0;
    bool splice = false;
};

/** BMO Tune RT's DSP, whole: float in, float out, a parameter struct, and
    MIDI notes. No framework, no host, no allocation after prepare(), and no
    dependence on how the host slices blocks -- every stage is a per-sample
    state machine, which the block-size invariance harness checks bit for
    bit (spec §8, T-1).
*/
class TuneCore
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setParams (const TuneParams&) noexcept;

    /** Mono, in place. `events` sorted by offset. */
    void process (float* samples, int numSamples, const NoteEvent* events = nullptr, int numEvents = 0) noexcept;

    /** What to report to the host for these parameters at this rate. */
    static int latencyFor (const TuneParams&, double sampleRate) noexcept;
    int latencySamples() const noexcept { return latencyFor (params, fs); }

    /** Offline tools only: called once per sample with that sample's frame.
        A plain function pointer, so installing one cannot allocate. */
    using AnalysisTap = void (*) (void* context, const AnalysisFrame&);
    void setAnalysisTap (AnalysisTap tap, void* context) noexcept { analysisTap = tap; analysisContext = context; }

    const Detector& detector() const noexcept { return det; }
    const CorrectionLaw& correction() const noexcept { return law; }
    const ClassicEngine& classic() const noexcept { return engine; }

private:
    void applyParams() noexcept;

    double fs = 48000.0;
    TuneParams params;
    bool paramsDirty = true;

    Detector det;
    CorrectionLaw law;
    MidiTarget midi;
    ClassicEngine engine;

    long long samplePosition = 0;
    AnalysisTap analysisTap = nullptr;
    void* analysisContext = nullptr;
};

} // namespace bmo::tune

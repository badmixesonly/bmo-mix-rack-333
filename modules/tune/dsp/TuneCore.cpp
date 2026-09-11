#include "modules/tune/dsp/TuneCore.h"
#include "modules/tune/dsp/Denormals.h"
#include <algorithm>
#include <cmath>

namespace bmo::tune
{

TuneParams TuneParams::fromValues (const float* v, int count) noexcept
{
    TuneParams p;
    if (count < Index::count)
        return p;

    const auto choice = [v] (int i, int n) { return std::clamp ((int) std::lround (v[i]), 0, n - 1); };
    const auto on = [v] (int i) { return v[i] >= 0.5f; };

    p.retune = v[Index::retune];
    p.key = choice (Index::key, 12);
    p.scale = (ScaleType) choice (Index::scale, (int) ScaleType::count);
    p.engine = (Engine) choice (Index::engine, 2);
    p.range = (Range) choice (Index::range, 5);
    p.vibratoPercent = v[Index::vibrato];
    p.flexPercent = v[Index::flex];
    p.glideMs = v[Index::glide];
    p.formant = on (Index::formant);
    p.formantShiftCents = v[Index::formantShift];
    p.latency = (LatencyMode) choice (Index::latency, 2);
    p.refA = v[Index::refA];

    p.allowed = 0;
    for (int n = 0; n < 12; ++n)
        if (on (Index::noteC + n))
            p.allowed = (NoteMask) (p.allowed | (1u << n));

    return p;
}

int TuneCore::latencyFor (const TuneParams& p, double sampleRate) noexcept
{
    const auto limits = limitsOf (p.range);
    return ClassicEngine::latencyFor (p.latency == LatencyMode::studio, sampleRate / limits.minHz);
}

void TuneCore::prepare (double sampleRate, int)
{
    fs = sampleRate;

    Detector::Settings ds;
    const auto limits = limitsOf (params.range);
    ds.minHz = limits.minHz;
    ds.maxHz = limits.maxHz;
    det.prepare (fs, ds);

    law.prepare (fs);
    engine.prepare (fs, fs / Detector::kCapacityMinHz);
    hybridEngine.prepare (fs, fs / Detector::kCapacityMinHz);
    switchLength = std::max (1, (int) std::lround (kSwitchMs * 0.001 * fs));
    active = params.engine;
    switching = false;

    paramsDirty = true;
    applyParams();
    reset();
}

void TuneCore::reset()
{
    det.reset();
    law.reset();
    engine.reset();
    hybridEngine.reset();
    switching = false;
    samplePosition = 0;
}

void TuneCore::setParams (const TuneParams& p) noexcept
{
    params = p;
    paramsDirty = true;
}

void TuneCore::applyParams() noexcept
{
    if (! paramsDirty)
        return;

    paramsDirty = false;

    const auto limits = limitsOf (params.range);
    Detector::Settings ds;
    ds.minHz = limits.minHz;
    ds.maxHz = limits.maxHz;
    det.setSettings (ds);

    CorrectionSettings cs;
    cs.refA = params.refA;
    cs.key = params.key;
    cs.scale = params.scale;
    cs.allowed = params.allowed;
    cs.retuneMs = CorrectionLaw::retuneMsFromKnob (params.retune);
    cs.vibratoAmount = params.vibratoPercent / 100.0;
    cs.flex = params.flexPercent / 100.0;
    cs.glideMs = params.glideMs;
    cs.glideAllowed = params.engine == Engine::hybrid;
    cs.clarityLo = ds.clarityLo;
    cs.clarityHi = ds.clarityHi;
    law.setSettings (cs);

    engine.setLatencyMode (params.latency == LatencyMode::studio, fs / limits.minHz);
    hybridEngine.setLatencyMode (params.latency == LatencyMode::studio, fs / limits.minHz);
    hybridEngine.setFormant (params.formant, params.formantShiftCents);

    if (params.engine != active)
    {
        // A switch mid-fade restarts the fade from the engine now playing.
        fadingFrom = active;
        active = params.engine;
        switching = true;
        switchPosition = 0;
    }
}

float TuneCore::runEngines (float x, double cents, double period, bool voiced, bool settled) noexcept
{
    const auto runClassic = [&] { return engine.process (x, cents, period, settled); };
    const auto runHybrid = [&] { return hybridEngine.process (x, cents, period, settled); };

    if (! switching)
    {
        if (active == Engine::hybrid)
        {
            engine.feed (x);
            return runHybrid();
        }

        hybridEngine.feed (x, period);
        return runClassic();
    }

    // Both run for the length of the fade. Equal-power: the two engines'
    // outputs are not sample-aligned (different delays), so they are
    // uncorrelated as far as a fade can tell, and a linear fade would dip.
    const auto c = runClassic();
    const auto h = runHybrid();
    const auto t = (double) (switchPosition + 1) / (double) (switchLength + 1);
    const auto toHybrid = active == Engine::hybrid;
    const auto in = std::sin (0.5 * kPi * t), out = std::cos (0.5 * kPi * t);

    if (++switchPosition >= switchLength)
        switching = false;

    return (float) (toHybrid ? out * c + in * h : out * h + in * c);
}

void TuneCore::process (float* samples, int numSamples) noexcept
{
    ScopedNoDenormals noDenormals;
    applyParams();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto x = samples[i];
        det.push (x);

        const auto& est = det.estimate();
        const auto evaluated = det.evaluatedThisSample();
        const auto cents = law.tick (est, evaluated);

        // Homing is allowed once the correction has faded all the way out on
        // an unvoiced stretch -- then the engine is carrying nothing worth
        // keeping in its delay.
        const auto settled = ! est.voiced && cents == 0.0;
        samples[i] = runEngines (x, cents, est.period, est.voiced, settled);

        if (analysisTap != nullptr)
        {
            AnalysisFrame f;
            f.sample = samplePosition;
            f.f0 = est.hz;
            f.clarity = est.clarity;
            f.voiced = est.voiced;
            f.evaluated = evaluated;
            f.pitchIn = law.state().pitchIn;
            f.target = law.state().target;
            f.note = law.state().note;
            f.appliedCents = cents;
            const auto hybridPlaying = active == Engine::hybrid;
            f.ratio = hybridPlaying ? hybridEngine.currentRatio() : engine.currentRatio();
            f.lag = hybridPlaying ? hybridEngine.currentLag() : engine.currentLag();
            f.splice = ! hybridPlaying && engine.splicedThisSample();
            analysisTap (analysisContext, f);
        }

        ++samplePosition;
    }
}

} // namespace bmo::tune

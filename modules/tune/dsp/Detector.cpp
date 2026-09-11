#include "modules/tune/dsp/Detector.h"
#include <algorithm>
#include <cmath>

namespace bmo::tune
{

namespace
{
    int nextPowerOfTwo (int n)
    {
        int size = 1;
        while (size < n)
            size <<= 1;
        return size;
    }

    double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
}

double Detector::parabolicOffset (double left, double centre, double right) noexcept
{
    // The vertex of the parabola through (-1, left), (0, centre), (1, right).
    // Same formula for a minimum of YIN's d' and a maximum of the NSDF.
    const auto denominator = left - 2.0 * centre + right;

    if (! std::isfinite (denominator) || std::abs (denominator) < 1.0e-12)
        return 0.0;

    const auto offset = 0.5 * (left - right) / denominator;

    // A vertex outside the bracket means the three points were not a peak
    // (or a trough) at all; trust the middle sample rather than extrapolate.
    return std::isfinite (offset) ? std::clamp (offset, -1.0, 1.0) : 0.0;
}

void Detector::prepare (double rate, const Settings& s)
{
    sampleRate = rate;

    // About 12 kHz for the coarse pass at every supported rate: 4 at 44.1
    // and 48, 8 at 96, 16 at 192. That keeps harmonics up to ~3.5 kHz, which
    // is plenty to bracket a period, at a sixteenth of the full-rate cost.
    decimation = std::max (1, (int) std::lround (rate / 12000.0));
    coarseRate = rate / decimation;
    antiAlias.prepare (rate, std::min (3000.0, 0.3 * coarseRate));

    // Allocate for the widest range any setting can ask for, so a
    // pitch-range change on the audio thread only moves the active window.
    const auto capMinCoarse = std::max (2, (int) std::floor (coarseRate / kCapacityMaxHz) - 1);
    const auto capMaxCoarse = (int) std::ceil (coarseRate / kCapacityMinHz) + 2;

    // Every lag is recomputed from scratch at least every quarter second.
    kernel.prepare (capMinCoarse, capMaxCoarse, (int) std::lround (coarseRate * 0.25));
    coarseNsdf.assign ((size_t) capMaxCoarse + 2, 0.0);

    // The fine pass reads a one-period window at a lag of up to a period and
    // a bit: 2 x the longest lag plus the refinement bracket, with room.
    const auto capMaxFull = (int) std::ceil (rate / kCapacityMinHz) + decimation + 2;
    fullRing.assign ((size_t) nextPowerOfTwo (2 * capMaxFull + 4 * decimation + 16), 0.0f);
    fullMask = (int) fullRing.size() - 1;
    cumSum.assign (fullRing.size(), 0.0);
    cumSq.assign (fullRing.size(), 0.0);

    baseHop = std::max (1, (int) std::lround (rate * 0.0005));
    zcrCoeff = std::exp (-1.0 / (rate * 0.010));

    configured = false;
    setSettings (s);
    reset();
}

void Detector::setSettings (const Settings& s) noexcept
{
    const auto minHz = std::clamp (std::min (s.minHz, s.maxHz * 0.5), kCapacityMinHz, kCapacityMaxHz * 0.5);
    const auto maxHz = std::clamp (std::max (s.maxHz, minHz * 2.0), minHz * 2.0, kCapacityMaxHz);
    const auto rangeChanged = ! configured || minHz != settings.minHz || maxHz != settings.maxHz;

    settings = s;
    settings.minHz = minHz;
    settings.maxHz = maxHz;

    if (! rangeChanged)
        return;

    configured = true;

    // Below the range floor, never at it: 0.6 of the lowest note keeps the
    // fundamental within a dB or so while taking out DC (which reads as
    // "periodic at every lag") and the rumble and kick bleed that would
    // otherwise sit under a period and fail the whole-cycle mean test.
    highpass.makeHighpass (sampleRate, 0.6 * minHz, 0.70710678);

    // One lag of slack at the top of the range and two at the bottom, so the
    // extreme notes still have a neighbour on each side to interpolate with.
    kernel.setLagRange (std::max (2, (int) std::floor (coarseRate / maxHz) - 1),
                        (int) std::ceil (coarseRate / minHz) + 2);

    minFullLag = std::max (2, (int) std::floor (sampleRate / maxHz) - decimation - 2);
    maxFullLag = (int) std::ceil (sampleRate / minHz) + decimation + 2;

    // A new range is a new question; nothing held from the old one applies.
    current = {};
    heldPeriod = candidatePeriod = 0.0;
    voicedRun = unvoicedRun = 0;
}

void Detector::reset()
{
    highpass.reset();
    antiAlias.reset();
    kernel.reset();
    std::fill (fullRing.begin(), fullRing.end(), 0.0f);
    std::fill (cumSum.begin(), cumSum.end(), 0.0);
    std::fill (cumSq.begin(), cumSq.end(), 0.0);
    runningSum = runningSq = 0.0;
    rebaseCountdown = kRebaseInterval;
    std::fill (coarseNsdf.begin(), coarseNsdf.end(), 0.0);
    fullWrite = 0;
    decimationPhase = 0;
    zcrState = 0.0;
    energyState = 0.0;
    previousSign = 1.0f;
    hopCountdown = baseHop;
    lastHop = baseHop;
    samplesSeen = 0;
    lastOnsetAt = -1'000'000;
    voicedRun = unvoicedRun = 0;
    heldPeriod = 0.0;
    current = {};
    evaluated = false;
}

void Detector::push (float input) noexcept
{
    const auto x = std::isfinite (input) ? (double) input : 0.0;
    const auto y = highpass.process (x);

    runningSum += y;
    runningSq += y * y;
    fullRing[(size_t) fullWrite] = (float) y;
    cumSum[(size_t) fullWrite] = runningSum;
    cumSq[(size_t) fullWrite] = runningSq;
    fullWrite = (fullWrite + 1) & fullMask;

    // The running sums grow without bound (the squared one linearly), and a
    // window's sum is the difference of two of them, so their magnitude is
    // the error floor. Rebasing every 2^20 samples keeps that floor below
    // 1e-9 of full scale forever, at the cost of one pass over the ring
    // every 20 seconds. Only differences are ever read, so it is invisible.
    if (--rebaseCountdown <= 0)
    {
        rebaseCountdown = kRebaseInterval;
        const auto baseSum = runningSum, baseSq = runningSq;
        for (size_t i = 0; i < cumSum.size(); ++i)
        {
            cumSum[i] -= baseSum;
            cumSq[i] -= baseSq;
        }
        runningSum = runningSq = 0.0;
    }

    // Zero crossings and energy, both as one-pole averages over ~10 ms, so
    // neither needs a window of its own.
    const auto sign = y >= 0.0 ? 1.0f : -1.0f;
    const auto crossed = sign != previousSign ? 1.0 : 0.0;
    previousSign = sign;
    zcrState = zcrCoeff * zcrState + (1.0 - zcrCoeff) * crossed;
    energyState = zcrCoeff * energyState + (1.0 - zcrCoeff) * y * y;

    const auto filtered = antiAlias.process (y);

    if (++decimationPhase >= decimation)
    {
        decimationPhase = 0;
        kernel.push ((float) filtered);
    }

    ++samplesSeen;
    current.onset = false;
    evaluated = false;

    if (--hopCountdown <= 0)
    {
        evaluate();
        evaluated = true;

        // A quarter period between evaluations once locked: the fine pass
        // costs a period's worth of multiplies, so this keeps its cost per
        // second flat across the range instead of 7x higher at 80 Hz.
        lastHop = current.voiced && heldPeriod > 0.0
                    ? std::max (baseHop, (int) std::lround (heldPeriod * 0.25))
                    : baseHop;
        hopCountdown = lastHop;
    }
}

void Detector::evaluate() noexcept
{
    double coarseLag = 0.0, period = 0.0, clarity = 0.0;

    const auto found = coarseSearch (coarseLag)
                    && refine (coarseLag * decimation, period, clarity);

    if (! found)
        clarity = 0.0;

    const auto rms = std::sqrt (std::max (0.0, energyState));
    const auto gate = dbToGain (settings.gateDb);
    const auto zcrHz = zcrState * sampleRate;

    // A candidate has to hold still before it can open voicing. In the first
    // period of a low note the window has not yet seen a whole cycle, and a
    // short lag across a smooth arc of the waveform can read 0.9 clarity --
    // but that false period moves from one evaluation to the next as the arc
    // does, while a real one stays put. Measured on a 147 Hz sine: the false
    // candidates ran 1143, 1655, 1043, 750 Hz on consecutive hops.
    const auto stable = found && candidatePeriod > 0.0
                     && std::abs (std::log2 (period / candidatePeriod)) < settings.stabilityCents / 1200.0;

    const auto frameVoiced = stable && clarity > settings.clarityHi
                          && rms > gate && zcrHz < settings.zcrMaxHz;
    const auto frameUnvoiced = ! found || clarity < settings.clarityLo || rms < gate * 0.5;

    candidatePeriod = found ? period : 0.0;
    current.candidate = candidatePeriod;
    updateVoicing (frameVoiced, frameUnvoiced);

    current.clarity = clarity;
    current.rms = rms;

    // On unvoiced, the period freezes where it was (spec §3.4): the engines
    // keep free-running on it, and the correction amount is what fades.
    if (found && current.voiced && clarity >= settings.clarityLo)
    {
        heldPeriod = period;
        current.period = period;
        current.hz = sampleRate / period;
    }
}

void Detector::updateVoicing (bool frameVoiced, bool frameUnvoiced) noexcept
{
    // Counters in samples, sized in periods: open after a quarter period of
    // agreement, close after two. Opening fast is the time-to-lock budget --
    // and a frame only counts toward it if its candidate is stable against
    // the previous one, so "one frame" is already two agreeing estimates.
    // Half a period was the first choice and cost 880 Hz a whole extra hop
    // (3.1 periods to lock rather than 2.6) for no robustness the stability
    // test was not already providing. Closing slow is what lets a vibrato's
    // trough or a soft consonant inside a word pass without dropping the note.
    const auto periodForCounts = candidatePeriod > 0.0 ? candidatePeriod
                               : (heldPeriod > 0.0 ? heldPeriod : sampleRate / 200.0);
    const auto attack  = std::max (1, (int) std::lround (0.25 * periodForCounts));
    const auto release = std::max (2 * baseHop, (int) std::lround (2.0 * periodForCounts));

    if (! current.voiced)
    {
        voicedRun = frameVoiced ? voicedRun + lastHop : 0;

        if (voicedRun >= attack)
        {
            current.voiced = true;
            current.onset = true;
            lastOnsetAt = samplesSeen;
            unvoicedRun = 0;

            // A transition is when the running sums are most likely to be
            // carrying a louder passage's residue; refresh them all now
            // rather than wait for the schedule (spec §3.1).
            kernel.recomputeAll();
        }
    }
    else
    {
        unvoicedRun = frameUnvoiced ? unvoicedRun + lastHop : 0;

        if (unvoicedRun >= release)
        {
            current.voiced = false;
            voicedRun = 0;
            kernel.recomputeAll();
        }
    }
}

bool Detector::coarseSearch (double& coarseLag) noexcept
{
    const auto lo = kernel.getMinLag();
    const auto hi = kernel.getMaxLag();

    // A relative floor: a lag whose two-period energy is this far below the
    // signal's own level is reading silence at the start of a note, and 0/0
    // there is not a correlation.
    const auto floor = std::max (1.0e-12, 1.0e-9 * kernel.energyAt (hi));

    for (int L = lo; L <= hi; ++L)
        coarseNsdf[(size_t) L] = kernel.nsdfAt (L, floor);

    // McLeod's key maxima: the highest point of each positive lobe between
    // zero crossings. The lobe the scan starts inside only counts if its
    // maximum is interior -- at short lags a lowpassed signal's NSDF starts
    // near 1 and falls, and that falling edge is not a period.
    struct Candidate { int lag; double value; };
    Candidate candidates[64];
    int count = 0;

    bool inLobe = false;
    int lobeLag = lo;
    double lobeValue = -2.0;

    const auto closeLobe = [&] () -> bool
    {
        inLobe = false;
        if (lobeLag <= lo || lobeLag >= hi || count >= 64)
            return false;

        if (! spansWholeCycles (lobeLag * decimation))
            return false;

        candidates[count++] = { lobeLag, lobeValue };
        return lobeValue >= settings.earlyExit;
    };

    for (int L = lo; L <= hi; ++L)
    {
        const auto v = coarseNsdf[(size_t) L];

        if (v > 0.0)
        {
            if (! inLobe)
            {
                inLobe = true;
                lobeValue = v;
                lobeLag = L;
            }
            else if (v > lobeValue)
            {
                lobeValue = v;
                lobeLag = L;
            }
        }
        else if (inLobe && closeLobe())
        {
            break;   // early exit: the patent's (E - 2H) <= eps E, eps = 1 - earlyExit
        }
    }

    if (inLobe)
        closeLobe();

    if (count == 0)
        return false;

    // Temporal continuity (guard 3): weight each candidate by its distance in
    // octaves from the held period, except just after an onset, where a real
    // leap must not be argued out of.
    const auto graceSamples = (std::int64_t) (settings.onsetGraceMs * 0.001 * sampleRate);
    const auto useContinuity = current.voiced && heldPeriod > 0.0
                            && samplesSeen - lastOnsetAt > graceSamples;

    double best = -1.0e9;
    double scores[64];

    for (int i = 0; i < count; ++i)
    {
        auto score = candidates[i].value;

        if (useContinuity)
            score -= settings.continuityWeight
                   * std::abs (std::log2 (candidates[i].lag * decimation / heldPeriod));

        scores[i] = score;
        best = std::max (best, score);
    }

    // Guard 2, McLeod's peak-fraction rule: the smallest lag that is nearly
    // as good as the best is the fundamental; the best itself is often a
    // multiple of it.
    int chosen = 0;
    const auto threshold = settings.peakFraction * best;

    for (int i = 0; i < count; ++i)
    {
        if (scores[i] >= threshold)
        {
            chosen = i;
            break;
        }
    }

    int tau = candidates[chosen].lag;

    // Guard 1, sub-multiples: if a half or a third of the chosen lag matches
    // nearly as well by the patent's own measure, the chosen one was a
    // multiple of the period.
    for (int k = 2; k <= 3; ++k)
    {
        const auto centre = (int) std::lround ((double) tau / k);
        if (centre - 1 <= lo)
            break;

        int subLag = centre;
        for (int L = centre - 1; L <= centre + 1; ++L)
            if (coarseNsdf[(size_t) L] > coarseNsdf[(size_t) subLag])
                subLag = L;

        const auto dTau = 1.0 - coarseNsdf[(size_t) tau];
        const auto dSub = 1.0 - coarseNsdf[(size_t) subLag];

        if (subLag > lo && subLag < hi && dSub < settings.subMultipleRatio * dTau)
            tau = subLag;
    }

    coarseLag = tau + parabolicOffset (coarseNsdf[(size_t) tau - 1],
                                       coarseNsdf[(size_t) tau],
                                       coarseNsdf[(size_t) tau + 1]);
    return true;
}

bool Detector::spansWholeCycles (int lag) const noexcept
{
    // The coarse pass's window is the lag itself -- that is the patent's form
    // and what buys the fast lock -- so at a short lag it sees only a
    // fragment of a slow wave, and a fragment near a crest correlates with
    // the fragment before it well enough to beat the real period under the
    // smallest-lag rule. Measured on a 110 Hz sine: a 1230 Hz lobe at 0.98
    // every half cycle.
    //
    // What tells them apart is cheap. The analysis signal is highpassed, so a
    // real period of it averages to zero, in both halves of the two-period
    // span; a fragment of a crest does not. The means come off running sums
    // in O(1), with no window of their own.
    const auto meanSquaredOverPower = [this] (int newest, int length)
    {
        const auto s = cumSumAt (newest) - cumSumAt (newest + length);
        const auto q = cumSqAt (newest) - cumSqAt (newest + length);

        if (q <= 1.0e-20)
            return 1.0;

        return (s * s) / ((double) length * q);
    };

    const auto limit = settings.cycleMeanLimit * settings.cycleMeanLimit;

    return meanSquaredOverPower (0, lag) < limit
        && meanSquaredOverPower (lag, lag) < limit;
}

double Detector::fullNsdf (int lag, int window) const noexcept
{
    double r = 0.0, m = 0.0;

    for (int j = 0; j < window; ++j)
    {
        const double a = fullAt (j);
        const double b = fullAt (j + lag);
        r += a * b;
        m += a * a + b * b;
    }

    return m > 1.0e-20 ? 2.0 * r / m : 0.0;
}

bool Detector::refine (double centre, double& period, double& clarity) noexcept
{
    // The coarse lag is good to half a coarse sample, which is half the
    // decimation factor at the full rate; search a little wider than that.
    const auto reach = decimation + 1;
    const auto window = std::clamp ((int) std::lround (centre), 8, maxFullLag);

    const auto lo = std::max (minFullLag, (int) std::floor (centre) - reach);
    const auto hi = std::min (maxFullLag, (int) std::ceil (centre) + reach);

    if (hi <= lo)
        return false;

    int bestLag = lo;
    double bestValue = -2.0;

    for (int L = lo; L <= hi; ++L)
    {
        const auto v = fullNsdf (L, window);
        if (v > bestValue)
        {
            bestValue = v;
            bestLag = L;
        }
    }

    const auto left  = fullNsdf (bestLag - 1, window);
    const auto right = fullNsdf (bestLag + 1, window);
    const auto offset = parabolicOffset (left, bestValue, right);

    period = bestLag + offset;
    clarity = std::clamp (bestValue - 0.25 * (left - right) * offset, 0.0, 1.0);
    return period > 1.0;
}

} // namespace bmo::tune

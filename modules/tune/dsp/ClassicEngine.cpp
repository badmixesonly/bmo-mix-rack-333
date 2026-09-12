#include "modules/tune/dsp/ClassicEngine.h"
#include "modules/tune/dsp/Detector.h"
#include <algorithm>
#include <cmath>

namespace bmo::tune
{

void ClassicEngine::prepare (double sampleRate, double longestPeriod)
{
    fs = sampleRate;
    rest = contract::liveRestSamples (fs);

    // The deepest read: the rest lag, a period of swing above it and one more
    // of margin for a splice in flight, plus a fade's drift and the kernel.
    const auto deepest = rest + 2.0 * longestPeriod + 2 * Sinc::kTaps + 64;
    int size = 1;
    while (size < (int) deepest)
        size <<= 1;

    ring.assign ((size_t) size, 0.0f);
    mask = size - 1;

    kernels.build (fs);

    reset();
}

void ClassicEngine::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    write = 0;
    lag = rest;
    ratio = 1.0;
    lastPeriod = 0.0;
    fading = false;
    spliced = false;
    splices = 0;
}

double ClassicEngine::read (const Sinc& t, double lagBehindNewest) const noexcept
{
    const auto newest = write - 1;
    return t.read (ring.data(), mask, (double) newest - lagBehindNewest);
}

double ClassicEngine::bestJump (double fromLag, double nominal, double T, double lo, double hi) const noexcept
{
    // The jump the period says, corrected by what the waveform says.
    //
    // Jumping by a whole DETECTED period assumes the detector is right. A few
    // per cent out on a scoop and the landing is a few per cent of a cycle
    // out of phase; an octave out and it is half a cycle out, which is the
    // worst place it could be. Either way the crossfade joins two points that
    // are not the same point and the waveform steps -- and that step is what
    // Frosty hears as a pop (testing-notes/tune-blind-2026-09-12.md: all
    // seven of his timestamps were splices, and the audible ones were the
    // ones that landed badly).
    //
    // So the period only proposes. The recent history either side is matched
    // by normalised cross-correlation over half a period around it, and the
    // jump goes where the waveform actually repeats. Costs nothing audible
    // when the period is right -- the correlation peaks at the nominal -- and
    // rescues it when it is not. This is WSOLA's similarity search, doing
    // here what it does there.
    const auto newest = write - 1;
    const auto window = std::clamp ((int) std::lround (0.5 * T), 32, 256);

    // HALF a period either way, and not more. Half resolves phase -- the
    // nearest point where this waveform repeats -- and cannot reach the next
    // cycle's peak, so on periodic material there is exactly one candidate
    // and the search either confirms the nominal or corrects it.
    //
    // A full period was tried and is much worse than doing nothing. Every
    // multiple of the period correlates equally well, so with that much reach
    // the shimmer decides which cycle wins; the jump then lands a whole
    // period from where the window wanted it, immediately leaves the window
    // again, and splices once more. On a correctly detected period it took
    // the landing error from 3e-10 to 0.13, the splice count from 8 to 15,
    // and put the 40-cent sine's THD+N at +46 dB.
    //
    // The cost of the narrow bound is that a period an OCTAVE out cannot be
    // rescued here: that needs a move of a whole detected period, which is
    // exactly the reach that thrashes. It belongs to the detector, not the
    // engine. See CoreTests.
    const auto reach = std::max (4, (int) std::lround (0.5 * T));

    // Never propose a read that needs samples not yet written, or one outside
    // the window the jump exists to get back into.
    const auto lowest = std::max (lo, (double) kFloor);
    auto from = (int) std::lround (std::max (nominal - reach, lowest - fromLag));
    auto to = (int) std::lround (std::min (nominal + reach, hi - fromLag));

    if (from > to)
        return nominal;

    const auto sample = [&] (double atLag, int j)
    {
        return (double) ring[(size_t) ((newest - (int) std::lround (atLag) - j) & mask)];
    };

    double outEnergy = 0.0;
    for (int j = 0; j < window; ++j)
    {
        const auto v = sample (fromLag, j);
        outEnergy += v * v;
    }

    if (outEnergy <= 1.0e-20)
        return nominal;

    const auto scale = 1.0 / std::sqrt (outEnergy);

    // Properly normalised, so it can be compared against a threshold and not
    // only against itself.
    const auto score = [&] (int d)
    {
        double dot = 0.0, energy = 0.0;
        for (int j = 0; j < window; ++j)
        {
            const auto a = sample (fromLag, j);
            const auto b = sample (fromLag + d, j);
            dot += a * b;
            energy += b * b;
        }
        return energy > 1.0e-20 ? scale * dot / std::sqrt (energy) : -1.0e30;
    };

    // When the period is right the nominal already lands on the waveform, and
    // that is the ordinary case -- so test it first and leave immediately if
    // it holds. Searching regardless cost 0.7 points of CPU at 48 kHz / 128,
    // taking the whole plugin from 0.93 % to 1.65 % and through its 1.5 %
    // gate, to re-derive an answer it already had. One window pass instead of
    // a few dozen.
    //
    // The threshold has to be severe. A landing error e and a normalised
    // correlation r are the same number twice: e = sqrt (2 (1 - r)). So 0.98
    // -- which sounds like a lot of agreement -- waves through a landing
    // error of 0.2, and it did: the 3 %-out cases went straight back to 0.32.
    // 0.9995 is a landing error of 0.03.
    const auto atNominal = (int) std::lround (nominal);
    if (atNominal >= from && atNominal <= to && score (atNominal) > 0.9995)
        return nominal;

    // Coarse then fine, so a long period does not cost a long search.
    auto bestD = from;
    auto best = -1.0e30;
    for (int d = from; d <= to; d += 8)
    {
        const auto s = score (d);
        if (s > best) { best = s; bestD = d; }
    }

    for (int d = std::max (from, bestD - 8); d <= std::min (to, bestD + 8); ++d)
    {
        const auto s = score (d);
        if (s > best) { best = s; bestD = d; }
    }

    // Sub-sample, by the parabola through the peak and its neighbours.
    //
    // Not a refinement -- a correctness fix. The search only scores whole
    // samples, and a period is not a whole number of them: at 440 Hz it is
    // 109.09, so an integer answer is up to half a sample, about 1.6 degrees,
    // out of phase. On a voice that is nothing; on the 40-cent sine of
    // CoreTests' T-3 check it put THD+N over -60 dB, where jumping by the
    // fractional k x T had been exact.
    if (bestD > from && bestD < to)
    {
        const auto l = score (bestD - 1), c = best, r = score (bestD + 1);
        return (double) bestD + Detector::parabolicOffset (l, c, r);
    }

    return (double) bestD;
}

void ClassicEngine::startFade (double newLag, int length, bool equalPower) noexcept
{
    // A fade already running is abandoned where it is: its outgoing read is
    // replaced by the incoming one, which is what the listener was hearing
    // most of by then anyway.
    fadeLag = lag;
    lag = newLag;
    fading = true;
    fadeEqualPower = equalPower;
    fadeLength = std::max (1, length);
    fadePosition = 0;
    fadeDiffAcc = fadeRefAcc = 0.0;
}

float ClassicEngine::process (float input, double cents, double period, bool settled) noexcept
{
    ring[(size_t) write] = std::isfinite (input) ? input : 0.0f;
    write = (write + 1) & mask;
    spliced = false;

    ratio = std::exp2 (std::clamp (cents, -1200.0, 1200.0) / 1200.0);
    if (period > 1.0)
        lastPeriod = period;

    const auto step = 1.0 - ratio;
    lag += step;
    if (fading)
        fadeLag = std::max ((double) kFloor, fadeLag + step);

    if (lastPeriod > 1.0 && ! fading)
    {
        const auto T = lastPeriod;
        const auto fade = std::max (16, (int) std::lround (0.5 * T));

        // The window. Its floor is raised by however far the outgoing read
        // will drift during a fade at the ratio in force, so a splice never
        // asks the kernel for a sample that has not arrived.
        const auto lo = kFloor + fade * std::max (0.0, ratio - 1.0);
        const auto hi = (double) rest + T;

        if (lag < lo || lag > hi)
        {
            // Whole periods only: the waveform one cycle away is the same
            // waveform. Usually one; more after a leap to a much higher note
            // shrank the window underneath the pointer.
            const auto k = lag < lo ? std::ceil ((lo - lag) / T) : -std::ceil ((lag - hi) / T);
            startFade (lag + bestJump (lag, k * T, T, lo, hi), fade, false);
            fadeWasSplice = true;
            spliced = true;
            ++splices;
        }
        else if (settled && std::abs (lag - rest) > 0.5)
        {
            // Home, over 5 ms of uncorrelated material.
            startFade (rest, std::max (16, (int) (0.005 * fs)), true);
        }
    }
    else if (lastPeriod <= 1.0 && settled && ! fading && std::abs (lag - rest) > 0.5)
    {
        startFade (rest, std::max (16, (int) (0.005 * fs)), true);
    }

    lag = std::max ((double) kFloor, lag);

    const auto& table = kernels.forRatio (ratio);
    auto y = read (table, lag);

    mismatch = 0.0;

    if (fading)
    {
        const auto outgoing = read (table, fadeLag);
        const auto incoming = y;
        const auto t = (double) (fadePosition + 1) / (double) (fadeLength + 1);

        // How badly this splice lands. The two reads are meant to be one
        // cycle apart on the same waveform, so across the fade they should
        // very nearly agree; whatever they do not agree by is the step the
        // listener hears. Accumulated over the fade and reported once, as an
        // RMS difference against the material's own RMS: about 0 for a jump
        // that lands in phase, and of order 1.4 for one that lands anywhere.
        //
        // This is the measurement the splice COUNT was standing in for and
        // should not have been. Frosty timestamped the pops he hears on
        // Failure (2026-09-12): seven of seven were splices, but only seven
        // of thirty-eight splices were audible at all, so a count cannot
        // tell a bad one from a silent one and driving it down did not drive
        // the pops down. testing-notes/tune-blind-2026-09-12.md.
        fadeDiffAcc += (incoming - outgoing) * (incoming - outgoing);
        fadeRefAcc += outgoing * outgoing + incoming * incoming;

        if (fadeEqualPower)
        {
            const auto a = std::cos (0.5 * kPi * t), b = std::sin (0.5 * kPi * t);
            y = a * outgoing + b * y;
        }
        else
        {
            y = (1.0 - t) * outgoing + t * y;
        }

        if (++fadePosition >= fadeLength)
        {
            fading = false;

            if (fadeWasSplice)
            {
                mismatch = fadeRefAcc > 1.0e-20 ? std::sqrt (2.0 * fadeDiffAcc / fadeRefAcc) : 0.0;
                worstMismatch = std::max (worstMismatch, mismatch);
                fadeWasSplice = false;
            }
        }
    }

    return std::isfinite (y) ? (float) y : 0.0f;
}

} // namespace bmo::tune

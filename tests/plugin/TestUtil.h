#pragma once

#include "core/product/SingleModuleProcessor.h"
#include <juce_events/juce_events.h>
#include <iostream>

/** The little that every plugin test needs: a failure counter, comparisons,
    parameter access by ID, and two test signals. */
namespace test
{

inline int failures = 0;

inline void check (bool condition, const juce::String& what)
{
    if (! condition)
    {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

inline void checkClose (double actual, double expected, double tolerance, const juce::String& what)
{
    if (! (std::abs (actual - expected) <= tolerance))
    {
        std::cerr << "FAIL: " << what << " -- expected " << expected
                  << " +/- " << tolerance << ", got " << actual << '\n';
        ++failures;
    }
}

inline juce::RangedAudioParameter& param (bmo::SingleModuleProcessor& p, const char* id)
{
    auto* rp = p.getEngine().params().find (id);
    jassert (rp != nullptr);
    return *rp;
}

inline void setValue (bmo::SingleModuleProcessor& p, const char* id, float real)
{
    p.getEngine().params().setReal (id, real);
}

inline float getValue (bmo::SingleModuleProcessor& p, const char* id)
{
    return p.getEngine().params().getReal (id);
}

/** The schema, written out. Order is part of it. */
struct Expected
{
    const char* id;
    const char* name;
    float min, max, defaultValue;
    int steps;              // 0 for a continuous parameter
};

/** Compares a processor's parameter list, in order, against a golden table. */
template <size_t N>
void checkSchema (juce::AudioProcessor& proc, const Expected (&schema)[N])
{
    const auto& parameters = proc.getParameters();

    check (parameters.size() == (int) N,
           "expected " + juce::String ((int) N) + " parameters, got " + juce::String (parameters.size()));

    for (int i = 0; i < juce::jmin (parameters.size(), (int) N); ++i)
    {
        const auto& expected = schema[(size_t) i];
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (parameters[i]);

        if (p == nullptr)
        {
            check (false, "parameter " + juce::String (i) + " is not a ranged parameter");
            continue;
        }

        const juce::String where { juce::String ("parameter ") + juce::String (i) + " (" + expected.id + ")" };

        check (p->paramID == expected.id,
               where + " should have ID '" + expected.id + "', has '" + p->paramID + "'");
        check (p->getName (64) == expected.name,
               where + " should be named '" + juce::String (expected.name) + "', is '" + p->getName (64) + "'");

        const auto range = p->getNormalisableRange();
        checkClose (range.start, expected.min, 1.0e-6, where + " range start");
        checkClose (range.end,   expected.max, 1.0e-6, where + " range end");
        checkClose (p->convertFrom0to1 (p->getDefaultValue()), expected.defaultValue, 1.0e-4, where + " default");
        check (p->getNumSteps() == (expected.steps == 0
                                      ? juce::AudioProcessor::getDefaultNumParameterSteps()
                                      : expected.steps),
               where + " step count: " + juce::String (p->getNumSteps()));
    }
}

/** Pink noise at -18 dBFS RMS: broadband, and at the level a mix actually
    sits at rather than at the top of the scale. */
inline std::vector<float> pink (int samples)
{
    juce::Random random (0x50f7);
    std::vector<float> source ((size_t) samples);

    double b0 = 0.0, b1 = 0.0, b2 = 0.0;

    for (auto& v : source)
    {
        const auto white = (double) random.nextFloat() * 2.0 - 1.0;
        b0 = 0.99765 * b0 + white * 0.0990460;
        b1 = 0.96300 * b1 + white * 0.2965164;
        b2 = 0.57000 * b2 + white * 1.0526913;
        v = (float) ((b0 + b1 + b2 + white * 0.1848) * 0.11);
    }

    double sum = 0.0;
    for (auto v : source) sum += (double) v * v;

    const auto scale = (float) (juce::Decibels::decibelsToGain (-18.0) / std::sqrt (sum / (double) source.size()));
    for (auto& v : source) v *= scale;

    return source;
}

/** A sung-note stand-in: a harmonic series at 75 Hz with a phrase envelope,
    at -18 dBFS RMS. */
inline std::vector<float> voice (int samples)
{
    std::vector<float> out ((size_t) samples);
    double sumSquares = 0.0;

    for (int i = 0; i < samples; ++i)
    {
        const auto t = (double) i / 48000.0;
        const auto beat = std::fmod (t, 0.55);
        const auto envelope = (std::fmod (t, 3.0) < 1.6 ? 1.0 : 0.0)
                            * (beat < 0.01 ? beat / 0.01 : std::exp (-(beat - 0.01) * 7.0));

        double sum = 0.0;

        for (int h = 1; h <= 120; ++h)
            sum += std::pow ((double) h, -1.4)
                     * std::sin (2.0 * juce::MathConstants<double>::twoPi * 75.0 * h * t);

        out[(size_t) i] = (float) (envelope * sum);
        sumSquares += (double) out[(size_t) i] * out[(size_t) i];
    }

    const auto rms = std::sqrt (sumSquares / (double) samples);
    const auto gain = rms > 0.0 ? juce::Decibels::decibelsToGain (-18.0f) / (float) rms : 1.0f;

    for (auto& v : out)
        v *= gain;

    return out;
}

inline double rmsDb (const std::vector<float>& v, size_t from = 0)
{
    double sum = 0.0;
    for (size_t i = from; i < v.size(); ++i) sum += (double) v[i] * v[i];
    return juce::Decibels::gainToDecibels (std::sqrt (sum / (double) (v.size() - from)));
}

/** Runs a source through a processor in blocks and returns the RMS of the
    output in dB, skipping the first `skip` blocks while smoothers settle. */
inline double outputDb (juce::AudioProcessor& proc, const std::vector<float>& source,
                        int block, int skip)
{
    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;

    const auto blocks = (int) source.size() / block;
    double sum = 0.0;
    int counted = 0;

    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            buffer.copyFrom (ch, 0, source.data() + (size_t) (b * block), block);

        proc.processBlock (buffer, midi);

        if (b < skip)
            continue;

        const auto* read = buffer.getReadPointer (0);

        for (int i = 0; i < block; ++i)
            sum += (double) read[i] * read[i];

        counted += block;
    }

    return juce::Decibels::gainToDecibels (std::sqrt (sum / (double) counted));
}

inline int finish (const char* suite)
{
    if (failures == 0)
        std::cout << "All " << suite << " tests passed.\n";

    return failures == 0 ? 0 : 1;
}

} // namespace test

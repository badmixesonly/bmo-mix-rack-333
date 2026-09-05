#pragma once

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace bmo
{

/** What a parameter is, stated without JUCE so the DSP-only builds and the
    rack's slot remapping can both read it.

    A module's parameter list is a permanent, append-only schema. Once a user
    saves a session, automation lanes and stored state are keyed by the ID and
    by the position in the list, and stored automation is normalised -- so
    widening a range silently rescales every automation point ever written.
    Nothing errors, nothing warns. The golden schema test in tests/plugin is
    where any change to one of these has to be argued for.
*/
enum class ParamKind { Float, Bool, Choice };

/** How a value reads on the panel and in the host. */
enum class ParamFormat
{
    Plain,       ///< the number
    Decibels,    ///< "+3.0 dB"
    Percent,     ///< "40 %"
    Pan          ///< "L 50", "C", "R 50"
};

struct ParamSpec
{
    const char*  id   = "";
    const char*  name = "";
    ParamKind    kind = ParamKind::Float;
    float        min = 0.0f, max = 1.0f, def = 0.0f, step = 0.0f;
    ParamFormat  format = ParamFormat::Plain;
    std::vector<const char*> choices;

    static ParamSpec floatParam (const char* id, const char* name, float min, float max,
                                 float step, float def, ParamFormat format = ParamFormat::Plain)
    {
        ParamSpec s;
        s.id = id; s.name = name; s.kind = ParamKind::Float;
        s.min = min; s.max = max; s.step = step; s.def = def; s.format = format;
        return s;
    }

    static ParamSpec boolParam (const char* id, const char* name, bool def)
    {
        ParamSpec s;
        s.id = id; s.name = name; s.kind = ParamKind::Bool;
        s.min = 0.0f; s.max = 1.0f; s.step = 1.0f; s.def = def ? 1.0f : 0.0f;
        return s;
    }

    static ParamSpec choiceParam (const char* id, const char* name,
                                  std::vector<const char*> choices, int def)
    {
        ParamSpec s;
        s.id = id; s.name = name; s.kind = ParamKind::Choice;
        s.choices = std::move (choices);
        s.min = 0.0f; s.max = (float) (s.choices.size() - 1); s.step = 1.0f; s.def = (float) def;
        return s;
    }

    int numChoices() const noexcept { return (int) choices.size(); }

    /** The unit the host shows beside the value. */
    const char* label() const noexcept
    {
        switch (format)
        {
            case ParamFormat::Decibels: return "dB";
            case ParamFormat::Percent:  return "%";
            case ParamFormat::Pan:      return "";
            case ParamFormat::Plain:    return "";
        }

        return "";
    }

    //== Normalised <-> real, without JUCE ====================================
    // The rack's generic slot parameters hold normalised values and convert
    // through these, so they have to agree with what juce::NormalisableRange
    // does for the same range: linear, snapped to the step.

    float clampReal (float v) const noexcept
    {
        v = v < min ? min : (v > max ? max : v);

        if (step > 0.0f)
            v = min + step * std::round ((v - min) / step);

        return v < min ? min : (v > max ? max : v);
    }

    float toNormalised (float real) const noexcept
    {
        if (max <= min)
            return 0.0f;

        const auto n = (clampReal (real) - min) / (max - min);
        return n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n);
    }

    float fromNormalised (float n) const noexcept
    {
        n = n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n);
        return clampReal (min + n * (max - min));
    }

    /** The value as the panel and the host print it. */
    std::string text (float real) const
    {
        char buf[64];

        switch (kind)
        {
            case ParamKind::Bool:
                return real >= 0.5f ? "On" : "Off";

            case ParamKind::Choice:
            {
                const auto i = (int) std::lround (real);
                return i >= 0 && i < numChoices() ? choices[(size_t) i] : "";
            }

            case ParamKind::Float:
                break;
        }

        switch (format)
        {
            case ParamFormat::Decibels:
                std::snprintf (buf, sizeof (buf), "%s%.1f dB", real > 0.0f ? "+" : "", (double) real);
                return buf;

            case ParamFormat::Percent:
                std::snprintf (buf, sizeof (buf), "%d %%", (int) std::lround (real));
                return buf;

            case ParamFormat::Pan:
            {
                const auto i = (int) std::lround (real);
                if (i == 0) return "C";
                std::snprintf (buf, sizeof (buf), "%s %d", i < 0 ? "L" : "R", std::abs (i));
                return buf;
            }

            case ParamFormat::Plain:
                break;
        }

        std::snprintf (buf, sizeof (buf), "%g", (double) real);
        return buf;
    }
};

using ParamSpecs = std::vector<ParamSpec>;

/** Position of an ID in a spec list, or -1. */
inline int indexOfParam (const ParamSpecs& specs, const char* id) noexcept
{
    for (size_t i = 0; i < specs.size(); ++i)
        if (std::string (specs[i].id) == id)
            return (int) i;

    return -1;
}

/** One parameter setting inside a factory preset, in real units: a detent
    index for a choice, 0 or 1 for a switch. */
struct Setting
{
    const char* id;
    float value;
};

struct FactoryPreset
{
    const char* name;
    std::vector<Setting> settings;
};

} // namespace bmo

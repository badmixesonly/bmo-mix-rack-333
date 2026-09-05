#pragma once

#include "ParamSpec.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace bmo
{

/** JUCE parameter objects from a spec list, for a standalone product.

    Types, names, ranges, steps and text functions have to come out exactly as
    the original plugins made them, because the golden schema tests compare
    against what those plugins reported and a host session references it.
*/
inline juce::AudioProcessorValueTreeState::ParameterLayout makeLayout (const ParamSpecs& specs,
                                                                       int versionHint)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& s : specs)
    {
        const juce::ParameterID id { s.id, versionHint };

        switch (s.kind)
        {
            case ParamKind::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (id, s.name, s.def >= 0.5f));
                break;

            case ParamKind::Choice:
            {
                juce::StringArray choices;
                for (auto* c : s.choices)
                    choices.add (c);

                layout.add (std::make_unique<juce::AudioParameterChoice> (id, s.name, choices, (int) s.def));
                break;
            }

            case ParamKind::Float:
            {
                juce::AudioParameterFloatAttributes attr;
                const auto spec = s;   // copied into the lambda: specs() lists are static

                if (s.format != ParamFormat::Plain)
                    attr = attr.withLabel (s.label())
                               .withStringFromValueFunction ([spec] (float v, int)
                               {
                                   return juce::String (spec.text (v));
                               });

                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    id, s.name, juce::NormalisableRange<float> { s.min, s.max, s.step }, s.def, attr));
                break;
            }
        }
    }

    return layout;
}

/** The parameters an APVTS holds, in spec order. */
inline std::vector<juce::RangedAudioParameter*> collect (juce::AudioProcessorValueTreeState& apvts,
                                                         const ParamSpecs& specs)
{
    std::vector<juce::RangedAudioParameter*> out;

    for (const auto& s : specs)
    {
        auto* p = apvts.getParameter (s.id);
        jassert (p != nullptr);
        out.push_back (p);
    }

    return out;
}

} // namespace bmo

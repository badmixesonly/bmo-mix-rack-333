#include "Tokens.h"
#include "core/state/PresetManager.h"
#include <array>
#include <cmath>

namespace bmo::ui
{

namespace
{
    struct Entry { const char* name; juce::Colour Tokens::* member; };

    const Entry kEntries[]
    {
        { "plate",      &Tokens::plate },
        { "plateEdge",  &Tokens::plateEdge },
        { "well",       &Tokens::well },
        { "hairline",   &Tokens::hairline },
        { "outline",    &Tokens::outline },
        { "text1",      &Tokens::text1 },
        { "text2",      &Tokens::text2 },
        { "knobFace",   &Tokens::knobFace },
        { "knobEdge",   &Tokens::knobEdge },
        { "pointer",    &Tokens::pointer },
        { "ringFace",   &Tokens::ringFace },
        { "meterInk",   &Tokens::meterInk },
        { "track",      &Tokens::track },
        { "trackFill",  &Tokens::trackFill },
        { "switchOff",  &Tokens::switchOff },
        { "switchOn",   &Tokens::switchOn },
        { "switchAlt",  &Tokens::switchAlt },
        { "accent",     &Tokens::accent },
        { "neutral",    &Tokens::neutral },
        { "meterLow",   &Tokens::meterLow },
        { "meterHigh",  &Tokens::meterHigh },
        { "meterClip",  &Tokens::meterClip },
        { "meterGr",    &Tokens::meterGr },
    };

    Tokens current;
    juce::Time lastModified;
    bool loadedOnce = false;

    bool parseColour (const juce::var& v, juce::Colour& out)
    {
        if (! v.isString())
            return false;

        auto s = v.toString().trim();

        if (s.startsWithChar ('#'))
            s = s.substring (1);

        if (s.length() == 6)
            s = "ff" + s;

        if (s.length() != 8)
            return false;

        out = juce::Colour ((juce::uint32) s.getHexValue64());
        return true;
    }
}

const Tokens& tokens() noexcept { return current; }

//== Derived colours ==========================================================

namespace
{
    float toLinear (float c) noexcept
    {
        return c <= 0.03928f ? c / 12.92f : std::pow ((c + 0.055f) / 1.055f, 2.4f);
    }

    float relativeLuminance (juce::Colour c) noexcept
    {
        return 0.2126f * toLinear (c.getFloatRed())
             + 0.7152f * toLinear (c.getFloatGreen())
             + 0.0722f * toLinear (c.getFloatBlue());
    }

    /** Both derivations walk the accent toward one end of the range in fixed
        steps and stop at the first value that clears. Called from paint, and
        each step costs three pow(), so the answers are memoised: there are
        only ever a handful of live (colour, ground) pairs -- one per module
        per ground -- and a linear scan of eight is cheaper than one step of
        the search it replaces. Painting is all on the message thread, so no
        locking is needed here. */
    struct Memo
    {
        juce::uint32 from = 0, ground = 0;
        float ratio = 0.0f;
        juce::Colour result;
        bool valid = false;
    };

    juce::Colour searchCached (juce::Colour from, juce::Colour ground, float minRatio,
                               juce::Colour target)
    {
        static std::array<Memo, 8> memo {};
        static size_t next = 0;

        const auto fromKey = from.getARGB();
        const auto groundKey = ground.getARGB();

        for (const auto& m : memo)
            if (m.valid && m.from == fromKey && m.ground == groundKey && m.ratio == minRatio)
                return m.result;

        auto result = from;

        // 50 steps of 2% is enough to reach either end of the range; stopping
        // at the first pass keeps as much of the original colour as the ratio
        // allows, so the hue stays recognisable.
        for (int i = 0; i <= 50; ++i)
        {
            result = from.interpolatedWith (target, (float) i * 0.02f);

            if (contrastRatio (result, ground) >= minRatio)
                break;
        }

        memo[next] = { fromKey, groundKey, minRatio, result, true };
        next = (next + 1) % memo.size();

        return result;
    }
}

float contrastRatio (juce::Colour a, juce::Colour b) noexcept
{
    const auto la = relativeLuminance (a);
    const auto lb = relativeLuminance (b);

    return (juce::jmax (la, lb) + 0.05f) / (juce::jmin (la, lb) + 0.05f);
}

juce::Colour accentTextOn (juce::Colour accent, juce::Colour ground, float minRatio) noexcept
{
    // Away from the ground: darker on a pale plate, lighter on a dark one.
    const auto target = relativeLuminance (ground) > 0.18f ? juce::Colours::black
                                                           : juce::Colours::white;

    return searchCached (accent, ground, minRatio, target);
}

juce::Colour onAccentOf (juce::Colour fill, float minRatio) noexcept
{
    // The ink sits on the fill, so the fill is its own ground. Every accent
    // in the suite is light enough that darkening is the direction that
    // works; a future dark accent gets white out of the same test.
    const auto target = relativeLuminance (fill) > 0.18f ? juce::Colours::black
                                                         : juce::Colours::white;

    return searchCached (fill, fill, minRatio, target);
}

juce::File themeDirectory() { return suitePresetRoot().getChildFile ("Themes"); }
juce::File themeFile()      { return themeDirectory().getChildFile ("Default.json"); }

juce::StringArray tokenNames()
{
    juce::StringArray names;
    for (const auto& e : kEntries)
        names.add (e.name);
    return names;
}

Tokens tokensFromJson (const juce::var& object)
{
    Tokens t;

    if (auto* obj = object.getDynamicObject())
        for (const auto& e : kEntries)
            if (obj->hasProperty (e.name))
                parseColour (obj->getProperty (e.name), t.*(e.member));

    return t;
}

bool pollTheme()
{
    const auto file = themeFile();

    if (! file.existsAsFile())
    {
        if (! loadedOnce)
            return false;

        // The file went away: back to the built-in set.
        loadedOnce = false;
        lastModified = {};
        current = Tokens {};
        return true;
    }

    const auto modified = file.getLastModificationTime();

    if (loadedOnce && modified == lastModified)
        return false;

    lastModified = modified;
    loadedOnce = true;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());
    current = tokensFromJson (parsed);
    return true;
}

} // namespace bmo::ui

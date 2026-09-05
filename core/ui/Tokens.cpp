#include "Tokens.h"
#include "core/state/PresetManager.h"

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
        { "track",      &Tokens::track },
        { "trackFill",  &Tokens::trackFill },
        { "switchOff",  &Tokens::switchOff },
        { "switchOn",   &Tokens::switchOn },
        { "switchAlt",  &Tokens::switchAlt },
        { "accent",     &Tokens::accent },
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

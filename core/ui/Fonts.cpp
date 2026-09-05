#include "Fonts.h"
#include <BinaryData.h>

namespace bmo::ui
{

namespace
{
    /** Parsed once each. These are called from paint, and building a typeface
        reads and decodes the whole file. */
    juce::Typeface::Ptr minervaBlack()
    {
        static const juce::Typeface::Ptr face = juce::Typeface::createSystemTypefaceFor (
            BinaryData::TGMinervaBlackBlack_otf, (size_t) BinaryData::TGMinervaBlackBlack_otfSize);
        return face;
    }

    juce::Typeface::Ptr blender()
    {
        static const juce::Typeface::Ptr face = juce::Typeface::createSystemTypefaceFor (
            BinaryData::TGBlender_otf, (size_t) BinaryData::TGBlender_otfSize);
        return face;
    }

    juce::Font build (const juce::Typeface::Ptr& face, float height, float tracking)
    {
        if (face == nullptr)   // should not happen: the file is in the binary
            return juce::Font (juce::FontOptions {}.withHeight (height));

        return juce::Font (juce::FontOptions (face).withHeight (height))
                   .withExtraKerningFactor (tracking);
    }
}

juce::Font labelFont (float height, bool)
{
    // Minerva Black is a single weight, so the bold flag has nothing to select
    // and is kept only so callers do not all have to change.
    return build (minervaBlack(), height, 0.08f);
}

juce::Font captionFont (float height)
{
    return build (blender(), height, 0.04f);
}

void drawLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                juce::Justification justification, const juce::Font& font, juce::Colour fill)
{
    if (text.isEmpty())
        return;

    g.setColour (fill);
    g.setFont (font);
    g.drawText (text, area, justification, false);
}

} // namespace bmo::ui

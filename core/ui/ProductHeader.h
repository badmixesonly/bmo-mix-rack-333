#pragma once

#include "LookAndFeel.h"

namespace bmo::ui
{

/** The strip across the top of a standalone product: the name on the left,
    the maker on the right. Drawn, not laid out; nothing on it is clickable. */
class ProductHeader final : public juce::Component
{
public:
    static constexpr int kHeight = 28;

    explicit ProductHeader (juce::String productName, juce::Colour accent)
        : name (std::move (productName)), tint (accent) {}

    void paint (juce::Graphics& g) override
    {
        const auto& t = tokens();
        auto area = getLocalBounds();

        g.fillAll (t.plateEdge);

        // The module's colour, as a thin bar along the top edge.
        g.setColour (tint);
        g.fillRect (area.removeFromTop (3));

        drawLabel (g, name, area.reduced (10, 0).toFloat(), juce::Justification::centredLeft,
                   labelFont (12.5f, true), t.text1);
        drawLabel (g, "LT3a", area.reduced (10, 0).toFloat(), juce::Justification::centredRight,
                   labelFont (10.0f, true), t.text2);
    }

private:
    juce::String name;
    juce::Colour tint;
};

} // namespace bmo::ui

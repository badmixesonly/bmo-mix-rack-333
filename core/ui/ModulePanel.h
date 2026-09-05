#pragma once

#include "Controls.h"
#include "core/state/ParamSet.h"

namespace bmo
{
struct ModuleDef;
}

namespace bmo::ui
{

/** What a module's panel is built against. The same whether the module is
    running as its own plugin or sitting in a rack slot. */
struct ModuleContext
{
    ParamSet& params;
    const ModuleDef& def;
    std::function<float()> peak;    ///< output level, linear, both channels
    std::function<float()> rms;
};

/** Base of every module panel: a fixed-size faceplate of the module's design
    width and the common content height, below whatever header the product
    puts over it.

    A panel paints its own plate, so a rack of them reads as one surface with
    the section rules lining up across modules.
*/
class ModulePanel : public juce::Component
{
public:
    /** Header 28 + preset row 24 + this = the common 740. */
    static constexpr int kContentHeight = 688;
    static constexpr int kPad     = 10;
    static constexpr int kRuleRow = 16;

    explicit ModulePanel (ModuleContext ctx) : context (std::move (ctx)) {}

    const ModuleContext& getContext() const noexcept { return context; }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (tokens().plate);
        paintPanel (g);
    }

protected:
    /** Rules and legends go here; the plate is already down. */
    virtual void paintPanel (juce::Graphics&) {}

    /** A hairline through the middle of a row, inset by the padding. */
    void drawRule (juce::Graphics& g, juce::Rectangle<int> row) const
    {
        g.setColour (tokens().hairline);
        g.fillRect (juce::Rectangle<float> ((float) kPad, (float) row.getCentreY(),
                                            (float) (getWidth() - kPad * 2), Tokens::hairlineWeight));
    }

    /** A section name drawn on a rule, in the module's accent. */
    void drawRuleLegend (juce::Graphics& g, juce::Rectangle<int> row,
                         const juce::String& text, juce::Colour colour) const
    {
        drawRule (g, row);

        const auto font = labelFont (11.0f, true);
        const auto width = juce::GlyphArrangement::getStringWidth (font, text) + 12.0f;
        const auto box = juce::Rectangle<float> (width, (float) row.getHeight())
                             .withCentre (row.toFloat().getCentre());

        g.setColour (tokens().plate);
        g.fillRect (box);
        drawLabel (g, text, box, juce::Justification::centred, font, colour);
    }

    ModuleContext context;
};

} // namespace bmo::ui

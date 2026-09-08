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

    // Optional: empty for every module that does not ask for one. Only a
    // dynamics module's panel (BMO Opto's DynamicsMeter) reads these today --
    // see core/product/ModuleEngine.h. Check before calling: a
    // default-constructed std::function throws if invoked.
    std::function<float()> inputPeak;         ///< input level, linear, before the DSP
    std::function<float()> inputRms;
    std::function<float()> gainReductionDb;   ///< always >= 0
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

    /** Section legend type size. */
    static constexpr float kLegendSize = 13.0f;
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

    /** A section name drawn on a rule, in the module's own colour.

        Pass the raw accent: it is stepped to a legible contrast against the
        plate here, so a panel never has to know how. As the raw accent these
        measured 1.72-2.00:1 -- the panel's navigation was the second least
        readable thing on it. */
    void drawRuleLegend (juce::Graphics& g, juce::Rectangle<int> row,
                         const juce::String& text, juce::Colour accent) const
    {
        // The module's accent as it stands, not stepped for contrast: a section
        // legend is set in exactly the colour that module's bypass switch
        // lights up in, so the panel's navigation and its bypass agree.
        //
        // Frosty's call, and it costs contrast in both appearances: 1.72-2.00:1
        // on the pale plate against the 4.57-4.69:1 accentInk was giving, and
        // 5.87:1 on the dark one against 9.07:1. The size below is part of the
        // same decision -- a legend set in a colour this pale has to be big
        // enough to survive it.
        //
        // This is the one place a module's colour is used as ink without going
        // through accentInk, so a section legend and a knob caption are
        // deliberately no longer the same colour.
        drawRule (g, row);

        const auto font = labelFont (kLegendSize, true);
        const auto width = juce::GlyphArrangement::getStringWidth (font, text) + 14.0f;
        const auto box = juce::Rectangle<float> (width, (float) row.getHeight())
                             .withCentre (row.toFloat().getCentre());

        g.setColour (tokens().plate);
        g.fillRect (box);
        drawLabel (g, text, box, juce::Justification::centred, font, accent);
    }

    ModuleContext context;
};

} // namespace bmo::ui

#pragma once

#include "core/state/ParamSet.h"
#include "modules/deq/dsp/Design.h"
#include "modules/deq/dsp/DspCore.h"
#include "modules/deq/params.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace bmo::deq
{

/** The curve: every band's node over the response the audio path actually has.

    The line is the bands' own matched-Z designs multiplied together -- the
    serial topology, for a centred source -- evaluated from the same code the
    DSP runs, so what is drawn cannot drift from what is heard. (A side band
    does nothing to a centred source and so does not bend it; its node is still
    drawn.) The selected band's own contribution is filled under it, and a
    dynamic band's range is a whisker from where its gain sits to where the
    dynamics can take it.

    Drag a node for frequency and gain, wheel for Q, double-click the curve to
    switch on the next free band there. Every move is a host gesture, so it
    automates and undoes like a knob.
*/
class ResponseView final : public juce::Component,
                           private juce::Timer
{
public:
    ResponseView (ParamSet& params, juce::Colour accent,
                  std::function<int()> selectedBand, std::function<void (int)> selectBand);
    ~ResponseView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    /** Mockup C's curve: fewer grid lines and labels, small bare nodes, no gain
        scale. The compact panel's curve is 300 px wide. */
    void setCompact (bool c)
    {
        if (c != compact)
        {
            compact = c;
            rebuildPaths();   // the axis row's height, so the plot, follows it
        }
        repaint();
    }

    /** The plotting area, inside the axis labels. */
    juce::Rectangle<float> plot() const;

    static constexpr float kSpanDb = 24.0f;   ///< the gain range, top to centre

    /** How far the component runs past the well on each side, so a node at
        either end of the range is drawn whole. Place it with
        `wellBounds.expanded (kOverhang, 0)`. */
    static constexpr int kOverhang = 8;

private:
    struct Band
    {
        bool on = false, dynamic = false;
        Shape shape = Shape::bell;
        Placement placement = Placement::stereo;
        double hz = 1000.0, q = 0.71, gainDb = 0.0, rangeDb = 0.0;
    };

    void timerCallback() override;
    bool readBands();
    void rebuildPaths();

    float xFor (double hz) const;
    double hzFor (float x) const;
    float yFor (double db) const;
    double dbFor (float y) const;
    juce::Point<float> nodeFor (const Band&) const;
    int bandAt (juce::Point<float>) const;

    float value (int band, Control c) const { return params.getReal (indexOf (band, c)); }

    ParamSet& params;
    juce::Colour accent;
    std::function<int()> selected;
    std::function<void (int)> select;

    std::array<Band, kBands> bands;
    juce::Path curve, selectedFill;
    DesignGrid grid = DesignGrid::make (kDisplayRate);
    bool compact = false;

    int dragging = -1;
    int lastSelected = -2;   ///< per instance: two DEQs in a session each have their own
    static constexpr double kDisplayRate = 48000.0;
};

} // namespace bmo::deq

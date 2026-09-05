#pragma once

#include "Tokens.h"
#include "Fonts.h"

namespace bmo::ui
{

/** A rotary control drawn as a potentiometer: a face and a pointer.

    No value readout of any kind -- a plus one side, a minus the other where
    there is something to subtract, and nothing else. Numbers make people mix
    with their eyes, hunting a tidy figure and flinching from a large move.
*/
class Knob : public juce::Slider
{
public:
    enum class Style
    {
        utility,    ///< pale blue face, dotted track. Input, output, gain.
        character,  ///< the module's accent: a band's gain, drive, tone.
        filter,     ///< blue face, legend around it, no track. The cut filters.
        ring        ///< the white selector ring a band's gain sits inside.
    };

    Knob() : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox) {}

    void setStyle (Style s) noexcept   { style = s; }
    Style getStyle() const noexcept    { return style; }

    /** The module's colour, for the character style. */
    void setAccent (juce::Colour c) noexcept { accent = c; }
    juce::Colour getAccent() const noexcept  { return accent; }

    void setDetents (int count) noexcept { detents = count; }
    int  getDetents() const noexcept     { return detents; }

    void setFaceScale (float s) noexcept { faceScale = s; }
    float getFaceScale() const noexcept  { return faceScale; }

    /** Where the dotted track sits, in pixels from the centre. Zero means
        "just outside my own face". A band's gain has to clear the ring drawn
        around it, and the face inside knows nothing about the ring's size. */
    void setTrackRadius (float r) noexcept { trackRadius = r; }
    float getTrackRadius() const noexcept  { return trackRadius; }

    /** The inner control of a concentric pair claims only its own circle, so
        the ring around it stays grabbable right up to the corners. */
    void setCircularHitTest (bool b) noexcept { circularHit = b; }

    bool hitTest (int x, int y) override
    {
        if (! circularHit)
            return juce::Slider::hitTest (x, y);

        const auto centre = getLocalBounds().toFloat().getCentre();
        const auto radius = (float) juce::jmin (getWidth(), getHeight()) * 0.5f * faceScale;

        return juce::Point<float> ((float) x, (float) y).getDistanceFrom (centre) <= radius + 4.0f;
    }

private:
    Style style = Style::utility;
    juce::Colour accent { tokens().accent };
    bool  circularHit = false;
    int   detents = 0;
    float faceScale = 1.0f;
    float trackRadius = 0.0f;
};

//==============================================================================
class BmoLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    BmoLookAndFeel() { refreshColours(); }

    /** Re-reads the tokens. Call after a theme change. */
    void refreshColours();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawHighlighted, bool shouldDrawDown) override;

    juce::Font getLabelFont (juce::Label&) override;

    /** The preset strip is built from TextButtons, and it is panel text like
        any other. The popup list of preset names is not: a heavy display face
        makes a list of names slower to read, so that keeps the system font. */
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    /** A ring of dots, used for the track around a gain control. */
    static void drawDottedArc (juce::Graphics&, juce::Point<float> centre, float radius,
                               float startAngle, float endAngle, juce::Colour, float dotSize);

    /** The slashed O of a polarity switch. */
    static const juce::String& phaseGlyph();
};

} // namespace bmo::ui

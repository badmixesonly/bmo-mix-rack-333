#include "LookAndFeel.h"

namespace bmo::ui
{

const juce::String& BmoLookAndFeel::phaseGlyph()
{
    static const juce::String glyph (juce::CharPointer_UTF8 ("\xc3\x98"));
    return glyph;
}

void BmoLookAndFeel::refreshColours()
{
    const auto& t = tokens();

    setColour (juce::ResizableWindow::backgroundColourId, t.plate);
    setColour (juce::Label::textColourId,                 t.text1);

    setColour (juce::ComboBox::backgroundColourId,        t.plate);
    setColour (juce::ComboBox::textColourId,              t.text1);
    setColour (juce::ComboBox::outlineColourId,           t.outline);
    setColour (juce::ComboBox::arrowColourId,             t.track);

    setColour (juce::PopupMenu::backgroundColourId,       t.plateEdge);
    setColour (juce::PopupMenu::textColourId,             t.text1);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, t.trackFill);
    setColour (juce::PopupMenu::highlightedTextColourId,  t.pointer);

    // The preset strip is built from TextButtons, which otherwise come out in
    // JUCE's default blue and fight the scheme.
    setColour (juce::TextButton::buttonColourId,   t.plate);
    setColour (juce::TextButton::buttonOnColourId, t.trackFill);
    setColour (juce::TextButton::textColourOffId,  t.text1);
    setColour (juce::TextButton::textColourOnId,   t.pointer);

    setColour (juce::AlertWindow::backgroundColourId, t.plateEdge);
    setColour (juce::AlertWindow::textColourId,       t.text1);
    setColour (juce::AlertWindow::outlineColourId,    t.outline);
    setColour (juce::TextEditor::backgroundColourId,  t.plate);
    setColour (juce::TextEditor::textColourId,        t.text1);
    setColour (juce::TextEditor::outlineColourId,     t.outline);
    setColour (juce::TextEditor::highlightColourId,   t.trackFill);
}

juce::Font BmoLookAndFeel::getLabelFont (juce::Label& label)
{
    return labelFont (label.getHeight() > 0 ? juce::jmin (12.0f, (float) label.getHeight()) : 11.0f);
}

juce::Font BmoLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return labelFont (juce::jmin (13.0f, (float) buttonHeight * 0.55f));
}

//==============================================================================
void BmoLookAndFeel::drawDottedArc (juce::Graphics& g, juce::Point<float> centre, float radius,
                                    float startAngle, float endAngle, juce::Colour colour,
                                    float dotSize)
{
    const auto span = endAngle - startAngle;
    const auto count = juce::jlimit (8, 96, juce::roundToInt (radius * span * 0.16f));

    g.setColour (colour);

    for (int i = 0; i <= count; ++i)
    {
        const auto a = startAngle + span * (float) i / (float) count;
        const juce::Point<float> at { centre.x + radius * std::sin (a),
                                      centre.y - radius * std::cos (a) };

        g.fillEllipse (juce::Rectangle<float> (dotSize, dotSize).withCentre (at));
    }
}

//==============================================================================
void BmoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float startAngle, float endAngle,
                                       juce::Slider& slider)
{
    const auto& t = tokens();
    auto* knob = dynamic_cast<Knob*> (&slider);
    const auto style = knob != nullptr ? knob->getStyle() : Knob::Style::utility;
    const auto moduleAccent = knob != nullptr ? knob->getAccent() : t.accent;

    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto centre = bounds.getCentre();
    const auto scale  = knob != nullptr ? knob->getFaceScale() : 1.0f;
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f * scale;
    const auto angle  = startAngle + sliderPos * (endAngle - startAngle);
    const auto enabled = slider.isEnabled();

    const auto dim = [enabled] (juce::Colour c) { return enabled ? c : c.withAlpha (0.35f); };

    const auto at = [centre] (float a, float r)
    {
        return juce::Point<float> { centre.x + r * std::sin (a), centre.y - r * std::cos (a) };
    };

    // The selector ring of a band: a white annulus with the selected position
    // marked on it, drawn behind the gain control that sits inside it.
    if (style == Knob::Style::ring)
    {
        const auto thickness = radius * 0.30f;
        const auto mid = radius - thickness * 0.5f;

        juce::Path ring;
        ring.addCentredArc (centre.x, centre.y, mid, mid, 0.0f, 0.0f,
                            juce::MathConstants<float>::twoPi, true);

        g.setColour (dim (t.pointer));
        g.strokePath (ring, juce::PathStrokeType (thickness));

        g.setColour (dim (t.knobEdge));
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), 1.6f);
        g.drawEllipse (juce::Rectangle<float> ((radius - thickness) * 2.0f,
                                               (radius - thickness) * 2.0f).withCentre (centre), 1.6f);

        // Where the switch is set.
        juce::Path marker;
        marker.addCentredArc (centre.x, centre.y, mid, mid, 0.0f,
                              angle - 0.10f, angle + 0.10f, true);

        g.setColour (dim (t.knobFace));
        g.strokePath (marker, juce::PathStrokeType (thickness));
        return;
    }

    const auto character = style == Knob::Style::character;
    const auto face      = character ? faceOf (moduleAccent) : t.knobFace;
    const auto accent    = character ? moduleAccent : t.track;
    const auto faceBox   = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

    // Gain controls carry a dotted track, with the rest position marked on it
    // and a plus -- and, where the control cuts as well, a minus -- at its ends.
    if (style == Knob::Style::utility || character)
    {
        const auto given = knob != nullptr ? knob->getTrackRadius() : 0.0f;
        const auto track = given > 0.0f ? given : radius + Tokens::trackGap;

        // The track stops just clear of each symbol rather than running dots
        // through it.
        constexpr float symbolClearance = 0.11f;

        drawDottedArc (g, centre, track, startAngle + symbolClearance, endAngle - symbolClearance,
                       dim (accent.withAlpha (enabled ? 0.55f : 0.2f)), 1.6f);

        // The heavy dot marks the control's rest position and stays there: zero
        // on a control that cuts and boosts, the bottom of the sweep on one
        // that only goes up.
        const auto range = slider.getRange();
        const auto zero  = range.getLength() > 0.0
                             ? (float) juce::jlimit (0.0, 1.0, (0.0 - range.getStart()) / range.getLength())
                             : 0.5f;

        g.setColour (dim (accent));
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f)
                           .withCentre (at (startAngle + zero * (endAngle - startAngle), track)));

        // Drawn rather than set. Neither panel face has a minus sign that
        // matches its plus, and two strokes and a bar are the one case where
        // drawing beats setting: they match each other exactly, at any size,
        // on any machine.
        {
            const auto arm = 5.0f;
            const auto weight = 2.6f;

            g.setColour (dim (accent));

            if (range.getStart() < 0.0)
            {
                const auto minusAt = at (startAngle, track);
                g.fillRect (juce::Rectangle<float> (arm * 2.0f, weight).withCentre (minusAt));
            }

            const auto plusAt = at (endAngle, track);
            g.fillRect (juce::Rectangle<float> (arm * 2.0f, weight).withCentre (plusAt));
            g.fillRect (juce::Rectangle<float> (weight, arm * 2.0f).withCentre (plusAt));
        }
    }

    g.setColour (dim (face));
    g.fillEllipse (faceBox);
    g.setColour (dim (character ? t.outline : t.knobEdge));
    g.drawEllipse (faceBox.reduced (0.8f), character ? 1.6f : Tokens::knobStroke);

    // Pointer.
    {
        const auto tip  = radius - 3.0f;
        const auto tail = radius * 0.05f;

        g.setColour (dim (t.pointer));
        g.drawLine ({ at (angle, tail), at (angle, tip) }, 2.6f);
    }
}

//==============================================================================
void BmoLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                       bool shouldDrawHighlighted, bool shouldDrawDown)
{
    const auto& t = tokens();
    const auto bounds = button.getLocalBounds().toFloat().reduced (3.0f);
    const auto on = button.getToggleState();

    // The switch's engaged colour is set by whoever made it: the module's
    // accent, the deeper azure, or the suite's pink.
    const auto tint = button.findColour (juce::ToggleButton::tickColourId);

    auto fill = on ? tint : t.switchOff;

    if (shouldDrawDown)             fill = fill.darker (0.12f);
    else if (shouldDrawHighlighted) fill = fill.brighter (0.06f);

    if (! button.isEnabled())
        fill = fill.withAlpha (0.35f);

    // An engaged switch glows: a few rounded rectangles stepping outwards at
    // falling alpha. Kept faint, because the text has to stay first.
    if (on && button.isEnabled())
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (tint.withAlpha (0.10f * (float) i / 3.0f));
            g.fillRoundedRectangle (bounds.expanded ((float) i), Tokens::corner + (float) i);
        }

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, Tokens::corner);

    const auto ink = t.pointer.withAlpha (button.isEnabled() ? 1.0f : 0.4f);

    // The polarity switch is drawn, not set: typing the slashed O gives back
    // whatever the machine maps it to, which on several faces is a plain O and
    // says nothing.
    const auto text = button.getButtonText();
    const auto font = labelFont (bounds.getHeight() * 0.62f, true);

    if (text.contains (phaseGlyph()))
    {
        // Whatever follows the glyph -- " L", " R" -- is set beside it.
        const auto rest = text.replace (phaseGlyph(), "").trim();
        const auto restWidth = rest.isEmpty() ? 0.0f
                             : juce::GlyphArrangement::getStringWidth (font, rest) + 4.0f;

        const auto r = bounds.getHeight() * 0.30f;
        const auto weight = juce::jmax (1.6f, r * 0.22f);
        const auto centre = bounds.getCentre().translated (-restWidth * 0.5f, 0.0f);

        juce::Path symbol;
        symbol.addEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));
        symbol.startNewSubPath (centre.x - r * 0.95f, centre.y + r * 0.95f);
        symbol.lineTo         (centre.x + r * 0.95f, centre.y - r * 0.95f);

        g.setColour (ink);
        g.strokePath (symbol, juce::PathStrokeType (weight, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

        if (rest.isNotEmpty())
            ui::drawLabel (g, rest, bounds.withLeft (centre.x + r + 4.0f), juce::Justification::centredLeft,
                       font, ink);
        return;
    }

    ui::drawLabel (g, text, bounds, juce::Justification::centred, font, ink);
}

} // namespace bmo::ui

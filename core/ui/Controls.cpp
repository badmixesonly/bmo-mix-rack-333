#include "Controls.h"

namespace bmo::ui
{

juce::String compactFrequency (const juce::String& text)
{
    if (text.equalsIgnoreCase ("off"))
        return "OFF";

    if (text.containsIgnoreCase ("kHz"))
    {
        const auto number = text.upToFirstOccurrenceOf (" ", false, true).trim();

        if (number.contains ("."))
            return number.upToFirstOccurrenceOf (".", false, false) + "k"
                 + number.fromFirstOccurrenceOf (".", false, false);

        return number + "k";
    }

    if (text.containsIgnoreCase ("Hz"))
        return text.upToFirstOccurrenceOf (" ", false, true).trim();

    return text;
}

//==============================================================================
PlainKnob::PlainKnob (juce::RangedAudioParameter& parameter, const juce::String& captionText,
                      Knob::Style style, float faceScale, juce::Colour accent, juce::Colour captionColourIn)
    : caption (captionText), captionColour (captionColourIn)
{
    knob.setStyle (style);
    knob.setAccent (accent);
    knob.setFaceScale (faceScale);
    addAndMakeVisible (knob);

    attachment = std::make_unique<juce::SliderParameterAttachment> (parameter, knob);
}

void PlainKnob::paint (juce::Graphics& g)
{
    // The name sits under the knob, in the caption face, in captionColour
    // (the shared track colour unless the module asked for its own).
    drawLabel (g, caption,
               getLocalBounds().removeFromBottom (kCaptionRow).withTrimmedBottom (4).toFloat(),
               juce::Justification::centred, captionFont (15.0f),
               knob.isEnabled() ? captionColour : captionColour.withAlpha (0.4f));
}

void PlainKnob::resized()
{
    // Square and centred, capped at knobSide. jmin(width, height) is what the
    // rotary's radius comes from, so squaring an already-narrower-than-tall
    // area leaves the drawn knob exactly where it was -- what it buys is the
    // freedom to make the component wider than the knob, so a long caption
    // has somewhere to go. See setKnobSide().
    const auto area = getLocalBounds().withTrimmedBottom (kCaptionRow);
    const auto side = juce::jmin (area.getWidth(), area.getHeight(), knobSide);

    knob.setBounds (area.withSizeKeepingCentre (side, side));
}

void PlainKnob::setKnobSide (int maxSide)
{
    knobSide = maxSide;
    resized();
}

void PlainKnob::setKnobEnabled (bool shouldBeEnabled)
{
    knob.setEnabled (shouldBeEnabled);
    repaint();
}

//==============================================================================
ConcentricBand::ConcentricBand (juce::RangedAudioParameter& selector, const ParamSpec& selectorSpec,
                                juce::RangedAudioParameter* gain, juce::Colour accent)
    : hasCentre (gain != nullptr)
{
    ring.setDetents (selectorSpec.numChoices());
    ring.setSliderSnapsToMousePosition (false);

    // The legend follows the ring's sweep, so narrowing the sweep lifts the
    // end labels off the bottom of the dial -- which is where the gain's plus
    // and minus live. Leave them on the same arc and the two collide.
    {
        const auto r = ring.getRotaryParameters();
        ring.setRotaryParameters (r.startAngleRadians + kLegendInset,
                                  r.endAngleRadians   - kLegendInset,
                                  r.stopAtEnd);
    }

    ring.setStyle (hasCentre ? Knob::Style::ring : Knob::Style::filter);
    ring.setFaceScale (hasCentre ? 0.529f : 0.35f);

    // The legend is drawn here, not by the slider, so a change of position has
    // to repaint the parent or the marked position goes stale.
    ring.onValueChange = [this] { repaint(); };

    addAndMakeVisible (ring);
    ringAttachment = std::make_unique<juce::SliderParameterAttachment> (selector, ring);

    if (hasCentre)
    {
        // Added second, so it sits above the ring and takes the mouse first.
        centre.setStyle (Knob::Style::character);
        centre.setAccent (accent);

        // The face is small, but the component spans the whole cell: its gain
        // track and its plus and minus are drawn well outside the face, and a
        // component only tight around the face would clip them away entirely.
        centre.setFaceScale (0.286f);
        centre.setCircularHitTest (true);
        addAndMakeVisible (centre);

        centreAttachment = std::make_unique<juce::SliderParameterAttachment> (*gain, centre);
    }

    for (int i = 0; i < selectorSpec.numChoices(); ++i)
        legend.add (compactFrequency (selectorSpec.choices[(size_t) i]));
}

void ConcentricBand::setRingEnabled (bool shouldBeEnabled)
{
    ringEnabled = shouldBeEnabled;
    ring.setEnabled (shouldBeEnabled);
    repaint();
}

void ConcentricBand::paint (juce::Graphics& g)
{
    if (legend.isEmpty())
        return;

    const auto& t = tokens();
    const auto area = getLocalBounds().toFloat();
    const auto centrePoint = area.getCentre();

    const auto ringRadius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f * ring.getFaceScale();

    // Knob edge, gap, dotted track, the same gap again, then the legend. Never
    // so far out that a label runs off the top of the cell and is clipped by
    // whatever is above it.
    const auto textRadius = juce::jmin (
        hasCentre ? centre.getTrackRadius() + Tokens::legendGap
                  : ringRadius + Tokens::filterLegendGap,
        area.getHeight() * 0.5f - 9.0f);

    const auto startAngle = ring.getRotaryParameters().startAngleRadians;
    const auto endAngle   = ring.getRotaryParameters().endAngleRadians;
    const auto selected   = juce::roundToInt (ring.getValue());

    for (int i = 0; i < legend.size(); ++i)
    {
        const auto f = legend.size() > 1 ? (float) i / (float) (legend.size() - 1) : 0.0f;
        const auto a = startAngle + f * (endAngle - startAngle);

        const juce::Point<float> at { centrePoint.x + textRadius * std::sin (a),
                                      centrePoint.y - textRadius * std::cos (a) };

        const auto isSelected = (i == selected);

        // Grey for a position you could switch to, azure for the one you are
        // on. With no outline, white would vanish on the plate.
        auto fill = isSelected ? t.switchAlt : t.text2;

        if (! ringEnabled)
            fill = fill.withAlpha (0.35f);

        drawLabel (g, legend[i], juce::Rectangle<float> (38.0f, 15.0f).withCentre (at),
                   juce::Justification::centred, labelFont (isSelected ? 13.0f : 12.0f, true), fill);
    }
}

void ConcentricBand::resized()
{
    ring.setBounds (getLocalBounds());

    if (! hasCentre)
        return;

    centre.setBounds (getLocalBounds());

    // The gain track has to clear the selector ring drawn around it, and the
    // gain control cannot work that out from its own face.
    const auto area = getLocalBounds().toFloat();
    const auto ringRadius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f * ring.getFaceScale();

    centre.setTrackRadius (ringRadius + Tokens::trackGap);
}

//==============================================================================
SwitchButton::SwitchButton (juce::RangedAudioParameter& parameter, const juce::String& text,
                            juce::Colour tint)
{
    button.setButtonText (text);
    button.setColour (juce::ToggleButton::tickColourId, tint);
    addAndMakeVisible (button);

    attachment = std::make_unique<juce::ButtonParameterAttachment> (parameter, button);
}

void SwitchButton::resized()                 { button.setBounds (getLocalBounds()); }
void SwitchButton::setSwitchEnabled (bool e) { button.setEnabled (e); }

//==============================================================================
OutputMeter::OutputMeter (std::function<float()> peakSource, std::function<float()> rmsSource)
    : peak (std::move (peakSource)), rms (std::move (rmsSource))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (30);
}

void OutputMeter::mouseUp (const juce::MouseEvent&)
{
    vuMode = ! vuMode;
    displayed = 0.0f;
    repaint();
}

void OutputMeter::timerCallback()
{
    const auto level = vuMode ? (rms ? rms() : 0.0f) : (peak ? peak() : 0.0f);

    // A VU meter integrates; a peak meter jumps and falls back slowly.
    const auto rate = vuMode ? 0.28f : (level > displayed ? 1.0f : 0.16f);

    displayed += rate * (level - displayed);
    repaint();
}

void OutputMeter::paint (juce::Graphics& g)
{
    const auto& t = tokens();

    auto bounds = getLocalBounds();
    const auto labelArea = bounds.removeFromBottom (12);
    const auto well = bounds.withSizeKeepingCentre (kBarWidth, bounds.getHeight()).toFloat();

    g.setColour (t.well);
    g.fillRoundedRectangle (well, 2.0f);

    const auto db = juce::Decibels::gainToDecibels (displayed, -70.0f);
    const auto lo = vuMode ? -20.0f : -60.0f;
    const auto hi = vuMode ? 3.0f : 0.0f;
    const auto reading = vuMode ? db - kVuReference : db;
    const auto norm = juce::jlimit (0.0f, 1.0f, (reading - lo) / (hi - lo));

    if (norm > 0.002f)
    {
        auto bar = well.reduced (1.5f);
        bar = bar.removeFromBottom (bar.getHeight() * norm);

        const auto hot  = vuMode ? reading > 0.0f  : reading > -1.0f;
        const auto warm = vuMode ? reading > -3.0f : reading > -9.0f;

        g.setColour (hot ? t.meterClip : warm ? t.meterHigh : t.meterLow);
        g.fillRoundedRectangle (bar, 1.5f);
    }

    g.setColour (t.outline.withAlpha (0.6f));
    g.drawRoundedRectangle (well.reduced (0.5f), 2.0f, 1.0f);

    drawLabel (g, vuMode ? "VU" : "dBFS", labelArea.toFloat(), juce::Justification::centred,
               labelFont (9.0f), t.text2);
}

//==============================================================================
DynamicsMeter::DynamicsMeter (std::function<float()> inputRmsSource,
                              std::function<float()> outputRmsSource,
                              std::function<float()> gainReductionDbSource,
                              Mode initialMode, juce::Colour accent, juce::Colour hot,
                              juce::Colour face)
    : inputRms (std::move (inputRmsSource)), outputRms (std::move (outputRmsSource)),
      gainReductionDb (std::move (gainReductionDbSource)), mode (initialMode),
      accentColour (accent), hotColour (hot), faceColour (face)
{
    startTimerHz (30);
}

void DynamicsMeter::setMode (Mode newMode) noexcept
{
    if (newMode == mode)
        return;

    mode = newMode;
    // The two VU sources are linear RMS, the reduction source is already in
    // dB -- `displayed` is whichever unit the current mode reads in, so a
    // stale value from the old mode would paint a nonsense deflection for
    // one frame if it weren't reset here.
    displayed = 0.0f;
    repaint();
}

void DynamicsMeter::timerCallback()
{
    float level = 0.0f;

    switch (mode)
    {
        case Mode::input:     level = inputRms         ? inputRms()         : 0.0f; break;
        case Mode::output:    level = outputRms        ? outputRms()        : 0.0f; break;
        case Mode::reduction: level = gainReductionDb  ? gainReductionDb()  : 0.0f; break;
    }

    // Same integration on every mode: VU levels and a dB reduction figure
    // both read as "how much is happening right now", so one rate serves all
    // three rather than needing a peak/VU distinction of its own.
    displayed += 0.28f * (level - displayed);
    repaint();
}

float DynamicsMeter::fractionFor (float value, const std::vector<ScalePoint>& scale) const noexcept
{
    if (value <= scale.front().value)  return scale.front().fraction;
    if (value >= scale.back().value)   return scale.back().fraction;

    for (size_t i = 1; i < scale.size(); ++i)
    {
        const auto& a = scale[i - 1];
        const auto& b = scale[i];

        if (value <= b.value)
        {
            const auto t = (value - a.value) / (b.value - a.value);
            return a.fraction + t * (b.fraction - a.fraction);
        }
    }

    return scale.back().fraction;
}

void DynamicsMeter::paint (juce::Graphics& g)
{
    const auto& t = tokens();

    // Approximates a classic VU faceplate: compressed toward -20, spread out
    // from 0 to +3, where 0 VU sits noticeably right of centre rather than
    // in the middle of the sweep. Not one real meter's calibration data --
    // just close enough to read as the genre (see class comment).
    // -2, -1, +1 and +2 are struck but not numbered: from -3 up the scale
    // crowds into the last third of the sweep, and inking every one of them
    // is what left the numbers illegibly small and touching. The numbered
    // ones are the figures a VU is actually read against.
    static const std::vector<ScalePoint> vuScale {
        { -20.0f, 0.00f }, { -10.0f, 0.34f }, { -7.0f, 0.44f }, { -5.0f, 0.53f },
        { -3.0f, 0.63f },  { -2.0f, 0.69f, false }, { -1.0f, 0.76f, false }, { 0.0f, 0.83f },
        { 1.0f, 0.89f, false }, { 2.0f, 0.94f, false }, { 3.0f, 1.00f },
    };
    static const std::vector<ScalePoint> grScale {
        { 0.0f, 0.0f }, { 4.0f, 1.0f / 6.0f }, { 8.0f, 2.0f / 6.0f }, { 12.0f, 0.5f },
        { 16.0f, 4.0f / 6.0f }, { 20.0f, 5.0f / 6.0f }, { kGrRangeDb, 1.0f },
    };

    const auto isReduction = mode == Mode::reduction;

    auto bounds = getLocalBounds().toFloat();
    const auto modeLabelArea = bounds.removeFromBottom (14.0f);

    // Sweep geometry: needle pivots at bottom-centre, arcs upward. 100
    // degrees total, split evenly either side of straight up.
    const auto pivot     = bounds.getBottomLeft().translated (bounds.getWidth() * 0.5f, 0.0f);
    const auto radius    = juce::jmin (bounds.getWidth() * 0.5f, bounds.getHeight()) - 6.0f;
    // 124 rather than 100 degrees: the numbers are set larger now, and the
    // extra arc is what keeps them apart at the crowded top of the scale.
    const auto sweep     = juce::degreesToRadians (124.0f);
    const auto startAngle = -sweep * 0.5f;
    const auto angleFor  = [&] (float fraction) { return startAngle + fraction * sweep; };

    // Face plate: dark, so the light ink on it reads. The bezel stays the
    // module's accent -- semantic colour on the frame, luminance contrast on
    // everything that has to be read.
    g.setColour (faceColour);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (accentColour.withAlpha (0.7f));
    g.drawRoundedRectangle (bounds.reduced (0.75f), 4.0f, 1.5f);

    // Scale ticks and numbers. Split into two paths so the 0 VU and above
    // zone -- hotColour, a classic VU meter's red printed in the module's
    // own colour instead -- strokes separately from the rest of the scale.
    const auto& scale = isReduction ? grScale : vuScale;
    juce::Path ticks, hotTicks;

    for (const auto& p : scale)
    {
        const auto angle = angleFor (p.fraction);
        const auto inner = pivot.getPointOnCircumference (radius - 7.0f, angle);
        const auto outer = pivot.getPointOnCircumference (radius, angle);
        const auto hot = ! isReduction && p.value >= 0.0f;

        auto& path = hot ? hotTicks : ticks;
        path.startNewSubPath (inner);
        path.lineTo (outer);

        if (! p.numbered)
            continue;

        // The scale is printed in white, the hot zone in the module's own
        // colour. Colour marks the zone; contrast does the reading.
        const auto labelCentre = pivot.getPointOnCircumference (radius - 19.0f, angle);
        drawLabel (g, juce::String ((int) p.value),
                   juce::Rectangle<float> (28.0f, 15.0f).withCentre (labelCentre),
                   juce::Justification::centred, labelFont (11.5f),
                   hot ? hotColour : t.pointer);
    }

    g.setColour (t.pointer);
    g.strokePath (ticks, juce::PathStrokeType (1.4f));
    g.setColour (hotColour);
    g.strokePath (hotTicks, juce::PathStrokeType (1.4f));

    // Needle.
    const auto valueForNeedle = isReduction ? juce::jlimit (0.0f, kGrRangeDb, displayed)
                                              : juce::Decibels::gainToDecibels (displayed, -70.0f) - kVuReference;
    const auto needleAngle = angleFor (fractionFor (valueForNeedle, scale));
    const auto tip = pivot.getPointOnCircumference (radius - 4.0f, needleAngle);

    // White in every mode. The needle is the one thing on this panel that has
    // to be legible before you look at it, so it gets the maximum contrast
    // against the face rather than a colour that says which mode is up --
    // the button row underneath already says that.
    g.setColour (t.pointer);
    g.drawLine (juce::Line<float> (pivot, tip), 2.4f);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (pivot));

    const auto readoutLabel = isReduction ? "GR" : (mode == Mode::input ? "IN" : "OUT");
    // Spelled as an escape rather than a literal bullet: MSVC without /utf-8
    // reads a BOM-less source file in the system codepage, which would mangle
    // the character on Windows only. This is the one non-ASCII glyph in the
    // suite's sources -- keep it that way, or set the flag.
    drawLabel (g, juce::String (juce::CharPointer_UTF8 ("VU  \xe2\x80\xa2  ")) + readoutLabel,
               modeLabelArea,
               juce::Justification::centred, labelFont (9.0f), t.text2);
}

} // namespace bmo::ui

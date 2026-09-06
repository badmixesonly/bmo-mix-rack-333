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
                      Knob::Style style, float faceScale, juce::Colour accent)
    : caption (captionText)
{
    knob.setStyle (style);
    knob.setAccent (accent);
    knob.setFaceScale (faceScale);
    addAndMakeVisible (knob);

    attachment = std::make_unique<juce::SliderParameterAttachment> (parameter, knob);
}

void PlainKnob::paint (juce::Graphics& g)
{
    const auto& t = tokens();

    // The name sits under the knob, in the caption face, in the track colour.
    drawLabel (g, caption,
               getLocalBounds().removeFromBottom (kCaptionRow).withTrimmedBottom (4).toFloat(),
               juce::Justification::centred, captionFont (15.0f),
               knob.isEnabled() ? t.track : t.track.withAlpha (0.4f));
}

void PlainKnob::resized()
{
    knob.setBounds (getLocalBounds().withTrimmedBottom (kCaptionRow));
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
                              Mode initialMode)
    : inputRms (std::move (inputRmsSource)), outputRms (std::move (outputRmsSource)),
      gainReductionDb (std::move (gainReductionDbSource)), mode (initialMode)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (30);
}

void DynamicsMeter::mouseUp (const juce::MouseEvent&)
{
    mode = mode == Mode::input ? Mode::output : (mode == Mode::output ? Mode::reduction : Mode::input);
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

void DynamicsMeter::paint (juce::Graphics& g)
{
    const auto& t = tokens();

    auto bounds = getLocalBounds();
    const auto labelArea = bounds.removeFromBottom (12);
    const auto well = bounds.withSizeKeepingCentre (kBarWidth, bounds.getHeight()).toFloat();

    g.setColour (t.well);
    g.fillRoundedRectangle (well, 2.0f);

    float norm = 0.0f;
    bool hot = false, warm = false;
    juce::Colour barColour;
    juce::String label;

    if (mode == Mode::reduction)
    {
        norm = juce::jlimit (0.0f, 1.0f, displayed / kGrRangeDb);
        barColour = t.meterGr;
        label = "GR";
    }
    else
    {
        const auto db = juce::Decibels::gainToDecibels (displayed, -70.0f);
        const auto reading = db - kVuReference;
        norm = juce::jlimit (0.0f, 1.0f, (reading - -20.0f) / (3.0f - -20.0f));
        hot  = reading > 0.0f;
        warm = reading > -3.0f;
        barColour = hot ? t.meterClip : warm ? t.meterHigh : t.meterLow;
        label = mode == Mode::input ? "IN" : "OUT";
    }

    if (norm > 0.002f)
    {
        auto bar = well.reduced (1.5f);
        bar = bar.removeFromBottom (bar.getHeight() * norm);

        g.setColour (barColour);
        g.fillRoundedRectangle (bar, 1.5f);
    }

    g.setColour (t.outline.withAlpha (0.6f));
    g.drawRoundedRectangle (well.reduced (0.5f), 2.0f, 1.0f);

    drawLabel (g, label, labelArea.toFloat(), juce::Justification::centred, labelFont (9.0f), t.text2);
}

} // namespace bmo::ui

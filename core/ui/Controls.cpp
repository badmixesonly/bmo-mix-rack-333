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
    : caption (captionText), captionColour (captionColourIn), accentColour (accent)
{
    // A component name, so a layout test can find this knob by the caption a
    // reader sees. JUCE hands it to the accessibility layer as well.
    setName (captionText);

    knob.setStyle (style);
    knob.setAccent (accent);
    knob.setFaceScale (faceScale);
    addAndMakeVisible (knob);

    attachment = std::make_unique<juce::SliderParameterAttachment> (parameter, knob);
}

juce::Rectangle<int> PlainKnob::captionBox() const
{
    return { 0, knob.getBottom(), getWidth(), captionRow() - 4 };
}

float PlainKnob::captionOverflow() const
{
    return juce::GlyphArrangement::getStringWidth (captionFont (captionSize), caption)
             - (float) captionBox().getWidth();
}

void PlainKnob::paint (juce::Graphics& g)
{
    // The name sits under the knob, in the caption face. Derived here rather
    // than cached in the constructor so that editing the theme file recolours
    // an open panel -- the editors repaint on a theme change but do not
    // rebuild their controls.
    //
    // A caption is a legible step of whichever colour system its knob belongs
    // to, not of the module's accent regardless. A character knob -- drive,
    // tone, a band's gain -- is drawn in the module's colour and its caption
    // follows it. A utility knob is deliberately the same pale blue in every
    // module, with a blue track and blue plus and minus, so INPUT and OUTPUT
    // read as the same control wherever they are; setting those captions in
    // the accent put pink text on BMO EQ's blue cap and orange on the
    // Saturator's.
    const auto system = knob.getStyle() == Knob::Style::character ? accentColour
                                                                 : tokens().track;

    // The colour system as it stands, not stepped for contrast. A caption is
    // the larger of a panel's two labels -- 15 pt against a section legend's
    // 13 -- and it names a knob you are already looking at, where the legend
    // is what you navigate by. So the raw colour goes here and the legible
    // step goes on the legend; see ModulePanel::drawRuleLegend.
    //
    // The two swapped in 0.2.3 and the swap costs contrast here: on the pale
    // plate a caption goes from 4.57-4.69:1 to 1.72-2.00:1, and on the dark
    // one from 9.07 to 5.87. Frosty's call, taken on a render with those
    // numbers in front of him. Do not "fix" it.
    const auto ink = captionColour.isTransparent() ? system : captionColour;


    // Hung off the knob's own bottom edge, not the component's. The two are
    // the same thing for a knob that fills its cell, which every knob in the
    // suite did until gain knobs were capped at one shared size -- a capped
    // knob centres in a taller area, and a caption pinned to the foot of the
    // cell would drift away from it by half the difference, and drift further
    // every time the type got smaller.
    const auto box = captionBox();

    drawLabel (g, caption, box.toFloat(),
               juce::Justification::centred, captionFont (captionSize),
               knob.isEnabled() ? ink : ink.withAlpha (0.4f));
}

void PlainKnob::resized()
{
    // Square and centred, capped at knobSide. jmin(width, height) is what the
    // rotary's radius comes from, so squaring an already-narrower-than-tall
    // area leaves the drawn knob exactly where it was -- what it buys is the
    // freedom to make the component wider than the knob, so a long caption
    // has somewhere to go. See setKnobSide().
    const auto area = getLocalBounds().withTrimmedBottom (captionRow());
    const auto side = juce::jmin (area.getWidth(), area.getHeight(), knobSide);

    knob.setBounds (area.withSizeKeepingCentre (side, side));
}

void PlainKnob::setKnobSide (int maxSide)
{
    knobSide = maxSide;
    resized();
}

void PlainKnob::setCaptionSize (float points)
{
    captionSize = points;
    resized();
    repaint();
}

void PlainKnob::setKnobEnabled (bool shouldBeEnabled)
{
    knob.setEnabled (shouldBeEnabled);
    repaint();
}

void PlainKnob::setAccent (juce::Colour accent)
{
    accentColour = accent;
    knob.setAccent (accent);
    repaint();
}

//==============================================================================
ConcentricBand::ConcentricBand (juce::RangedAudioParameter& selector, const ParamSpec& selectorSpec,
                                juce::RangedAudioParameter* gain, juce::Colour accent,
                                bool outsetFan)
    : accentColour (accent), hasCentre (gain != nullptr)
{
    ring.setDetents (selectorSpec.numChoices());
    ring.setSliderSnapsToMousePosition (false);

    // The legend follows the ring's sweep -- paint() puts label i at the angle
    // the pointer takes for value i -- so the sweep is what decides where the
    // legend sits, and the two kinds of control want different sweeps.
    {
        const auto r = ring.getRotaryParameters();
        const auto start = r.startAngleRadians + kLegendInset;

        if (gain != nullptr)
        {
            // A band's frequencies occupy the left half of the dial and its
            // gain the right, so the two controls on it are told apart by
            // which side of the knob they are on.
            //
            // Before 0.2.3 they shared the whole circle at different radii,
            // and the collisions that came of it were fixed one at a time:
            // the gain's rest dot and the selected frequency both want to
            // point straight up, and on a band with an odd number of
            // positions they landed a pixel and a half apart and read as one
            // mark. BMO EQ's high shelf is three positions with 12 kHz in the
            // middle, and 12 kHz is the default, so that was the panel's
            // opening state.
            //
            // The middle of the available frequencies sits at 9 o'clock.
            // Higher values radiate clockwise from it, toward 12; lower ones
            // counter-clockwise, toward 6. An even count straddles 9 o'clock
            // rather than landing on it, which is what "the middle of the
            // frequencies" means when there is no middle frequency.
            //
            // The ends stop short of 12 and 6, or run just past them, and the
            // direction alternates down the panel. Left to itself every band
            // puts a label at dead-centre top and dead-centre bottom, so the
            // high shelf's lowest and the mid bell's highest would sit one
            // above the other on the same x with only a rule between them --
            // a column of numbers down the middle of the panel. Alternating
            // the nudge breaks it.
            const auto pi = juce::MathConstants<float>::pi;
            const auto nudge = outsetFan ? -kFanNudge : kFanNudge;

            ring.setRotaryParameters (pi + nudge, pi * 2.0f - nudge, r.stopAtEnd);
        }
        else
        {
            // A filter. Its positions sit evenly around a full circle, on a
            // step sized so that exactly one position is left over: five
            // frequencies plus one gap, at sixty degrees each. Nothing is
            // drawn in the empty one -- it falls at the foot of the dial,
            // between the highest cut and Off, and the double gap there is
            // what says which way the control sweeps.
            const auto step = juce::MathConstants<float>::twoPi
                                / (float) (selectorSpec.numChoices() + 1);

            ring.setRotaryParameters (start,
                                      start + step * (float) (selectorSpec.numChoices() - 1),
                                      r.stopAtEnd);
        }
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

        // The gain keeps the suite's own sweep untouched: rest dot straight
        // up, minus at about 7:25, plus at about 4:35, pointer vertical at
        // 0 dB like every other knob in the suite. Only the frequency fan
        // moved, and it moved to the half of the dial the gain was not using.
        //
        // Two other arrangements were built and thrown away on the way here.
        // Turning the gain a quarter clockwise, to put its rest dot opposite
        // the fan at 3 o'clock, works on paper and reads as a knob turned hard
        // right at zero -- the rest dot is where the pointer rests, so moving
        // one moves the other. Flipping and shrinking the sweep to a 120
        // degree arc on the right, minus at 5 and plus at 1, keeps the two
        // controls on separate sides but makes gain rise anti-clockwise: with
        // zero at 3 o'clock and the sweep symmetric about it, the end that
        // carries the minus is the end that fixes the direction.
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

ConcentricBand::Geometry ConcentricBand::geometry() const
{
    const auto area = getLocalBounds().toFloat();
    const auto ringRadius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f * ring.getFaceScale();

    // Knob edge, gap, dotted track, the same gap again, then the legend --
    // never so far out that a label runs off the top of the cell and is
    // clipped by whatever is above it.
    //
    // A band keeps a 9 px margin for that. A filter needs less of one: its
    // lowest position is the blank, so nothing is pushing down, and the row
    // above it is a rule with clear space under the line. At 9 the clamp was
    // biting -- the legend wanted 33.3 and got 29 -- and that was the whole of
    // why the circle looked cramped.
    const auto margin = hasCentre ? 9.0f : 4.0f;

    // The gain's dotted track runs between the selector ring and the frequency
    // legend, and the legend is the outermost thing on the dial. Swapping the
    // two -- legend tight to the ring, track outside it -- was built and works,
    // but it is not needed once the two controls are on opposite halves, and
    // it costs the track the clearance the cell's height gives the legend.
    const auto textRadius = juce::jmin (
        hasCentre ? centre.getTrackRadius() + Tokens::legendGap
                  : ringRadius + Tokens::filterLegendGap,
        area.getHeight() * 0.5f - margin);

    if (hasCentre)
        return { ringRadius, textRadius, 0 };

    // Measure how far the ink actually reaches above and below the dial, and
    // nudge the assembly by half the difference. Every position but the blank
    // carries a label, and the blank is at the foot, so the answer is always a
    // shift downwards -- but it is measured rather than assumed, so it follows
    // a change in the number of positions or the radius on its own.
    const auto start = ring.getRotaryParameters().startAngleRadians;
    const auto end   = ring.getRotaryParameters().endAngleRadians;

    auto top = -ringRadius, bottom = ringRadius;

    for (int i = 0; i < legend.size(); ++i)
    {
        const auto f = legend.size() > 1 ? (float) i / (float) (legend.size() - 1) : 0.0f;
        const auto y = -std::cos (start + f * (end - start)) * textRadius;

        top    = juce::jmin (top,    y - kLegendBoxHeight * 0.5f);
        bottom = juce::jmax (bottom, y + kLegendBoxHeight * 0.5f);
    }

    return { ringRadius, textRadius, juce::roundToInt (-(top + bottom) * 0.5f) };
}

void ConcentricBand::paint (juce::Graphics& g)
{
    if (legend.isEmpty())
        return;

    const auto& t = tokens();
    const auto area = getLocalBounds().toFloat();
    const auto geo = geometry();
    const auto centrePoint = area.getCentre().translated (0.0f, (float) geo.shift);
    const auto textRadius = geo.textRadius;

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

        // The module's own colour for the position you are on, neutral for one
        // you could switch to. Until 0.2.2 selected was the shared azure at
        // 2.22:1 and unselected was text2 at 2.45:1 -- so the *unselected*
        // legends had more contrast than the selected one, on the control this
        // module is mostly used through. Moving unselected up to text1 fixed
        // that on the pale plate: 4.53:1 selected against 4.37, near enough
        // equal, with hue doing the work.
        //
        // On the dark plate the same pair measures 8.98:1 selected against
        // 10.87, so the inversion is still there -- the frequency you did not
        // pick is seven points of L* brighter than the one you did. That is
        // deliberate and it is not fixable from this line. text1 sits at 10.87
        // of a 13.53 ceiling on this plate, and a *coloured* ink cannot pass it:
        // taking the accent past that luminance means mixing it so far toward
        // white it stops reading as the accent. The only lever is to dim the
        // unselected legends instead, and that was built, rendered and thrown
        // away -- at 6.00:1 it works, and it dims the numbers you read to
        // decide where to go next in order to emphasise the one you already
        // know.
        //
        // What carries selection here is the band marker, which since 0.2.3 is
        // the accent at full strength pointing straight at the chosen position.
        // It did not used to be: when this was first measured the marker was
        // near-black on a middle-grey ring, so nothing on the dial said which
        // frequency was live. Fixing the ring fixed the premise, and the
        // labels were left to hue. Frosty's call, on a side-by-side.
        auto fill = isSelected ? accentInk (accentColour) : t.text1;

        if (! ringEnabled)
            fill = fill.withAlpha (0.35f);

        drawLabel (g, legend[i], juce::Rectangle<float> (kLegendBoxWidth, kLegendBoxHeight).withCentre (at),
                   juce::Justification::centred,
                   kPointUsesCaption ? captionFont (isSelected ? kPointSize : kPointSizeIdle)
                                     : labelFont   (isSelected ? kPointSize : kPointSizeIdle, true),
                   fill);
    }
}

void ConcentricBand::resized()
{
    // The dial moves with its legend, so a filter is nudged down by the same
    // amount paint() nudges the labels -- see geometry().
    const auto bounds = getLocalBounds().translated (0, geometry().shift);

    ring.setBounds (bounds);

    if (! hasCentre)
        return;

    centre.setBounds (bounds);

    // The gain track has to clear the selector ring drawn around it, and the
    // gain control cannot work that out from its own face.
    const auto area = getLocalBounds().toFloat();
    const auto ringRadius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f * ring.getFaceScale();

    centre.setTrackRadius (ringRadius + Tokens::concentricTrackGap);
}

//==============================================================================
SwitchButton::SwitchButton (juce::RangedAudioParameter& parameter, const juce::String& text,
                            juce::Colour tint)
{
    setName (text);
    button.setButtonText (text);
    button.setColour (juce::ToggleButton::tickColourId, tint);
    addAndMakeVisible (button);

    attachment = std::make_unique<juce::ButtonParameterAttachment> (parameter, button);
}

void SwitchButton::resized()                 { button.setBounds (getLocalBounds()); }
void SwitchButton::setSwitchEnabled (bool e) { button.setEnabled (e); }

void SwitchButton::setLockedOn (bool shouldBeLocked)
{
    locked = shouldBeLocked;

    // Clicks off rather than enabled off: enabled off is what dims it, and a
    // switch the DSP is holding on is not a dimmed switch, it is an engaged
    // one you cannot turn off from here.
    button.setInterceptsMouseClicks (! locked, ! locked);

    if (locked)
    {
        // dontSendNotification, so the attachment does not hear it and the
        // parameter keeps the value the user set. The caller re-asserts this
        // while the lock holds -- a parameter change would otherwise push the
        // stored value back into the button underneath us -- and hands the
        // switch its real state back when the lock lifts.
        button.setToggleState (true, juce::dontSendNotification);
        button.repaint();
    }
}

void SwitchButton::setToggleStateSilently (bool shouldBeOn)
{
    button.setToggleState (shouldBeOn, juce::dontSendNotification);
    button.repaint();
}

void SwitchButton::setTint (juce::Colour tint)
{
    button.setColour (juce::ToggleButton::tickColourId, tint);
    button.repaint();
}

void SwitchButton::setActiveInkFrom (juce::Colour accent)
{
    // Derived against the fill the ink will sit on rather than against the
    // plate. On the pale plate the two land within a step of each other, so
    // the label matches the module's captions; on a dark one, deriving
    // against the plate would *lighten* the accent and put pale green on a
    // white switch.
    const auto fill = button.findColour (juce::ToggleButton::tickColourId);

    button.setColour (juce::ToggleButton::textColourId, accentTextOn (accent, fill));
    button.repaint();
}

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
                              Mode initialMode, juce::Colour accent, juce::Colour hot)
    : inputRms (std::move (inputRmsSource)), outputRms (std::move (outputRmsSource)),
      gainReductionDb (std::move (gainReductionDbSource)), mode (initialMode),
      accentColour (accent), hotColour (hot)
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

void DynamicsMeter::setColours (juce::Colour accent, juce::Colour hot) noexcept
{
    accentColour = accent;
    hotColour = hot;
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
    // The first point is struck but never printed, and it is where the needle
    // parks in silence. 0.2.1 started the scale at -20, so an idle meter left
    // the needle lying across its own leftmost numeral -- the rest state, and
    // therefore the state the meter is in most of the time.
    //
    // -7 and -3 lost their numbers as well. Five printed figures is what fits:
    // at this radius the -10 to -5 gap is the tightest pair on the scale and
    // clears by about 6 px, and every figure added between them takes that
    // back. Hardware faceplates ink only the round figures for the same
    // reason, and every tick is still struck here.
    static const std::vector<ScalePoint> vuScale {
        { -30.0f, 0.00f, false },
        { -20.0f, 0.10f }, { -15.0f, 0.27f, false }, { -10.0f, 0.41f }, { -7.0f, 0.50f, false }, { -5.0f, 0.58f },
        { -3.0f, 0.67f, false }, { -2.0f, 0.72f, false }, { -1.0f, 0.78f, false }, { 0.0f, 0.85f },
        { 1.0f, 0.90f, false }, { 2.0f, 0.95f, false }, { 3.0f, 1.00f },
    };
    static const std::vector<ScalePoint> grScale {
        { 0.0f, 0.0f }, { 4.0f, 1.0f / 6.0f }, { 8.0f, 2.0f / 6.0f }, { 12.0f, 0.5f },
        { 16.0f, 4.0f / 6.0f }, { 20.0f, 5.0f / 6.0f }, { kGrRangeDb, 1.0f },
    };

    const auto isReduction = mode == Mode::reduction;

    // The whole box is face. A caption naming the current mode used to take 14
    // px off the bottom of it, and it was saying what the IN/GR/OUT row under
    // the meter already says -- louder, in the same place, and without the 9 pt
    // and 2.45:1 on the pale plate that the caption was read at. The 14 px went
    // back to the panel. With it went the suite's only non-ASCII source glyph.
    auto bounds = getLocalBounds().toFloat();

    // Sweep geometry: needle pivots at bottom-centre, arcs upward. 100
    // degrees total, split evenly either side of straight up.
    // Half the width of the hub the needle turns on.
    constexpr float kHubRadius = 3.5f;

    // A 124 degree sweep is limited by width, never by height: the arc ends
    // reach sin(62) = 0.88 of the radius sideways but only 0.47 of it
    // downwards. Taking the radius from jmin(width/2, height) therefore sized
    // the arc to the wrong dimension whenever the face was taller than half
    // its width, which is every face this has been given -- 0.2.1 shipped with
    // roughly 40% of the meter empty above the needle.
    const auto radius = bounds.getWidth() * 0.5f - 8.0f;

    // What actually gets drawn runs from the apex, one radius above the pivot,
    // down to the hub. Centre that block in whatever face the panel hands over
    // rather than pinning the pivot to the bottom edge, so a taller box can
    // never bring the dead band back. A box shorter than the block puts the
    // pivot below the face, which is where the hardware hides it anyway.
    const auto drawnHeight = radius + kHubRadius;
    const auto pivot = juce::Point<float> (bounds.getCentreX(),
                                           bounds.getY() + (bounds.getHeight() + drawnHeight) * 0.5f);
    // 124 rather than 100 degrees: the numbers are set larger now, and the
    // extra arc is what keeps them apart at the crowded top of the scale.
    const auto sweep     = juce::degreesToRadians (124.0f);
    const auto startAngle = -sweep * 0.5f;
    const auto angleFor  = [&] (float fraction) { return startAngle + fraction * sweep; };

    // Face plate: dark, so the light ink on it reads. The bezel stays the
    // module's accent -- semantic colour on the frame, luminance contrast on
    // everything that has to be read.
    g.setColour (t.meterFace);
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
                   hot ? hotColour : t.meterInk);
    }

    g.setColour (t.meterInk);
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
    g.setColour (t.meterInk);
    g.drawLine (juce::Line<float> (pivot, tip), 2.4f);
    g.fillEllipse (juce::Rectangle<float> (kHubRadius * 2.0f, kHubRadius * 2.0f).withCentre (pivot));
}

} // namespace bmo::ui

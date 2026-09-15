#include "LevelBars.h"
#include "core/ui/ModulePanel.h"

namespace bmo::vcomp
{

namespace
{
    /** The suite's peak ballistics, from OutputMeter::timerCallback: straight
        to a new high, back down at 0.16 of the distance a tick. */
    constexpr float kRiseRate = 1.0f;
    constexpr float kFallRate = 0.16f;

    constexpr float kCaptionSize = 10.0f;
    constexpr int   kHandleWidth = 3;

    /** Ticks every 12 dB: four across a 60 dB level bar, one across the GR
        bar's 24. Enough to read a threshold against, few enough not to turn
        the well into a ruler. */
    constexpr float kTickStepDb = 12.0f;
}

LevelBar::LevelBar (juce::String captionText, Grow growDirection,
                    float minimumDb, float maximumDb, std::function<float()> levelSource)
    : caption (std::move (captionText)),
      grow (growDirection),
      minDb (minimumDb),
      maxDb (maximumDb),
      source (std::move (levelSource))
{
    setInterceptsMouseClicks (false, false);
}

void LevelBar::setFlatColour (juce::Colour colour)
{
    flat = colour;
    useFlatColour = true;
}

void LevelBar::attachThreshold (juce::RangedAudioParameter& param, juce::Colour colour)
{
    threshold = &param;
    handleColour = colour;

    // Only a bar carrying a handle takes the mouse. The other two are read,
    // not touched, and a meter that swallows clicks it does nothing with is a
    // meter that feels broken.
    setInterceptsMouseClicks (true, false);
}

juce::Rectangle<int> LevelBar::wellBounds() const
{
    auto area = getLocalBounds();
    area.removeFromLeft (kCaptionWidth);
    return area.withSizeKeepingCentre (area.getWidth(), kBarHeight);
}

float LevelBar::normalised (float db) const
{
    return juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
}

void LevelBar::refresh()
{
    const auto reading = source ? source() : minDb;
    const auto target = normalised (reading);

    displayed += (target > displayed ? kRiseRate : kFallRate) * (target - displayed);

    repaint();
}

void LevelBar::paint (juce::Graphics& g)
{
    const auto t = ui::panelTokensFor (*this);

    const auto well = wellBounds().toFloat();

    ui::drawLabel (g, caption,
                   getLocalBounds().removeFromLeft (kCaptionWidth - 6).toFloat(),
                   juce::Justification::centredRight, ui::labelFont (kCaptionSize), t.text1);

    g.setColour (t.well);
    g.fillRoundedRectangle (well, 2.0f);

    if (displayed > 0.002f)
    {
        auto bar = well.reduced (1.5f);
        const auto length = bar.getWidth() * displayed;

        bar = grow == Grow::rightward ? bar.removeFromLeft (length)
                                      : bar.removeFromRight (length);

        if (useFlatColour)
        {
            g.setColour (flat);
        }
        else
        {
            // The suite's zones, at the same thresholds OutputMeter uses.
            const auto db = minDb + displayed * (maxDb - minDb);
            g.setColour (db > -1.0f ? t.meterClip : db > -9.0f ? t.meterHigh : t.meterLow);
        }

        g.fillRoundedRectangle (bar, 1.5f);
    }

    // Ticks every 12 dB, drawn over the fill. Without them the bars are three
    // lengths with no units, which is tolerable for IN and OUT -- you read
    // those against each other -- and not tolerable on the bar carrying the
    // gate handle, where the whole point is knowing what level you are setting
    // the threshold to. So the scale is on every bar rather than only the one
    // that needs it: three bars with two kinds of well would read as two
    // different instruments.
    g.setColour (t.meterInk.withAlpha (0.22f));

    for (auto db = minDb + kTickStepDb; db < maxDb; db += kTickStepDb)
    {
        const auto at = well.getX() + well.getWidth() * normalised (db);
        g.drawVerticalLine ((int) at, well.getY() + 2.0f, well.getBottom() - 2.0f);
    }

    g.setColour (t.outline.withAlpha (0.6f));
    g.drawRoundedRectangle (well.reduced (0.5f), 2.0f, 1.0f);

    if (threshold != nullptr)
    {
        // The handle is drawn on the same scale the fill is, so where it sits
        // is literally the level it will act at -- which is the entire reason
        // the gate lives here instead of on a knob.
        const auto value = threshold->convertFrom0to1 (threshold->getValue());
        const auto at = well.getX() + well.getWidth() * normalised (value);

        // Resolved here rather than at attachThreshold, so it follows a line's
        // ink and follows an appearance change with it. The handle was the
        // last thing on an LTV panel still carrying the module accent, and a
        // lone periwinkle mark on a silver plate is the loose end this closes.
        g.setColour (ui::panelAccentFor (*this, handleColour));
        g.fillRect (juce::Rectangle<float> (at - (float) kHandleWidth * 0.5f,
                                            well.getY() - 3.0f,
                                            (float) kHandleWidth,
                                            well.getHeight() + 6.0f));
    }
}

void LevelBar::setThresholdFromX (int x)
{
    const auto well = wellBounds();
    const auto proportion = juce::jlimit (0.0f, 1.0f,
                                          (float) (x - well.getX()) / (float) well.getWidth());

    threshold->setValueNotifyingHost (
        threshold->convertTo0to1 (minDb + proportion * (maxDb - minDb)));
}

void LevelBar::mouseDown (const juce::MouseEvent& e)
{
    if (threshold == nullptr)
        return;

    // Click anywhere on the bar puts the handle there, then the drag refines
    // it: a handle you have to grab exactly is a handle you miss.
    dragging = true;
    threshold->beginChangeGesture();
    setThresholdFromX (e.x);
}

void LevelBar::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        setThresholdFromX (e.x);
}

void LevelBar::mouseUp (const juce::MouseEvent&)
{
    if (! dragging)
        return;

    dragging = false;
    threshold->endChangeGesture();
}

} // namespace bmo::vcomp

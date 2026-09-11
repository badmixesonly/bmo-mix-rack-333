#pragma once

#include "core/ui/Controls.h"
#include "core/ui/Tokens.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace bmo::deq
{

//==============================================================================
/** A choice parameter as a row of switches, one lit.

    The suite has a switch for a bool (ui::SwitchButton) and a dial for a
    stepped choice (ui::ConcentricBand), and nothing for a short choice that
    reads best as words side by side -- STEREO / MID / SIDE, ABOVE / BELOW. So
    these are plain juce::ToggleButtons with the same tint SwitchButton sets,
    drawn by the same look and feel: they are the suite's switches in every
    pixel, and only the wiring is new. A ParameterAttachment carries the
    choice, so a click is one gesture and host automation moves the lit one. */
class ChoiceRow final : public juce::Component
{
public:
    ChoiceRow (juce::RangedAudioParameter& parameter, juce::StringArray labels, juce::Colour tint,
               bool vertical = false)
        : stacked (vertical),
          attachment (parameter, [this] (float v) { show ((int) std::lround (v)); })
    {
        for (int i = 0; i < labels.size(); ++i)
        {
            auto b = std::make_unique<juce::ToggleButton> (labels[i]);
            b->setColour (juce::ToggleButton::tickColourId, tint);
            b->setClickingTogglesState (false);
            b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (*b);
            buttons.push_back (std::move (b));
        }

        attachment.sendInitialUpdate();
    }

    void resized() override
    {
        auto area = getLocalBounds();
        const auto n = (int) buttons.size();
        const auto gap = ui::Tokens::switchGap;

        for (int i = 0; i < n; ++i)
        {
            if (stacked)
            {
                const auto h = (area.getHeight() - gap * (n - 1 - i)) / (n - i);
                buttons[(size_t) i]->setBounds (area.removeFromTop (h));
                area.removeFromTop (gap);
            }
            else
            {
                const auto w = (area.getWidth() - gap * (n - 1 - i)) / (n - i);
                buttons[(size_t) i]->setBounds (area.removeFromLeft (w));
                area.removeFromLeft (gap);
            }
        }
    }

    const std::vector<std::unique_ptr<juce::ToggleButton>>& getButtons() const noexcept { return buttons; }

    /** Stacked top to bottom, or side by side. */
    void setVertical (bool shouldStack)
    {
        stacked = shouldStack;
        resized();
    }

    /** New words for the same choices -- a wider panel can afford the long
        forms. Must be as many as there are choices. */
    void setLabels (const juce::StringArray& labels)
    {
        jassert (labels.size() == (int) buttons.size());
        for (int i = 0; i < juce::jmin (labels.size(), (int) buttons.size()); ++i)
            buttons[(size_t) i]->setButtonText (labels[i]);
    }

private:
    void show (int index)
    {
        for (int i = 0; i < (int) buttons.size(); ++i)
            buttons[(size_t) i]->setToggleState (i == index, juce::dontSendNotification);
    }

    bool stacked;
    std::vector<std::unique_ptr<juce::ToggleButton>> buttons;
    juce::ParameterAttachment attachment;
};

//==============================================================================
/** The twelve band tabs. Selecting one is panel state, not a parameter -- it
    decides which band the controls under it are bound to, and nothing more.

    A tab shows whether its band is on (switch fill against the dark well of
    an off one) and whether its dynamics are (a dot in the gain-reduction
    azure), so which bands are doing something reads without selecting any. */
class BandTabs final : public juce::Component,
                       private juce::Timer
{
public:
    BandTabs (int count, std::function<bool (int)> isOn, std::function<bool (int)> isDynamic,
              std::function<void (int)> choose)
        : bands (count), on (std::move (isOn)), dynamic (std::move (isDynamic)), onChoose (std::move (choose))
    {
        startTimerHz (10);
    }

    void setSelected (int band) { selected = band; repaint(); }
    void setRows (int r)        { rows = juce::jmax (1, r); repaint(); }

    /** Space between tabs: 8 on the full panel (the switch gap), 7 on the
        compact one, where six tabs and five gaps have to come out of 300. */
    void setGap (int g)         { gap = juce::jmax (0, g); repaint(); }

    /** How wide the strip is for tabs `tabWidth` across, in `rows` rows. */
    int widthFor (int tabWidth) const
    {
        const auto perRow = (bands + rows - 1) / rows;
        return perRow * tabWidth + (perRow - 1) * gap;
    }

    juce::Rectangle<int> tabBounds (int band) const
    {
        const auto perRow = (bands + rows - 1) / rows;
        const auto row = band / perRow, col = band % perRow;
        const auto w = (getWidth() - gap * (perRow - 1)) / perRow;
        const auto h = (getHeight() - gap * (rows - 1)) / rows;
        return { col * (w + gap), row * (h + gap), w, h };
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = ui::tokens();

        for (int b = 0; b < bands; ++b)
        {
            const auto r = tabBounds (b).toFloat();
            const auto isSel = b == selected, isOn = on && on (b);

            g.setColour (isSel ? accent : (isOn ? t.switchOff : t.well));
            g.fillRoundedRectangle (r, ui::Tokens::corner);

            if (! isSel && ! isOn)
            {
                g.setColour (t.hairline);
                g.drawRoundedRectangle (r.reduced (0.5f), ui::Tokens::corner, 1.0f);
            }

            const auto ink = isSel ? ui::onAccentOf (accent) : (isOn ? t.text1 : t.text2);
            ui::drawLabel (g, juce::String (b + 1), r, juce::Justification::centred, ui::labelFont (12.0f, true), ink);

            if (dynamic && dynamic (b))
            {
                g.setColour (isSel ? ink : t.meterGr);
                g.fillEllipse (r.getRight() - 9.0f, r.getY() + 4.0f, 5.0f, 5.0f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int b = 0; b < bands; ++b)
            if (tabBounds (b).contains (e.getPosition()))
            {
                selected = b;
                repaint();
                if (onChoose) onChoose (b);
                return;
            }
    }

    juce::Colour accent = ui::tokens().accent;

private:
    void timerCallback() override { repaint(); }

    int bands, rows = 1, selected = 0, gap = 7;
    std::function<bool (int)> on, dynamic;
    std::function<void (int)> onChoose;
};

//==============================================================================
/** Gain reduction as a bar that grows down from the top, in the suite's
    meterGr. The module's deepest band, from the context's gainReductionDb --
    the panel has no path to a single band's figure, by design (a panel sees
    parameters and five meters, never the DSP).

    Mockups A and C: a 12 px bar, GR under it, and on the full panel the
    figure under that. The bar is centred in whatever width it is given, so
    the cell can be as wide as its words. */
class GainReductionBar final : public juce::Component,
                               private juce::Timer
{
public:
    explicit GainReductionBar (std::function<float()> source) : reduction (std::move (source))
    {
        startTimerHz (30);
    }

    void setShowsValue (bool s) { showsValue = s; repaint(); }

    /** The height a bar of `barHeight` needs with its words under it. */
    int heightFor (int barHeight) const { return barHeight + kCaptionRow + (showsValue ? kValueRow : 0); }

    void paint (juce::Graphics& g) override
    {
        const auto& t = ui::tokens();
        auto area = getLocalBounds().toFloat();
        const auto value = showsValue ? area.removeFromBottom ((float) kValueRow) : juce::Rectangle<float>();
        const auto caption = area.removeFromBottom ((float) kCaptionRow);
        const auto bar = area.withSizeKeepingCentre (kBarWidth, area.getHeight());

        g.setColour (t.well);
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (t.hairline.withAlpha (0.5f));
        g.drawRoundedRectangle (bar.reduced (0.5f), 2.0f, 1.0f);

        const auto depth = juce::jlimit (0.0f, 1.0f, shown / kRangeDb);
        g.setColour (t.meterGr);
        g.fillRect (bar.reduced (2.0f).removeFromTop ((bar.getHeight() - 4.0f) * depth));

        ui::drawLabel (g, "GR", caption, juce::Justification::centredBottom, ui::labelFont (11.0f), t.text1);

        // Tenths, and a real minus: the bar says how much at a glance, this
        // says it exactly. Blank at rest rather than "-0.0".
        if (showsValue && shown >= 0.05f)
            ui::drawLabel (g, juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) + juce::String (shown, 1), value,
                           juce::Justification::centredTop, ui::captionFont (11.0f), t.text2);
    }

private:
    void timerCallback() override
    {
        const auto now = reduction ? reduction() : 0.0f;
        shown = now > shown ? now : shown + 0.25f * (now - shown);
        repaint();
    }

    static constexpr float kRangeDb = 24.0f, kBarWidth = 12.0f;
    static constexpr int kCaptionRow = 16, kValueRow = 14;
    std::function<float()> reduction;
    float shown = 0.0f;
    bool showsValue = false;
};

//==============================================================================
/** The band's shape as the suite's stepped selector: a ui::ConcentricBand
    with no gain -- BMO EQ's cut-filter dial -- legended BELL, LS, HS, LC, HC,
    with SHAPE under it. Mockups A and C drew it so; the first build used a
    row of five switches instead, which is the thing Frosty called out.

    The dial is centred in the space above the caption and is as big as the
    shorter side of that space allows, so the cell is made wider than it is
    tall to give the legend's diagonals room. */
class ShapeDial final : public juce::Component
{
public:
    ShapeDial (juce::RangedAudioParameter& shape, const ParamSpec& spec, juce::Colour accent)
        : dial (shape, spec, nullptr, accent)
    {
        dial.setLegend ({ "BELL", "LS", "HS", "LC", "HC" });
        addAndMakeVisible (dial);
        setName ("SHAPE");
    }

    void setCaptionSize (float points) { captionSize = points; resized(); repaint(); }

    /** The row under the dial that SHAPE is drawn in. */
    int captionRow() const { return juce::roundToInt (captionSize * 1.2f) + 4; }

    /** How far SHAPE or the widest legend label runs past its box; <= 0 fits. */
    float captionOverflow() const
    {
        return juce::jmax (juce::GlyphArrangement::getStringWidth (ui::captionFont (captionSize), "SHAPE") - (float) getWidth(),
                           dial.legendOverflow());
    }

    ui::ConcentricBand& getDial() noexcept { return dial; }

    void resized() override
    {
        dial.setBounds (getLocalBounds().withTrimmedBottom (captionRow()));
    }

    void paint (juce::Graphics& g) override
    {
        ui::drawLabel (g, "SHAPE", getLocalBounds().removeFromBottom (captionRow()).toFloat(),
                       juce::Justification::centred, ui::captionFont (captionSize), ui::tokens().text1);
    }

private:
    ui::ConcentricBand dial;
    float captionSize = 11.0f;
};

} // namespace bmo::deq

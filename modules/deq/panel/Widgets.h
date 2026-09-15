#pragma once

#include "core/ui/Controls.h"
#include "core/ui/Tokens.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace bmo::deq
{

//==============================================================================
/** A choice parameter as a row of switches, one lit -- or none.

    The suite has a switch for a bool (ui::SwitchButton) and a dial for a
    stepped choice (ui::ConcentricBand), and nothing for a short choice that
    reads best as words side by side. So these are plain juce::ToggleButtons
    with the same tint SwitchButton sets, drawn by the same look and feel: they
    are the suite's switches in every pixel, and only the wiring is new. A
    ParameterAttachment carries the choice, so a click is one gesture and host
    automation moves the lit one.

    **`implicitChoice` is the option that gets no button of its own.** Pass -1
    and every choice has one, which is the plain behaviour. Pass an index and
    that choice is drawn as *nothing lit*, and clicking a lit button returns to
    it.

    BMO DEQ's placement is why (Frosty, 2026-09-15). STEREO / MID / SIDE read as
    three modes to pick between, and STEREO is not a mode -- it is what a band
    does when you have not asked for anything, on a mono source as much as a
    stereo one. A button for it invites the question "which of these three am I
    in", and the honest answer is that two of them are the special cases. So the
    panel offers MID and SIDE, and neither lit is the default behaviour. */
class ChoiceRow final : public juce::Component
{
public:
    ChoiceRow (juce::RangedAudioParameter& parameter, juce::StringArray labels, juce::Colour tint,
               bool vertical = false, int implicitChoice = -1)
        : stacked (vertical), implicit (implicitChoice),
          attachment (parameter, [this] (float v) { show ((int) std::lround (v)); })
    {
        for (int i = 0; i < labels.size(); ++i)
        {
            if (i == implicit)
                continue;

            auto b = std::make_unique<juce::ToggleButton> (labels[i]);
            b->setColour (juce::ToggleButton::tickColourId, tint);
            b->setClickingTogglesState (false);

            // Clicking the lit one goes back to the implicit choice, so the
            // default is reachable without a button for it. With no implicit
            // choice this is a plain radio set and a lit button ignores itself.
            b->onClick = [this, i]
            {
                const auto going = (current == i && implicit >= 0) ? implicit : i;
                attachment.setValueAsCompleteGesture ((float) going);
            };

            addAndMakeVisible (*b);
            buttons.push_back (std::move (b));
            choices.push_back (i);
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
        forms. Indexed by **choice**, including any that has no button. */
    void setLabels (const juce::StringArray& labels)
    {
        for (size_t b = 0; b < buttons.size(); ++b)
            if (choices[b] < labels.size())
                buttons[b]->setButtonText (labels[choices[b]]);
    }

private:
    void show (int index)
    {
        current = index;

        for (size_t b = 0; b < buttons.size(); ++b)
            buttons[b]->setToggleState (choices[b] == index, juce::dontSendNotification);
    }

    bool stacked;
    int implicit = -1;
    int current = -1;
    std::vector<std::unique_ptr<juce::ToggleButton>> buttons;
    std::vector<int> choices;   ///< which choice each button writes
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
    /** `toggle` switches a band on or off. It is the **only** way to do that
        from the tabs, and it is on the double-click because a single click
        already selects: a tab that switched a band off when you were only
        trying to look at it would be unusable.

        The band's ON switch used to live in the strip below and was dropped on
        2026-09-15 -- it sat inside the placement group and read as a fourth
        placement mode. This gesture and the one on the curve's nodes replace
        it. */
    BandTabs (int count, std::function<bool (int)> isOn, std::function<bool (int)> isDynamic,
              std::function<void (int)> choose, std::function<void (int)> toggle = {})
        : bands (count), on (std::move (isOn)), dynamic (std::move (isDynamic)),
          onChoose (std::move (choose)), onToggle (std::move (toggle))
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

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        // The first click of the pair has already selected this band, so a
        // double-click is always "the band I am looking at, on or off".
        for (int b = 0; b < bands; ++b)
            if (tabBounds (b).contains (e.getPosition()))
            {
                if (onToggle) onToggle (b);
                repaint();
                return;
            }
    }

    juce::Colour accent = ui::tokens().accent;

private:
    void timerCallback() override { repaint(); }

    int bands, rows = 1, selected = 0, gap = 7;
    std::function<bool (int)> on, dynamic;
    std::function<void (int)> onChoose, onToggle;
};

//==============================================================================
/** What the dynamics are doing to the gain, as a bar read from both ends.

    The module's deepest band, from the context's gainReductionDb -- the panel
    has no path to a single band's figure, by design (a panel sees parameters
    and five meters, never the DSP).

    **Gain taken away grows down from the top. Gain added grows up from the
    bottom.** Frosty, 2026-09-15, after the UI pass measured that an upward
    band drew exactly the same empty bar as a band with its dynamics switched
    off -- the source was `max (0, ...)` and the meter could not tell the two
    apart. See `DspCore::currentGainReductionDb`.

    Both directions run the **full** height for the full range, so they share
    the track rather than splitting it and neither costs the other any
    resolution. That is the reason this is not a centre-out bar, which was the
    other candidate rendered: centre-out halves both. It works because only one
    of them can be non-zero at a time -- the source is one band's offset, not a
    sum -- so the two fills can never collide, and which end a fill starts from
    *is* the sign.

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

    /** The readout for a signed figure: a real minus for gain taken away, a
        plus for gain added. Tenths, so the bar says how much at a glance and
        this says it exactly. */
    static juce::String valueText (float db)
    {
        return juce::String (db >= 0.0f ? juce::CharPointer_UTF8 ("\xe2\x88\x92")
                                        : juce::CharPointer_UTF8 ("+"))
             + juce::String (std::abs (db), 1);
    }

    /** The widest readout this bar can ever print.

        Fixed rather than sampled, because a cell sized to whatever the meter
        happened to read when somebody looked at it is a cell that clips later.
        `DspCore::currentGainReductionDb` returns the deepest *single* band
        rather than a sum, and a band's offset is bounded by its own range
        parameter, which runs to 24 dB either way -- so one of these two is the
        widest string, and it cannot grow without the schema changing.

        Both ends are measured rather than one, because the minus and the plus
        are different glyphs and which is wider is a property of the caption
        face. Assuming would be how the next version of "-12." gets written. */
    static juce::String widestValue()
    {
        const auto cut = valueText (kRangeDb), added = valueText (-kRangeDb);
        const auto font = ui::captionFont (kValueSize);

        return juce::GlyphArrangement::getStringWidth (font, added)
             > juce::GlyphArrangement::getStringWidth (font, cut) ? added : cut;
    }

    /** How far the widest word this bar draws runs past its own box; <= 0
        fits. The caption is drawn always, the readout only on the full panel.

        The readout clipped to "-12." on the 600 from its first build until the
        2026-09-15 UI pass, and no assertion could have caught it: the suite's
        text-fits checks walk PlainKnob captions and switch labels, and this is
        painted by hand inside a Component of its own. */
    float valueOverflow() const
    {
        auto widest = juce::GlyphArrangement::getStringWidth (ui::labelFont (kCaptionSize), "GR");

        if (showsValue)
            widest = juce::jmax (widest, juce::GlyphArrangement::getStringWidth (ui::captionFont (kValueSize), widestValue()));

        return widest - (float) getWidth();
    }

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

        // Down from the top for gain taken away, up from the bottom for gain
        // added, both over the whole track. One fill or the other, never both.
        const auto inner = bar.reduced (2.0f);
        const auto depth = juce::jlimit (-1.0f, 1.0f, shown / kRangeDb);

        g.setColour (t.meterGr);

        if (depth > 0.0f)
            g.fillRect (inner.withHeight (inner.getHeight() * depth));
        else if (depth < 0.0f)
            g.fillRect (inner.withTrimmedTop (inner.getHeight() * (1.0f + depth)));

        ui::drawLabel (g, "GR", caption, juce::Justification::centredBottom, ui::labelFont (kCaptionSize), t.text1);

        // Blank at rest rather than "-0.0", and the sign carries which end the
        // fill is growing from -- so the figure and the picture agree even
        // when the bar is too short to read a direction off.
        if (showsValue && std::abs (shown) >= 0.05f)
            ui::drawLabel (g, valueText (shown), value,
                           juce::Justification::centredTop, ui::captionFont (kValueSize), t.text2);
    }

private:
    void timerCallback() override
    {
        // Snap to a bigger move, ease back from it -- by magnitude now that
        // the value is signed, so a deep boost holds the way a deep cut does
        // and does not get overtaken by a shallower cut of the opposite sign.
        const auto now = reduction ? reduction() : 0.0f;
        shown = std::abs (now) > std::abs (shown) ? now : shown + 0.25f * (now - shown);
        repaint();
    }

    static constexpr float kRangeDb = 24.0f, kBarWidth = 12.0f;
    static constexpr float kCaptionSize = 11.0f, kValueSize = 11.0f;
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

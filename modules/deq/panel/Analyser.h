#pragma once

#include "core/dsp/AnalyserTap.h"
#include "core/ui/Tokens.h"
#include "core/ui/Fonts.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace bmo::deq
{

//==============================================================================
/** The spectrum drawn behind the curve, from the module's post-EQ tap.

    Everything here runs on the message thread, on the view's own timer. The
    audio thread's whole involvement is `AnalyserTap::write`, which is a copy
    and a relaxed store and happens only while `setEnabled(true)` -- see
    `core/dsp/AnalyserTap.h`, which is emphatic that this path cannot change the
    sound or add latency, and why.

    **It measures a different quantity from the curve it sits behind**, and the
    two share a well. The curve's axis is EQ gain, plus or minus `kSpanDb`
    around a centre line; this is signal level, `kFloorDb` at the bottom of the
    well to `kCeilingDb` at the top. They are not comparable and are not drawn
    as though they were: the spectrum is a filled shape at low alpha sitting
    behind the grid, and the curve is a line in front of it.

    Post-EQ, one tap drawn, two in the plumbing (`spec/decisions.md`,
    2026-09-12). Post reads as pre when the bands are off, which covers "what am
    I working on" at the cost of having to stop processing to see it.
*/
class Analyser
{
public:
    /** The five options, in the order the chooser offers them. `accent` is the
        module's own colour and has no token: it is the one option whose value
        depends on which module is asking, which is also why it is named for
        the role rather than for the teal it happens to be on BMO DEQ. */
    enum class Tint { accent, orange, gold, pink, neutral, count };

    static constexpr const char* kTintNames[] { "Accent", "Orange", "Gold", "Pink", "Neutral" };

    /** The default, Frosty 2026-09-12: it collides with nothing, never competes
        with the curve in front of it, and a panel that ships in someone else's
        colour has made a claim on their behalf. */
    static constexpr Tint kDefaultTint = Tint::neutral;

    //== What it is pointed at =================================================

    /** The tap, or null for a module with none. Enabling is what makes the
        audio thread write at all, so an analyser that is switched off, or a
        panel that has been closed, costs the DSP nothing. */
    void setTap (AnalyserTap* t) noexcept
    {
        if (tap == t)
            return;

        if (tap != nullptr)
            tap->setEnabled (false);

        tap = t;

        if (tap != nullptr)
            tap->setEnabled (on);
    }

    void setEnabled (bool shouldBe) noexcept
    {
        on = shouldBe;

        if (tap != nullptr)
            tap->setEnabled (on);

        if (! on)
            magnitudes.fill (kFloorDb);
    }

    bool isEnabled() const noexcept { return on; }

    void setTint (Tint t) noexcept { chosen = t; }
    Tint tint() const noexcept { return chosen; }

    juce::Colour colourFor (juce::Colour moduleAccent) const
    {
        const auto& k = ui::tokens();

        switch (chosen)
        {
            case Tint::orange:  return k.analyserOrange;
            case Tint::gold:    return k.analyserGold;
            case Tint::pink:    return k.analyserPink;
            case Tint::neutral: return k.analyserNeutral;
            case Tint::accent:
            case Tint::count:
            default:            return moduleAccent;
        }
    }

    //== The frame =============================================================

    /** Pulls the newest frame and folds it into the smoothed magnitudes.
        Returns whether anything moved enough to be worth a repaint.

        Message thread. `rate` is the module's real sample rate; 0 or less
        means the host has not prepared it and there is nothing to draw. */
    bool update (double rate)
    {
        if (! on || tap == nullptr || rate <= 0.0)
            return false;

        std::array<float, kFftSize> frame {};

        // Fewer samples than a frame means the audio thread has not filled the
        // tap yet -- a plugin that has just been enabled. Draw nothing rather
        // than a frame of zeros, which would read as a real silent signal.
        if (tap->read (frame.data(), kFftSize) < kFftSize)
            return false;

        window.multiplyWithWindowingTable (frame.data(), kFftSize);

        // performFrequencyOnlyForwardTransform wants 2*size and writes
        // magnitudes into the first half.
        std::array<float, kFftSize * 2> work {};
        std::copy (frame.begin(), frame.end(), work.begin());
        fft.performFrequencyOnlyForwardTransform (work.data());

        // A Hann window sums to half its length, and the transform is
        // unnormalised: divide by that, so a full-scale sine reads 0 dBFS
        // rather than "some number that depends on the frame size".
        const auto scale = 2.0f / (float) kFftSize;

        auto largestMove = 0.0f;

        for (int bin = 0; bin < kBins; ++bin)
        {
            const auto db = juce::Decibels::gainToDecibels (work[(size_t) bin] * scale, kFloorDb);
            auto& shown = magnitudes[(size_t) bin];

            // Fast up, slow down: the same shape as every meter in the suite,
            // for the same reason -- a peak that appears and vanishes between
            // two frames should still be visible in one of them.
            const auto next = db > shown ? db : shown + kFall * (db - shown);

            largestMove = juce::jmax (largestMove, std::abs (next - shown));
            shown = next;
        }

        // A tenth of a dB anywhere. Below that the shape is settled and a
        // repaint would redraw the same picture -- which matters because this
        // runs at the view's frame rate whenever a panel is open.
        const auto moved = largestMove > 0.1f;

        binHz = rate / (double) kFftSize;
        return moved;
    }

    /** The spectrum as a closed shape over `plot`, ready to fill.

        Built per **pixel column** rather than per bin, and that is not an
        optimisation. A log frequency axis spreads the low bins further apart
        than a pixel and packs the high ones tighter than one: iterating bins
        would leave gaps under about 200 Hz and alias badly above 5 kHz. Each
        column takes the loudest bin that lands in it, which is what a peak
        display should show and what every analyser worth reading does.

        `xFor` maps Hz to the view's own x, so the spectrum cannot drift from
        the grid it is drawn against -- it is the same function the axis labels
        and the nodes go through. */
    template <typename XForHz>
    void buildPath (juce::Path& out, juce::Rectangle<float> plot, XForHz&& xFor) const
    {
        out.clear();

        if (! on || binHz <= 0.0)
            return;

        const auto left = plot.getX(), right = plot.getRight();
        const auto columns = juce::jmax (1, (int) std::ceil (right - left));

        auto started = false;
        auto nextBin = 1;   // bin 0 is DC and has no place on a log axis

        for (int c = 0; c <= columns; ++c)
        {
            const auto x = juce::jmin (right, left + (float) c);

            // Every bin from where the last column finished up to this one.
            // The advance is monotonic, so each bin is looked at once across
            // the whole sweep rather than once per column.
            auto loudest = kFloorDb;
            auto any = false;

            while (nextBin < kBins)
            {
                const auto hz = (double) nextBin * binHz;

                if (xFor (hz) > x && any)
                    break;

                if (xFor (hz) > x && ! any)
                {
                    // No bin lands in this column: it is wider in Hz than the
                    // spacing allows, which happens at the very bottom of the
                    // axis. Take the next one anyway so the shape stays
                    // continuous instead of dropping to the floor.
                    loudest = magnitudes[(size_t) nextBin];
                    any = true;
                    break;
                }

                loudest = juce::jmax (loudest, magnitudes[(size_t) nextBin]);
                any = true;
                ++nextBin;
            }

            if (! any)
                break;

            const auto norm = juce::jlimit (0.0f, 1.0f, (loudest - kFloorDb) / (kCeilingDb - kFloorDb));
            const auto y = plot.getBottom() - norm * plot.getHeight();

            if (! started)
            {
                out.startNewSubPath (x, plot.getBottom());
                started = true;
            }

            out.lineTo (x, y);
        }

        if (started)
        {
            out.lineTo (right, plot.getBottom());
            out.closeSubPath();
        }
    }

    /** dBFS at the foot and the head of the well. The floor is low enough that
        a quiet mix still has shape near the bottom, and the ceiling is 0 rather
        than a headroom figure because a sample peak is the one level a reader
        can check against something. */
    static constexpr float kFloorDb = -96.0f, kCeilingDb = 0.0f;

    ~Analyser()
    {
        // A panel going away has to stop the audio thread writing. This is the
        // other half of setTap's contract and the reason a closed editor is
        // free rather than nearly free.
        if (tap != nullptr)
            tap->setEnabled (false);
    }

private:
    static constexpr int kFftOrder = 12;
    static constexpr int kFftSize  = 1 << kFftOrder;    ///< 4096: 11.7 Hz bins at 48 k
    static constexpr int kBins     = kFftSize / 2;
    static constexpr float kFall   = 0.18f;

    AnalyserTap* tap = nullptr;
    bool on = false;
    Tint chosen = kDefaultTint;

    juce::dsp::FFT fft { kFftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kFftSize, juce::dsp::WindowingFunction<float>::hann };

    std::array<float, kBins> magnitudes = [] { std::array<float, kBins> a {}; a.fill (kFloorDb); return a; }();
    double binHz = 0.0;
};

//==============================================================================
/** The analyser's own toggle, and the five-option chooser behind it.

    **It lives in the well, not on a switch row**, and that is a rule rather
    than a preference. `DeqPanel`'s own documentation says every control on this
    panel changes the sound -- it is the stated reason the compact/expanded
    switch is on the host's bar and never here. The analyser changes nothing
    anyone hears, so putting it among DEQ and AUTO would quietly make that
    sentence false. A display control belongs on the display.

    Small, low-contrast and in the corner, because it is furniture: the well is
    for the curve. Left-click toggles; **right-click chooses the colour**
    (`spec/decisions.md`, 2026-09-12: "five options do not want five buttons on
    a panel this busy"). */
class AnalyserButton final : public juce::Component
{
public:
    AnalyserButton (Analyser& a, juce::Colour moduleAccent, std::function<void()> onChange)
        : analyser (a), accent (moduleAccent), changed (std::move (onChange))
    {
        setName ("SPEC");
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = ui::tokens();
        const auto r = getLocalBounds().toFloat();
        const auto lit = analyser.isEnabled();

        g.setColour (lit ? analyser.colourFor (accent).withAlpha (0.85f) : t.hairline.withAlpha (0.5f));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);

        ui::drawLabel (g, "SPEC", r, juce::Justification::centred, ui::captionFont (9.0f),
                       lit ? analyser.colourFor (accent) : t.text2);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            showChooser();
            return;
        }

        analyser.setEnabled (! analyser.isEnabled());
        repaint();

        if (changed)
            changed();
    }

private:
    void showChooser()
    {
        juce::PopupMenu menu;

        for (int i = 0; i < (int) Analyser::Tint::count; ++i)
            menu.addItem (i + 1, Analyser::kTintNames[i], true, (int) analyser.tint() == i);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [this] (int choice)
                            {
                                if (choice <= 0)
                                    return;

                                analyser.setTint ((Analyser::Tint) (choice - 1));
                                repaint();

                                if (changed)
                                    changed();
                            });
    }

    Analyser& analyser;
    juce::Colour accent;
    std::function<void()> changed;
};

} // namespace bmo::deq

#pragma once

#include "core/ui/Controls.h"
#include "core/ui/Fonts.h"
#include "core/ui/Tokens.h"

namespace bmo::vcomp
{

//==============================================================================
/** One horizontal bar meter: a caption, a well, and a fill that grows from one
    end.

    **Module-local on purpose.** A horizontal dBFS bar is generic enough to
    belong in core/ui one day, and it will go there the moment a second module
    wants one -- the same rule modules/AGENTS.md applies to DynamicsMeter's
    scale ("the point at which to lift ScalePoint out into the caller -- not
    before"). Lifting it now would mean designing for a caller that does not
    exist. BMO DEQ's ResponseView is the precedent for a panel owning its own
    view.

    **Why this and not DynamicsMeter.** BMO Opto's needle VU is a period
    instrument: it reads average level with VU ballistics on a scale borrowed
    from a 1940s volume indicator, which is right for a module modelling an
    LA-2A and wrong for this one. A modern compressor is judged on peaks in
    dBFS, and the three readings a compressor user actually wants -- what went
    in, what it took off, what came out -- are wanted *at the same time*, which
    a single needle behind a three-way switch cannot do. Three bars show all
    three at once and cost less height than the needle did.

    **Ballistics are the suite's**, taken from OutputMeter: a peak meter jumps
    to a new high immediately and falls back at 0.16 of the distance per tick,
    so it is readable rather than twitchy. Not re-derived here -- if that
    number changes there, change it here. */
class LevelBar final : public juce::Component
{
public:
    /** Which end the fill grows from. Reduction grows leftward from zero on
        the right, which is how every hardware and plugin gain-reduction meter
        has ever read: the bar hangs down from unity rather than building up
        from silence, because that is what the compressor is doing. */
    enum class Grow { rightward, leftward };

    /** `source` returns the reading already in dB -- dBFS for a level, dB of
        reduction for GR. The caller adapts, because the caller is the one that
        knows which of ModuleContext's sources is linear. */
    LevelBar (juce::String caption, Grow, float minDb, float maxDb,
              std::function<float()> source);

    void paint (juce::Graphics&) override;

    /** Pulls a new reading and repaints. Driven from the panel's timer rather
        than one of its own: three bars ticking on three timers would be three
        repaints a frame for one panel. */
    void refresh();

    /** Colour the fill in one colour at every level instead of the suite's
        low/high/clip zones. For the GR bar, where "hot" is not a warning --
        a compressor working hard is not a compressor in trouble, and painting
        24 dB of reduction in the clip red would say it was. */
    void setFlatColour (juce::Colour);

    /** Draw a draggable threshold handle on this bar, bound to `param`.

        This is the gate, and it is the only control on the panel that is not a
        knob or a switch. A gate threshold is the one parameter a user sets by
        looking at the level they are setting it against, so putting it *on*
        the level is the whole point -- a knob would make them read a number
        and translate it into what they can see happening.

        Held as a raw pointer to the parameter, like the attachment classes do;
        the ParamSet outlives every panel. */
    void attachThreshold (juce::RangedAudioParameter&, juce::Colour handleColour);

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    /** The well, in this component's coordinates -- what the layout test reads
        to check the three bars line up, and what the handle maths uses. */
    juce::Rectangle<int> wellBounds() const;

    static constexpr int kCaptionWidth = 38;
    static constexpr int kBarHeight    = 14;

private:
    /** 0..1 along the well for a dB reading, before Grow is applied. */
    float normalised (float db) const;

    /** Sets the threshold parameter from a mouse x, through the standard
        gesture triplet. */
    void setThresholdFromX (int x);

    juce::String caption;
    Grow grow;
    float minDb, maxDb;
    std::function<float()> source;

    juce::Colour flat;
    bool useFlatColour = false;

    juce::RangedAudioParameter* threshold = nullptr;
    juce::Colour handleColour;
    bool dragging = false;

    float displayed = 0.0f;   ///< linear 0..1 along the well, ballistically smoothed

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelBar)
};

} // namespace bmo::vcomp

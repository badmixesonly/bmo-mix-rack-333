#pragma once

#include "LookAndFeel.h"
#include "core/state/ParamSpec.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <limits>
#include <vector>

namespace bmo::ui
{

/** A knob with its name underneath and nothing else: no number, only a plus
    one side and, where it cuts, a minus the other. */
class PlainKnob final : public juce::Component
{
public:
    /** Leave `captionColour` alone and the caption is derived from `accent`
        against the current plate -- `accentTextOn`, so it reads at 4.5:1 and
        it is the module's own colour.

        Until 0.2.2 it defaulted to the shared track azure, which put every
        caption in the suite at 1.95:1 and, worse, put INPUT and DRIVE in blue
        underneath an orange knob. BMO Opto had already worked around both by
        hardcoding its own hex. Pass a colour here only to override that. */
    PlainKnob (juce::RangedAudioParameter&, const juce::String& caption,
               Knob::Style style = Knob::Style::utility, float faceScale = 0.5f,
               juce::Colour accent = tokens().accent,
               juce::Colour captionColour = {});

    void paint (juce::Graphics&) override;
    void resized() override;

    void setKnobEnabled (bool);

    /** Re-colours the knob and, unless a caption colour was passed in, its
        caption with it. For a module whose colour depends on its own state --
        BMO Opto runs greyscale in Tele and lavender in Stressed -- rather than
        on which module it is. */
    void setAccent (juce::Colour);

    /** Caps how wide the knob itself may draw, leaving the rest of the
        component's width to the caption underneath.

        Without this a long caption can only be given room by widening the
        whole control, which widens the knob with it. BMO Opto's "MAKEUP" is
        the case that forced it: at the 92 px the knob wants, the caption
        clipped to "MAKEU". The knob is laid out square and centred, so the
        drawn radius -- jmin(width, height) -- is unchanged by a wider
        component once the side is capped. */
    void setKnobSide (int maxSide);

private:
    /** Room under the knob for its name. */
    static constexpr int kCaptionRow = 22;

    juce::String caption;
    juce::Colour captionColour;   ///< transparent means "derive from accentColour"
    juce::Colour accentColour;
    int knobSide = std::numeric_limits<int>::max();
    Knob knob;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlainKnob)
};

//==============================================================================
/** A band, or a filter.

    With a gain parameter it is a band: the selector is the white ring,
    legended with its switch positions, and the cut and boost sits inside it.
    Grab the ring for the selector, the middle for gain. Without one it is a
    filter: a single knob with the same legend around it.

    The legend comes from the selector's spec, so it is right in the rack too,
    where the parameter object underneath is a generic slot parameter.
*/
class ConcentricBand final : public juce::Component
{
public:
    ConcentricBand (juce::RangedAudioParameter& selector, const ParamSpec& selectorSpec,
                    juce::RangedAudioParameter* gain, juce::Colour accent = tokens().accent);

    void paint (juce::Graphics&) override;
    void resized() override;

    void setRingEnabled (bool);

private:
    /** How much narrower the selector sweep is than the gain sweep, each
        side, in radians. */
    static constexpr float kLegendInset = 0.60f;

    /** Frequency legend type: size when selected, when not, and which face. */
    static constexpr float kPointSize         = 9.9f;
    static constexpr float kPointSizeIdle     = 9.0f;
    static constexpr bool  kPointUsesCaption  = false;

    /** The box a legend label is drawn into. */
    static constexpr float kLegendBoxWidth  = 38.0f;
    static constexpr float kLegendBoxHeight = 15.0f;

    /** Where the dial and its legend sit inside the cell.

        `shift` is how far down the whole assembly is nudged. It is zero for a
        band, whose legend is symmetric about the dial. A filter's is not: its
        positions run around a full circle with the last one blank, and that
        blank falls at the foot, so there is ink above the dial with no
        counterpart below it. Centring the dial in the cell then reads as the
        dial sitting high, because what the eye centres is the ink. The shift
        is half that imbalance, measured rather than guessed, so it follows if
        the number of positions or the radius ever changes. */
    struct Geometry { float ringRadius, textRadius; int shift; };
    Geometry geometry() const;

    Knob ring, centre;
    std::unique_ptr<juce::SliderParameterAttachment> ringAttachment, centreAttachment;

    juce::StringArray legend;
    juce::Colour accentColour;
    bool hasCentre = false, ringEnabled = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConcentricBand)
};

//==============================================================================
class SwitchButton final : public juce::Component
{
public:
    /** `tint` is what the switch lights up in, and it is required rather than
        defaulted. What a switch lights up in is not a free choice -- the table
        in modules/AGENTS.md gives it: the module's accent for a bypass or for
        mono, `polarity` for a polarity flip, `switchAlt` for anything else. A
        default argument here quietly said otherwise, and the colour it
        defaulted to was `switchOn`, a second name for BMO EQ's pink that no
        call site had used since before 0.2.2. */
    SwitchButton (juce::RangedAudioParameter&, const juce::String& text,
                  juce::Colour tint);

    void resized() override;
    void setSwitchEnabled (bool);

    /** What the switch lights up in. See PlainKnob::setAccent. */
    void setTint (juce::Colour);

    /** The label to use while engaged, instead of one derived from the fill.

        For a switch whose fill is the same in every module -- polarity, which
        is always white -- so the label is what says which module it belongs
        to. Pass the module's accent: it is stepped against the fill here, not
        against the plate, so it stays dark on a white switch whatever the
        plate underneath is doing. */
    void setActiveInkFrom (juce::Colour accent);

private:
    juce::ToggleButton button;
    std::unique_ptr<juce::ButtonParameterAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwitchButton)
};

//==============================================================================
/** Output meter, switchable between peak dBFS and VU by clicking it.

    VU is not a different scale on the same number: it is an RMS reading with
    slow ballistics, 0 VU at -18 dBFS, which is why it reads weight where a peak
    meter reads headroom.
*/
class OutputMeter final : public juce::Component,
                          private juce::Timer
{
public:
    OutputMeter (std::function<float()> peakSource, std::function<float()> rmsSource);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    std::function<float()> peak, rms;
    float displayed = 0.0f;
    bool  vuMode = false;

    static constexpr float kVuReference = -18.0f;
    static constexpr int   kBarWidth    = 14;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputMeter)
};

//==============================================================================
/** Input, output or gain-reduction, one at a time -- a horizontal needle VU
    meter with a printed scale, BMO Opto's centrepiece, and any dynamics
    module after it.

    Input and output read as VU, calibrated the same way as OutputMeter (0 VU
    = -18 dBFS, see its class comment), with the needle swept across a scale
    approximating a classic VU faceplate's non-linear spacing (compressed at
    the low end, spread out from 0 to +3) -- not derived from any one real
    meter's calibration data, just close enough to read as the genre. Gain
    reduction reads the module's own published figure directly, in dB, on a
    plain linear 0..kGrRangeDb scale.

    Mode is switched externally via setMode() -- the panel owns a row of
    labelled buttons for that (see modules/opto/panel/OptoPanel.cpp); this
    class used to cycle modes on click, which tested as unintuitive with
    nothing on screen to say what clicking would do. */
class DynamicsMeter final : public juce::Component,
                            private juce::Timer
{
public:
    enum class Mode { input, output, reduction };

    /** `hotColour` marks 0 VU and above -- a classic VU meter's red zone,
        but left up to the caller since a module's own theme may want
        something other than red there.

        The face the scale is printed on is `meterFace` and is not a parameter:
        it was a caller's choice while it was a raw hex in a panel, and the
        reason given was that the face and its ink together decide whether the
        meter can be read at all -- the first cut drew a #97ddff hot zone on
        `well` #d6d6d6, 1.02:1, invisible. A token settles that once for every
        module and lets a theme move it, which a constructor argument captured
        at build time could not. */
    DynamicsMeter (std::function<float()> inputRmsSource,
                   std::function<float()> outputRmsSource,
                   std::function<float()> gainReductionDbSource,
                   Mode initialMode = Mode::output,
                   juce::Colour accent = tokens().accent,
                   juce::Colour hotColour = tokens().meterClip);

    void paint (juce::Graphics&) override;

    void setMode (Mode) noexcept;
    Mode getMode() const noexcept { return mode; }

    /** Bezel and hot-zone colour, for a module whose palette depends on its
        own state rather than on which module it is. The face stays as
        constructed: a needle meter needs a dark one whatever the mode. */
    void setColours (juce::Colour accent, juce::Colour hot) noexcept;

private:
    void timerCallback() override;

    /** One control point on the printed scale: a value in the mode's own
        unit (dB relative to the VU reference, or dB of gain reduction),
        where it sits across the needle's sweep, 0..1, and whether it is
        numbered.

        Not every tick is numbered, because a VU scale crowds hard from -3
        upwards and printing all of it there is what made the numbers
        unreadable. Hardware faceplates do the same: every tick is struck,
        only the round ones are inked. */
    struct ScalePoint { float value; float fraction; bool numbered = true; };

    /** A std::vector rather than a juce::Array because the scales are written
        out as a braced list of braced pairs, and juce::Array's initialiser-list
        constructor is a template whose element type cannot be deduced from
        nested braces -- std::vector's is not, so `{ { -20.0f, 0.0f }, ... }`
        just works. */
    float fractionFor (float value, const std::vector<ScalePoint>& scale) const noexcept;

    std::function<float()> inputRms, outputRms, gainReductionDb;
    Mode mode;
    float displayed = 0.0f;

    static constexpr float kVuReference = -18.0f;
    static constexpr float kGrRangeDb   = 24.0f;
    juce::Colour accentColour, hotColour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicsMeter)
};

/** "1.6 kHz" -> "1k6", "360 Hz" -> "360", "Off" -> "OFF". A legend has to fit
    around a knob, and this is how the hardware prints it. */
juce::String compactFrequency (const juce::String& text);

} // namespace bmo::ui

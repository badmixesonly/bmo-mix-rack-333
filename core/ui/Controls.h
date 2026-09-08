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
    /** `captionColour` defaults to the shared track colour every other
        module's caption uses; BMO Opto passes its own accent (with added
        contrast) so COMP/MAKEUP read in the module's own colour rather than
        the suite-wide blue -- see modules/opto/panel/OptoPanel.cpp. */
    PlainKnob (juce::RangedAudioParameter&, const juce::String& caption,
               Knob::Style style = Knob::Style::utility, float faceScale = 0.5f,
               juce::Colour accent = tokens().accent,
               juce::Colour captionColour = tokens().track);

    void paint (juce::Graphics&) override;
    void resized() override;

    void setKnobEnabled (bool);

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
    juce::Colour captionColour;
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

    Knob ring, centre;
    std::unique_ptr<juce::SliderParameterAttachment> ringAttachment, centreAttachment;

    juce::StringArray legend;
    bool hasCentre = false, ringEnabled = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConcentricBand)
};

//==============================================================================
class SwitchButton final : public juce::Component
{
public:
    /** `tint` is what the switch lights up in: the suite's pink by default,
        the module's accent or the deeper azure where a panel says so. */
    SwitchButton (juce::RangedAudioParameter&, const juce::String& text,
                  juce::Colour tint = tokens().switchOn);

    void resized() override;
    void setSwitchEnabled (bool);

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

        `faceColour` is the plate the scale is printed on. It is a caller's
        choice rather than always `well` because the two are what decide
        whether the meter can be read at all: the first cut drew a #97ddff hot
        zone on `well` #d6d6d6, which measures 1.02:1 and is invisible. A
        needle meter wants a dark face and light ink, the way the hardware
        does it. */
    DynamicsMeter (std::function<float()> inputRmsSource,
                   std::function<float()> outputRmsSource,
                   std::function<float()> gainReductionDbSource,
                   Mode initialMode = Mode::output,
                   juce::Colour accent = tokens().accent,
                   juce::Colour hotColour = tokens().meterClip,
                   juce::Colour faceColour = tokens().well);

    void paint (juce::Graphics&) override;

    void setMode (Mode) noexcept;
    Mode getMode() const noexcept { return mode; }

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
    juce::Colour accentColour, hotColour, faceColour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicsMeter)
};

/** "1.6 kHz" -> "1k6", "360 Hz" -> "360", "Off" -> "OFF". A legend has to fit
    around a knob, and this is how the hardware prints it. */
juce::String compactFrequency (const juce::String& text);

} // namespace bmo::ui

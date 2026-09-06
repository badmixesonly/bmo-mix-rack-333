#pragma once

#include "LookAndFeel.h"
#include "core/state/ParamSpec.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

namespace bmo::ui
{

/** A knob with its name underneath and nothing else: no number, only a plus
    one side and, where it cuts, a minus the other. */
class PlainKnob final : public juce::Component
{
public:
    PlainKnob (juce::RangedAudioParameter&, const juce::String& caption,
               Knob::Style style = Knob::Style::utility, float faceScale = 0.5f,
               juce::Colour accent = tokens().accent);

    void paint (juce::Graphics&) override;
    void resized() override;

    void setKnobEnabled (bool);

private:
    /** Room under the knob for its name. */
    static constexpr int kCaptionRow = 22;

    juce::String caption;
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
/** Input, output or gain-reduction, one at a time, cycled by clicking --
    BMO Opto's centre meter, and any dynamics module after it.

    Input and output read as VU, calibrated the same way as OutputMeter (0 VU
    = -18 dBFS, see its class comment); gain reduction reads the module's own
    published figure directly, in dB, filling the same well from empty
    upward rather than the hardware's needle-down convention -- simpler to
    read at a glance against the other two modes, and an easy flip later
    (`paint()`) if that turns out to be the wrong call once this is actually
    running.
*/
class DynamicsMeter final : public juce::Component,
                            private juce::Timer
{
public:
    enum class Mode { input, output, reduction };

    DynamicsMeter (std::function<float()> inputRmsSource,
                   std::function<float()> outputRmsSource,
                   std::function<float()> gainReductionDbSource,
                   Mode initialMode = Mode::output);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

    Mode getMode() const noexcept { return mode; }

private:
    void timerCallback() override;

    std::function<float()> inputRms, outputRms, gainReductionDb;
    Mode mode;
    float displayed = 0.0f;

    static constexpr float kVuReference = -18.0f;
    static constexpr float kGrRangeDb   = 24.0f;
    static constexpr int   kBarWidth    = 14;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicsMeter)
};

/** "1.6 kHz" -> "1k6", "360 Hz" -> "360", "Off" -> "OFF". A legend has to fit
    around a knob, and this is how the hardware prints it. */
juce::String compactFrequency (const juce::String& text);

} // namespace bmo::ui

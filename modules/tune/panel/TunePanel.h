#pragma once

#include "core/product/ModuleDef.h"
#include "core/ui/ModulePanel.h"
#include <array>

namespace bmo::tune
{

/** BMO Tune RT's panel: round 7 of the panel studies (design/), built from
    the suite's own controls.

    Top to bottom:
      - the mode banner, Classic or Hybrid, across the panel: the one choice
        that decides what the lower section draws;
      - what to tune to: the keyboard (click a note out of the scale), Key
        with its ♯ and ♭, Scale, Pitch Range and Ref A as value boxes that
        open menus, and Latency level with Ref A;
      - a rule, then everything that shapes the correction, as a clock around
        a centred Retune: Vibrato and Flex at 10 and 2 o'clock in both modes;
        on Hybrid, Glide and Shift at 8 and 4 and Formant Keep/Follow under
        Retune, between them.

    **The hide rule.** A control a mode does not use is hidden, not greyed
    (Frosty, 2026-09-10), and nothing moves into its place. Which controls
    that is comes from `isHybridOnly()` in params.h and nowhere else --
    applyMode() walks that list -- and tests/plugin/PanelTests.cpp checks the
    panel against it, as tests/dsp/ModeTests.cpp checks the DSP.

    Colours are lime by the suite's rules, as the studies settled them: knob
    caps by `faceOf`, captions by `accentInk`, and every selector lit in the
    raw accent. Captions are drawn by the panel rather than by PlainKnob,
    whose caption is the raw accent -- lime is too light for that to read on
    the pale plate (1.35:1), and the studies were approved with `accentInk`.
*/
class TunePanel final : public ui::ModulePanel,
                        private juce::Timer
{
public:
    static constexpr int kDesignWidth = 360;

    explicit TunePanel (ui::ModuleContext);
    ~TunePanel() override;

    void resized() override;

    /** Every control on the panel that sets parameter `index` -- for the
        layout test, which holds the hide rule to isHybridOnly(). */
    std::vector<juce::Component*> controlsFor (int index);

    /** Every switch on the panel, for the label-fit test. */
    std::vector<juce::ToggleButton*> switches();

    /** What a value box shows, and how much wider that is than its box. */
    struct BoxText { juce::String id, text; float overflow; };
    std::vector<BoxText> boxTexts();

    /** Reads every parameter back into the panel now, rather than on the next
        timer tick. For the snapshot tool and the tests, which have no message
        loop to wait on. */
    void syncNow() { sync (true); }

    class ValueBox;
    class Keyboard;

private:
    void paintPanel (juce::Graphics&) override;
    void timerCallback() override;

    /** Parameters into switch states, visibility and boxes. Cheap when
        nothing changed; `force` redoes everything. */
    void sync (bool force);

    /** The hide rule: every control of an isHybridOnly() parameter is shown
        on Hybrid and hidden on Classic. */
    void applyMode (bool hybrid);

    void setChoice (int index, int choice);
    void setReal (int index, float value);
    int choiceOf (int index) const;

    void showKeyMenu();
    void showScaleMenu();
    void showRangeMenu();
    void showRefMenu();
    void beginCustomRef();
    void commitCustomRef();

    /** The Key parameter as a letter and an accidental. */
    struct Spelling { int natural; int accidental; };   // natural 0..6 = C..B
    static Spelling spellingOf (int keyChoice);
    static int keyChoiceOf (Spelling);
    static bool hasSpelling (Spelling s) { return keyChoiceOf (s) >= 0; }

    juce::String keyText() const;
    juce::String refText() const;

    struct KnobPlace { ui::PlainKnob* knob; const char* caption; juce::Point<int> centre; int face; float captionSize; };
    std::vector<KnobPlace> knobPlaces();

    ui::PlainKnob retune, vibrato, flex, glide, shift;

    juce::ToggleButton classicButton { "Classic" }, hybridButton { "Hybrid" };
    juce::ToggleButton sharpButton, flatButton;
    juce::ToggleButton liveButton { "Live" }, studioButton { "Studio" };
    juce::ToggleButton keepButton { "Keep" }, followButton { "Follow" };

    std::unique_ptr<juce::Component> sharpGlyph, flatGlyph;

    std::unique_ptr<ValueBox> keyBox, scaleBox, rangeBox, refBox;
    std::unique_ptr<Keyboard> keyboard;
    juce::TextEditor refEditor;

    std::unique_ptr<juce::LookAndFeel_V4> menuLook;

    /** The last values sync() pushed into the controls. */
    std::array<float, 32> shown {};
    bool hybridShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunePanel)
};

} // namespace bmo::tune

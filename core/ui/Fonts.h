#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bmo::ui
{

/** The two places a typeface is named.

    Both faces are embedded in the binary rather than looked up on the machine.
    A name resolved at runtime gives every listener a different panel: the
    first round of design feedback on the EQ came from two people looking at
    two different fonts without either of them knowing it.

    See assets/fonts/README.md, including the note on licensing.

    labelFont is Minerva Black, which is everything on the panel, tracked out a
    little as the mockup has it. captionFont is Blender, which is the name
    under a utility knob -- INPUT, OUTPUT, GAIN -- and nothing else.
*/
juce::Font labelFont (float height, bool bold = false);
juce::Font captionFont (float height);

/** Text, filled and nothing else. The EQ used to stroke a black outline
    around every label so a pale fill would read on a pale ground; the outline
    is gone, so a label's colour now has to carry its own contrast. */
void drawLabel (juce::Graphics&, const juce::String&, juce::Rectangle<float>,
                juce::Justification, const juce::Font&, juce::Colour fill);

} // namespace bmo::ui

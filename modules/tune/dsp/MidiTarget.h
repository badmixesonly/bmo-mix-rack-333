#pragma once

#include "modules/tune/dsp/Scale.h"
#include <array>

namespace bmo::tune
{

/** What the MIDI input is doing to the target (spec §4.6).

    Three modes, in the order they are asked for:

      Off     MIDI is ignored.
      Target  the most recently pressed held note IS the target, bypassing
              the scale entirely -- the table-stakes mode.
      Scale   the held notes are the scale: the quantizer runs as usual but
              over the pitch classes being held.

    Latch keeps the last note (or chord) after release until the next
    note-on, so a part can be played in once and left. Required means no
    held (or latched) note = no correction at all, rather than falling back
    to the key and scale.

    Fixed-size and allocation-free: note events arrive on the audio thread.
*/
class MidiTarget
{
public:
    enum class Mode { off, target, scale };

    void reset() noexcept
    {
        count = 0;
        latchedCount = 0;
        allReleasedSinceLatch = true;
    }

    void noteOn (int note) noexcept
    {
        if (note < 0 || note > 127)
            return;

        remove (note);

        if (count == kMax)
            shiftOut (0);   // oldest falls off; last-note priority keeps the newest

        held[(size_t) count++] = note;

        // The first note-on after everything was released starts a new
        // latched chord; later ones add to it.
        if (allReleasedSinceLatch)
        {
            latchedCount = 0;
            allReleasedSinceLatch = false;
        }

        if (latchedCount < kMax)
        {
            for (int i = 0; i < latchedCount; ++i)
                if (latched[(size_t) i] == note)
                    return;
            latched[(size_t) latchedCount++] = note;
        }
    }

    void noteOff (int note) noexcept
    {
        remove (note);
        if (count == 0)
            allReleasedSinceLatch = true;
    }

    void allNotesOff() noexcept
    {
        count = 0;
        latchedCount = 0;
        allReleasedSinceLatch = true;
    }

    /** The note Target mode steers to, or false if there is none. */
    bool targetNote (bool latch, int& note) const noexcept
    {
        if (count > 0)
        {
            note = held[(size_t) count - 1];
            return true;
        }

        if (latch && latchedCount > 0)
        {
            note = latched[(size_t) latchedCount - 1];
            return true;
        }

        return false;
    }

    /** The mask Scale mode quantizes over; 0 if nothing is held. */
    NoteMask heldMask (bool latch) const noexcept
    {
        NoteMask m = 0;
        const auto* notes = count > 0 ? held.data() : latched.data();
        const auto n = count > 0 ? count : (latch ? latchedCount : 0);

        for (int i = 0; i < n; ++i)
            m = (NoteMask) (m | (1u << (notes[(size_t) i] % 12)));

        return m;
    }

    bool anyHeld (bool latch) const noexcept { return count > 0 || (latch && latchedCount > 0); }

private:
    static constexpr int kMax = 16;

    void remove (int note) noexcept
    {
        for (int i = 0; i < count; ++i)
            if (held[(size_t) i] == note)
            {
                shiftOut (i);
                return;
            }
    }

    void shiftOut (int index) noexcept
    {
        for (int i = index; i + 1 < count; ++i)
            held[(size_t) i] = held[(size_t) i + 1];
        --count;
    }

    std::array<int, kMax> held {}, latched {};
    int count = 0, latchedCount = 0;
    bool allReleasedSinceLatch = true;
};

} // namespace bmo::tune

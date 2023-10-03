/*
    Copyright 2023 Zach Vonler <zvonler@gmail.com>

    This file is part of DebouncedButton.

    DebouncedButton is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    DebouncedButton is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along with
    DebouncedButton.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef debounced_button_h
#define debounced_button_h

#ifdef UNIT_TESTING
#include <algorithm>
#include <cstdint>
#define max std::max
#else
#include <Arduino.h>
#endif

/*---------------------------------------------------------------------------*/

/**
 * Represents a debounced, two-state (digital) button.
 */
class DebouncedButton
{
public:
    enum Input {
        NONE,
        CLICK,
        DOUBLE_CLICK,
        LONG_PRESS,
        CLICK_AND_LONG_PRESS,
        DOUBLE_CLICK_AND_LONG_PRESS,
        RELEASE,
    };

    // The button's state must be different for at least this long to cause
    // the debounced state to change.
    static const uint32_t DEBOUNCE_MS = 20;

    // A press that lasts less than the cutoff is a click, one that lasts
    // longer is a hold (long press).
    static const uint32_t CLICKED_CUTOFF_MS = 150;

    static const uint32_t DOUBLE_CLICK_TIMEOUT_MS = 150;

private:
    /**
     * The state values that end in _PENDING indicate ones for which no Input
     * has yet been delivered.
     */
    enum State {
        IDLE,
        PRESSED,
        PRESSED_PENDING,
        CLICKED_PENDING,
        CLICKED_PRESSED_PENDING,
        DOUBLE_CLICKED_PENDING,
        DOUBLE_CLICKED_PRESSED_PENDING,
    };

    bool _pressed_state;
    State _state = IDLE;
    bool _prev_reading = false;
    bool _debounced_reading = false;
    uint32_t _last_reading_change_tm = 0;
    uint32_t _last_change_tm = 0;
    uint32_t _prev_last_change_tm = 0;

public:
    /**
     * Creates a new instance with the specified polarity.
     */
    DebouncedButton(bool pressed_state = true);

    /**
     * Adds a reading to the button, and returns any recognized Input.
     */
    Input update(bool reading, uint32_t tm);

    /**
     * Describes an input in human-readable terms.
     */
    const char* describe_input(Input input) const;

    /**
     * Returns the debounced state of the button, true for pressed and
     * false otherwise.
     */
    bool state() const { return _debounced_reading == _pressed_state; }

    /**
     * Returns the number of milliseconds between tm and the last change in
     * the debounced state, or 0 if tm is earlier than the last change time.
     */
    uint32_t duration(uint32_t tm) const { return max(uint32_t(0), tm - _last_change_tm); }

    /**
     * Returns the number of milliseconds the button was in its previous state.
     */
    uint32_t prev_duration(uint32_t tm) const { return _last_change_tm - _prev_last_change_tm; }

    /**
     * Resets the state change timestamps of the button, effectively meaning
     * the button has been in its current state since the beginning of time.
     */
    void reset_duration() { _last_change_tm = _prev_last_change_tm = 0; }
};

/*---------------------------------------------------------------------------*/

#endif

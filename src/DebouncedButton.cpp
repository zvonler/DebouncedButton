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

#include "DebouncedButton.h"

/*-------------------------------------------------------------------------*/

DebouncedButton::DebouncedButton(bool pressed_state)
    : _pressed_state(pressed_state)
{
    _prev_reading = !pressed_state;
}

DebouncedButton::Input
DebouncedButton::update(bool reading, uint32_t tm)
{
    if (!_pressed_state)
        reading = !reading;

    if (_prev_reading != reading) {
        // If the reading has changed, begin a new debounce period.
        _last_reading_change_tm = tm;
        _prev_reading = reading;
        return NONE;
    }

    auto reading_duration = tm - _last_reading_change_tm;

    Input input = NONE;

    if (_debounced_reading != reading) {
        if (reading_duration < DEBOUNCE_MS)
            return NONE;

        // The new reading has passed the debounce period.
        if (_state == IDLE) {
            _state = PRESSED_PENDING;
        } else if (_state == PRESSED_PENDING) {
            _state = CLICKED_PENDING;
        } else if (_state == PRESSED) {
            input = RELEASE;
            _state = IDLE;
        } else if (_state == CLICKED_PENDING) {
            _state = CLICKED_PRESSED_PENDING;
        } else if (_state == CLICKED_PRESSED_PENDING) {
            _state = DOUBLE_CLICKED_PENDING;
        } else if (_state == DOUBLE_CLICKED_PENDING) {
            _state = DOUBLE_CLICKED_PRESSED_PENDING;
        } else if (_state == DOUBLE_CLICKED_PRESSED_PENDING) {
            input = DOUBLE_CLICK;
            _state = CLICKED_PENDING;
        }

        _debounced_reading = reading;
        _prev_last_change_tm = _last_change_tm;
        _last_change_tm = tm;

    } else {
        if (_state == CLICKED_PENDING) {
            if (duration(tm) > DOUBLE_CLICK_TIMEOUT_MS) {
                input = CLICK;
                _state = IDLE;
            }
        } else if (_state == PRESSED_PENDING) {
            if (duration(tm) >= CLICKED_CUTOFF_MS) {
                input = LONG_PRESS;
                _state = PRESSED;
            }
        } else if (_state == CLICKED_PRESSED_PENDING) {
            if (duration(tm) >= CLICKED_CUTOFF_MS) {
                input = CLICK_AND_LONG_PRESS;
                _state = PRESSED;
            }
        } else if (_state == DOUBLE_CLICKED_PENDING) {
            if (duration(tm) >= CLICKED_CUTOFF_MS) {
                input = DOUBLE_CLICK;
                _state = IDLE;
            }
        } else if (_state == DOUBLE_CLICKED_PRESSED_PENDING) {
            if (duration(tm) >= CLICKED_CUTOFF_MS) {
                input = DOUBLE_CLICK_AND_LONG_PRESS;
                _state = PRESSED;
            }
        }
    }

    return input;
}

const char*
DebouncedButton::describe_input(Input input) const
{
    switch (input) {
        case NONE:                        return "none";
        case CLICK:                       return "click";
        case DOUBLE_CLICK:                return "double click";
        case LONG_PRESS:                  return "long press";
        case CLICK_AND_LONG_PRESS:        return "click and long press";
        case DOUBLE_CLICK_AND_LONG_PRESS: return "double click and long press";
        case RELEASE:                     return "release";
        default:                          return "unknown";
    }
    // Unreachable
}

/*-------------------------------------------------------------------------*/


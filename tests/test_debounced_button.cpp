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

#include <gtest/gtest.h>
#include <random>
#include <tuple>

#include "../src/DebouncedButton.h"

namespace {

/*---------------------------------------------------------------------------*/

class TestingScript
{
    using Input = DebouncedButton::Input;

public:
    struct ScriptPoint
    {
        uint32_t _tm;
        bool _button_state;
        Input _expected_input;

        ScriptPoint(uint32_t tm, bool button_state, Input expected_input = DebouncedButton::NONE)
            : _tm(tm)
            , _button_state(button_state)
            , _expected_input(expected_input)
        { }
    };

    ScriptPoint const* _script_points;
    std::string _name;
    int _num_points;

    TestingScript(char const* name, ScriptPoint const script_points[], int num_points)
        : _script_points(script_points)
        , _name(name)
        , _num_points(num_points)
    { }

    void execute(DebouncedButton& button)
    {
        for (int i = 0; i < _num_points; ++i)
        {
            ScriptPoint const& sp = _script_points[i];
            auto actual_input = button.update(sp._button_state, sp._tm);

            SCOPED_TRACE(_name + " i:" + std::to_string(i));
            EXPECT_EQ(sp._expected_input, actual_input);
        }
    }
};

using ScriptPoint = TestingScript::ScriptPoint;

/*---------------------------------------------------------------------------*/

#define ARRAY_SIZE(A) sizeof(A)/sizeof(A[0])
#define RUN_SCRIPT(NAME,BUTTON) TestingScript(#NAME, NAME ## _script, ARRAY_SIZE(NAME ## _script)).execute(BUTTON);

class TestDebouncedButton : public testing::Test
{
protected:
    std::mt19937 _rng;

    void SetUp() override
    {
        _rng.seed(123456);
    }
};

TEST_F(TestDebouncedButton, TestInitialState)
{
    DebouncedButton button;
    EXPECT_EQ(false, button.state());

    EXPECT_EQ(0, button.duration(0));
    EXPECT_EQ(12345, button.duration(12345));

    EXPECT_EQ(0, button.prev_duration(0));
    EXPECT_EQ(0, button.prev_duration(12345));
}

TEST_F(TestDebouncedButton, TestFirstReading)
{
    // Test with first reading true
    {
        DebouncedButton button;
        auto input = button.update(true, 1);
        EXPECT_EQ(DebouncedButton::NONE, input);
    }

    // Test with first reading false
    {
        DebouncedButton button;
        auto input = button.update(false, 1);
        EXPECT_EQ(DebouncedButton::NONE, input);
    }
}

TEST_F(TestDebouncedButton, TestRapidPresses)
{
    DebouncedButton button;

    // Test presses and releases that are all shorter than debounce delay
    std::uniform_int_distribution<uint32_t> sub_debounce_delay(1, button.DEBOUNCE_MS - 1);
    std::uniform_int_distribution<uint32_t> pressed(0, 1);

    uint32_t tm = 0;
    uint32_t end_tm = 120 * 1000;

    bool state = true;
    while (tm < end_tm) {
        auto input = button.update(state, tm);
        EXPECT_EQ(DebouncedButton::NONE, input);
        tm += sub_debounce_delay(_rng);
        state = !state;
    }
}


TEST_F(TestDebouncedButton, TestSinglePressAndRelease)
{
    DebouncedButton button;

    // Test of a press shorter than CLICKED_CUTOFF_MS
    {
        uint32_t press_tm = 0;
        uint32_t release_tm = press_tm + button.CLICKED_CUTOFF_MS - 10;
        uint32_t clicked_tm = release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 1;

        ScriptPoint click_script[] = {
            // Button pressed and held for debounce timeout
            { press_tm, true },
            { press_tm + button.DEBOUNCE_MS, true },
            // Button released just before click timeout and left released for debounce timeout
            // No click should be delivered before the double click timeout
            { release_tm, false },
            { release_tm + button.DEBOUNCE_MS, false },
            // Click should be delivered after the double click timeout
            { clicked_tm, false, DebouncedButton::CLICK },
        };

        RUN_SCRIPT(click, button);
    }

    // Test of a press longer than CLICKED_CUTOFF_MS
    {
        uint32_t press_tm = 0;
        uint32_t release_tm = press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS + 10;

        ScriptPoint hold_script[] = {
            // Button pressed and held for debounce timeout
            { press_tm, true },
            { press_tm + button.DEBOUNCE_MS, true },
            // Button still pressed just before release time should get held input
            { release_tm - 1, true, DebouncedButton::LONG_PRESS },
            // Button released just after click timeout and left released for debounce timeout
            // No click should be delivered before the double click timeout
            { release_tm, false },
            // After debounce timeout released input should be delivered
            { release_tm + button.DEBOUNCE_MS, false, DebouncedButton::RELEASE }
        };

        RUN_SCRIPT(hold, button);
    }
}

TEST_F(TestDebouncedButton, TestDoublePressAndRelease)
{
    DebouncedButton button;

    // Two clicks within the double click timeout
    {
        uint32_t first_press_tm = 0;
        uint32_t first_release_tm = first_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;

        uint32_t second_press_tm = first_release_tm + button.DOUBLE_CLICK_TIMEOUT_MS - button.DEBOUNCE_MS - 10;
        uint32_t second_release_tm = second_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;

        uint32_t double_clicked_tm = second_release_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS;

        ScriptPoint double_click_script[] = {
            // Button pressed and held for debounce timeout
            { first_press_tm, true },
            { first_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { first_release_tm, false },
            { first_release_tm + button.DEBOUNCE_MS, false },
            // Button pressed again and held for debounce timeout
            { second_press_tm, true },
            { second_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { second_release_tm, false },
            { second_release_tm + button.DEBOUNCE_MS, false },
            // Nothing else for click cutoff should trigger double click event
            { double_clicked_tm, false, DebouncedButton::DOUBLE_CLICK }
        };

        RUN_SCRIPT(double_click, button);
    }

    // Two clicks separated by more than the double click timeout
    {
        uint32_t first_press_tm = 0;
        uint32_t first_release_tm = first_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;
        uint32_t first_clicked_tm = first_release_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS + 1;

        uint32_t second_press_tm = first_release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 10;
        uint32_t second_release_tm = second_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;
        uint32_t second_clicked_tm = second_release_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS + 1;

        ScriptPoint two_clicks_script[] = {
            // Button pressed and held for debounce timeout
            { first_press_tm, true },
            { first_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { first_release_tm, false },
            { first_release_tm + button.DEBOUNCE_MS, false },
            // TODO: See if this and the next ScriptPoint can be combined
            // into { second_press_tm, true, DebouncedButton::CLICK }.
            { first_clicked_tm, false, DebouncedButton::CLICK },
            // Button pressed again and held for debounce timeout
            { second_press_tm, true },
            { second_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { second_release_tm, false },
            { second_release_tm + button.DEBOUNCE_MS, false },
            // Nothing else for click cutoff should trigger double click event
            { second_clicked_tm, false, DebouncedButton::CLICK }
        };

        RUN_SCRIPT(two_clicks, button);
    }

    // A long press followed by a click
    {
        uint32_t first_press_tm = 0;
        uint32_t first_release_tm = first_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS + 10;

        uint32_t second_press_tm = first_release_tm + button.DOUBLE_CLICK_TIMEOUT_MS - button.DEBOUNCE_MS - 10;
        uint32_t second_release_tm = second_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;

        uint32_t double_clicked_tm = second_release_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS + 1;

        ScriptPoint press_click_script[] = {
            // Button pressed and held for debounce timeout
            { first_press_tm, true },
            { first_press_tm + button.DEBOUNCE_MS, true },
            { first_release_tm - 1, true, DebouncedButton::LONG_PRESS },
            // Button released after click cutoff
            { first_release_tm, false },
            { first_release_tm + button.DEBOUNCE_MS, false, DebouncedButton::RELEASE },
            // Button pressed again and held for debounce timeout
            { second_press_tm, true },
            { second_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { second_release_tm, false },
            { second_release_tm + button.DEBOUNCE_MS, false },
            // Nothing else for click cutoff should trigger double click event
            { double_clicked_tm, false, DebouncedButton::CLICK }
        };

        RUN_SCRIPT(press_click, button);
    }

    // A click followed by a long press
    {
        uint32_t first_press_tm = 0;
        uint32_t first_release_tm = first_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;

        uint32_t second_press_tm = first_release_tm + button.DOUBLE_CLICK_TIMEOUT_MS - button.DEBOUNCE_MS - 10;
        uint32_t second_release_tm = second_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS + 10;

        uint32_t double_clicked_tm = second_release_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS;

        ScriptPoint click_press_script[] = {
            // Button pressed and held for debounce timeout
            { first_press_tm, true },
            { first_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { first_release_tm, false },
            { first_release_tm + button.DEBOUNCE_MS, false },
            // Button pressed again and held for debounce timeout
            { second_press_tm, true },
            { second_press_tm + button.DEBOUNCE_MS, true },
            // Button held past click cutoff
            { second_release_tm - 1, true, DebouncedButton::CLICK_AND_LONG_PRESS },
            // And released
            { second_release_tm, false },
            { second_release_tm + button.DEBOUNCE_MS, false, DebouncedButton::RELEASE },
        };

        RUN_SCRIPT(click_press, button);
    }
}

TEST_F(TestDebouncedButton, TestTriplePressAndRelease)
{
    // Three separate clicks
    {
        DebouncedButton button;

        uint32_t first_press_tm = 0;
        uint32_t first_release_tm = first_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;
        uint32_t first_clicked_tm = first_release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 1;

        uint32_t second_press_tm = first_clicked_tm + 1;
        uint32_t second_release_tm = second_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;
        uint32_t second_clicked_tm = second_release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 1;

        uint32_t third_press_tm = second_clicked_tm + 1;
        uint32_t third_release_tm = third_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;
        uint32_t third_clicked_tm = third_release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 1;

        ScriptPoint three_clicks_script[] = {
            // Button pressed and held for debounce timeout
            { first_press_tm, true },
            { first_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { first_release_tm, false },
            { first_release_tm + button.DEBOUNCE_MS, false },
            { first_clicked_tm, false, DebouncedButton::CLICK },
            // Button pressed again and held for debounce timeout
            { second_press_tm, true },
            { second_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { second_release_tm, false },
            { second_release_tm + button.DEBOUNCE_MS, false },
            { second_clicked_tm, false, DebouncedButton::CLICK },
            // Button pressed again and held for debounce timeout
            { third_press_tm, true },
            { third_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { third_release_tm, false },
            { third_release_tm + button.DEBOUNCE_MS, false },
            { third_clicked_tm, false, DebouncedButton::CLICK },
        };

        RUN_SCRIPT(three_clicks, button);
    }

    // Double click and separate click
    {
        DebouncedButton button;

        uint32_t first_press_tm = 0;
        uint32_t first_release_tm = first_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;

        uint32_t second_press_tm = first_release_tm + button.DOUBLE_CLICK_TIMEOUT_MS - 10;
        uint32_t second_release_tm = second_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;

        uint32_t double_click_tm = second_release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 1;

        uint32_t third_press_tm = double_click_tm + 1;
        uint32_t third_release_tm = third_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;
        uint32_t third_clicked_tm = third_release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 1;

        ScriptPoint double_plus_click_script[] = {
            // Button pressed and held for debounce timeout
            { first_press_tm, true },
            { first_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { first_release_tm, false },
            { first_release_tm + button.DEBOUNCE_MS, false },
            // Button pressed again and held for debounce timeout
            { second_press_tm, true },
            { second_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { second_release_tm, false },
            { second_release_tm + button.DEBOUNCE_MS, false },
            { double_click_tm, false, DebouncedButton::DOUBLE_CLICK },
            // Button pressed again and held for debounce timeout
            { third_press_tm, true },
            { third_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { third_release_tm, false },
            { third_release_tm + button.DEBOUNCE_MS, false },
            { third_clicked_tm, false, DebouncedButton::CLICK },
        };

        RUN_SCRIPT(double_plus_click, button);
    }

    // Click and separate double click
    {
        DebouncedButton button;

        uint32_t first_press_tm = 0;
        uint32_t first_release_tm = first_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;
        uint32_t first_clicked_tm = first_release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 1;

        uint32_t second_press_tm = first_release_tm + button.DOUBLE_CLICK_TIMEOUT_MS - 10;
        uint32_t second_release_tm = second_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;

        uint32_t third_press_tm = second_release_tm + button.DOUBLE_CLICK_TIMEOUT_MS - 10;
        uint32_t third_release_tm = third_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;

        uint32_t double_click_tm = third_release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 1;

        ScriptPoint click_plus_double_script[] = {
            // Button pressed and held for debounce timeout
            { first_press_tm, true },
            { first_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { first_release_tm, false },
            { first_release_tm + button.DEBOUNCE_MS, false },
            { first_clicked_tm, false, DebouncedButton::CLICK },
            // Button pressed again and held for debounce timeout
            { second_press_tm, true },
            { second_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { second_release_tm, false },
            { second_release_tm + button.DEBOUNCE_MS, false },
            // Button pressed again and held for debounce timeout
            { third_press_tm, true },
            { third_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { third_release_tm, false },
            { third_release_tm + button.DEBOUNCE_MS, false },
            { double_click_tm, false, DebouncedButton::DOUBLE_CLICK },
        };

        RUN_SCRIPT(click_plus_double, button);
    }

    // Double click and long press
    {
        DebouncedButton button;

        uint32_t first_press_tm = 0;
        uint32_t first_release_tm = first_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;

        uint32_t second_press_tm = first_release_tm + button.DOUBLE_CLICK_TIMEOUT_MS - 10;
        uint32_t second_release_tm = second_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;

        uint32_t double_click_tm = second_release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 1;

        uint32_t third_press_tm = double_click_tm + 1;
        uint32_t third_release_tm = third_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS + 10;
        uint32_t third_clicked_tm = third_release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 1;

        ScriptPoint double_plus_press_script[] = {
            // Button pressed and held for debounce timeout
            { first_press_tm, true },
            { first_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { first_release_tm, false },
            { first_release_tm + button.DEBOUNCE_MS, false },
            // Button pressed again and held for debounce timeout
            { second_press_tm, true },
            { second_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { second_release_tm, false },
            { second_release_tm + button.DEBOUNCE_MS, false },
            { double_click_tm, false, DebouncedButton::DOUBLE_CLICK },
            // Button pressed again and held for debounce timeout
            { third_press_tm, true },
            { third_press_tm + button.DEBOUNCE_MS, true },
            { third_release_tm - 1, true, DebouncedButton::LONG_PRESS },
            // Button released after click cutoff
            { third_release_tm, false },
            { third_release_tm + button.DEBOUNCE_MS, false, DebouncedButton::RELEASE },
        };

        RUN_SCRIPT(double_plus_press, button);
    }

    // Long press and double click
    {
        DebouncedButton button;

        uint32_t first_press_tm = 0;
        uint32_t first_release_tm = first_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS + 10;

        uint32_t second_press_tm = first_release_tm + button.DOUBLE_CLICK_TIMEOUT_MS - 10;
        uint32_t second_release_tm = second_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;

        uint32_t third_press_tm = second_release_tm + 1;
        uint32_t third_release_tm = third_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;

        uint32_t double_click_tm = third_release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 1;

        ScriptPoint press_plus_double_script[] = {
            // Button pressed and held for debounce timeout
            { first_press_tm, true },
            { first_press_tm + button.DEBOUNCE_MS, true },
            { first_release_tm - 1, true, DebouncedButton::LONG_PRESS },
            // Button released after click cutoff
            { first_release_tm, false },
            { first_release_tm + button.DEBOUNCE_MS, false, DebouncedButton::RELEASE },
            // Button pressed again and held for debounce timeout
            { second_press_tm, true },
            { second_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { second_release_tm, false },
            { second_release_tm + button.DEBOUNCE_MS, false },
            // Button pressed again and held for debounce timeout
            { third_press_tm, true },
            { third_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { third_release_tm, false },
            { third_release_tm + button.DEBOUNCE_MS, false },
            { double_click_tm, false, DebouncedButton::DOUBLE_CLICK },
        };

        RUN_SCRIPT(press_plus_double, button);
    }

    // Long press and two clicks
    {
        DebouncedButton button;

        uint32_t first_press_tm = 0;
        uint32_t first_release_tm = first_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS + 10;

        uint32_t second_press_tm = first_release_tm + 1;
        uint32_t second_release_tm = second_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;
        uint32_t second_click_tm = second_release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 1;

        uint32_t third_press_tm = second_click_tm + 1;
        uint32_t third_release_tm = third_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;
        uint32_t third_click_tm = third_release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 1;

        ScriptPoint press_plus_two_script[] = {
            // Button pressed and held for debounce timeout
            { first_press_tm, true },
            { first_press_tm + button.DEBOUNCE_MS, true },
            { first_release_tm - 1, true, DebouncedButton::LONG_PRESS },
            // Button released after click cutoff
            { first_release_tm, false },
            { first_release_tm + button.DEBOUNCE_MS, false, DebouncedButton::RELEASE },
            // Button pressed again and held for debounce timeout
            { second_press_tm, true },
            { second_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { second_release_tm, false },
            { second_release_tm + button.DEBOUNCE_MS, false },
            { second_click_tm, false, DebouncedButton::CLICK },
            // Button pressed again and held for debounce timeout
            { third_press_tm, true },
            { third_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { third_release_tm, false },
            { third_release_tm + button.DEBOUNCE_MS, false },
            { third_click_tm, false, DebouncedButton::CLICK },
        };

        RUN_SCRIPT(press_plus_two, button);
    }

    // Two clicks and long press
    {
        DebouncedButton button;

        uint32_t first_press_tm = 0;
        uint32_t first_release_tm = first_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;
        uint32_t first_click_tm = first_release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 1;

        uint32_t second_press_tm = first_click_tm + 1;
        uint32_t second_release_tm = second_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;
        uint32_t second_click_tm = second_release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 1;

        uint32_t third_press_tm = second_click_tm + 1;
        uint32_t third_release_tm = third_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS + 10;

        ScriptPoint two_plus_press_script[] = {
            // Button pressed and held for debounce timeout
            { first_press_tm, true },
            { first_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { first_release_tm, false },
            { first_release_tm + button.DEBOUNCE_MS, false },
            { first_click_tm, false, DebouncedButton::CLICK },
            // Button pressed again and held for debounce timeout
            { second_press_tm, true },
            { second_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { second_release_tm, false },
            { second_release_tm + button.DEBOUNCE_MS, false },
            { second_click_tm, false, DebouncedButton::CLICK },
            // Button pressed again and held for debounce timeout
            { third_press_tm, true },
            { third_press_tm + button.DEBOUNCE_MS, true },
            { third_release_tm - 1, true, DebouncedButton::LONG_PRESS },
            // Button released after click cutoff
            { third_release_tm, false },
            { third_release_tm + button.DEBOUNCE_MS, false, DebouncedButton::RELEASE },
        };

        RUN_SCRIPT(two_plus_press, button);
    }

    // Click and long press then separate click
    {
        DebouncedButton button;

        uint32_t first_press_tm = 0;
        uint32_t first_release_tm = first_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;
        uint32_t first_clicked_tm = first_release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 1;

        uint32_t second_press_tm = first_clicked_tm + 1;
        uint32_t second_release_tm = second_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS + 10;

        uint32_t third_press_tm = second_release_tm + button.DOUBLE_CLICK_TIMEOUT_MS;
        uint32_t third_release_tm = third_press_tm + button.DEBOUNCE_MS + button.CLICKED_CUTOFF_MS - 10;
        uint32_t third_clicked_tm = third_release_tm + button.DEBOUNCE_MS + button.DOUBLE_CLICK_TIMEOUT_MS + 1;

        ScriptPoint click_press_click_script[] = {
            // Button pressed and held for debounce timeout
            { first_press_tm, true },
            { first_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { first_release_tm, false },
            { first_release_tm + button.DEBOUNCE_MS, false },
            { first_clicked_tm, false, DebouncedButton::CLICK },
            // Button pressed again and held for debounce timeout
            { second_press_tm, true },
            { second_press_tm + button.DEBOUNCE_MS, true },
            { second_release_tm - 1, true, DebouncedButton::LONG_PRESS },
            // Button released after click cutoff
            { second_release_tm, false },
            { second_release_tm + button.DEBOUNCE_MS, false, DebouncedButton::RELEASE },
            // Button pressed again and held for debounce timeout
            { third_press_tm, true },
            { third_press_tm + button.DEBOUNCE_MS, true },
            // Button released before click cutoff
            { third_release_tm, false },
            { third_release_tm + button.DEBOUNCE_MS, false },
            { third_clicked_tm, false, DebouncedButton::CLICK },
        };

        RUN_SCRIPT(click_press_click, button);
    }
}

/*---------------------------------------------------------------------------*/

} // anonymous namespace

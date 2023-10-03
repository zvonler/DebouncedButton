/*
  SerialButtonTester

  Watches for user input gestures on a digital input pin and prints their
  description to the Serial Monitor.

  This example code is in the public domain.
*/


#include <DebouncedButton.h>

constexpr static int BUTTON_PIN = 2;

// The DebouncedButton is initialized with a boolean indicating which value
// corresponds to the button being pushed. When used with internal or external
// pullups, the pressed state should be false, and with a pulldown the pressed
// state should be true. If you seem to get inverted behavior from your button,
// try changing the value of this constant.
constexpr static bool PRESSED_STATE = true;

DebouncedButton button(PRESSED_STATE);

void setup()
{
    Serial.begin(115200);

    pinMode(BUTTON_PIN, INPUT);
}

void loop()
{
    auto now = millis();

    auto input = button.update(digitalRead(BUTTON_PIN), now);

    if (input != DebouncedButton::NONE) {
        Serial.print("Received input ");
        Serial.println(button.describe_input(input));
    }

    delay(10);
}


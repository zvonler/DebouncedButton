
# DebouncedButton

This library implements a class that models a momentary push button and
recognizes user input gestures, such as clicks, double-clicks, long presses,
etc.

## Theory of operation

The `DebouncedButton` class does not do any of the actual GPIO manipulation. The
client application is expected to call `DebouncedButton::update` frequently
(i.e. every few milliseconds) with the latest reading from the button pin. The
`update` method returns an `Input` enum to indicate if a user gesture has been
recognized on that update.

The `Input` enumeration values that can be returned from `update` are these:

| Name | Description |
| ---- | ----------- |
| NONE | No recognized gesture |
| CLICK | Button was pressed and released |
| DOUBLE_CLICK | Button was clicked twice rapidly |
| LONG_PRESS | Button was held down longer than a click |
| CLICK_AND_LONG_PRESS | Button was clicked and then long pressed |
| DOUBLE_CLICK_AND_LONG_PRESS | Button was double clicked and then long pressed |
| RELEASE | Button was released after a long press |

The `Input` values that include `_LONG_PRESS` are delivered when the long press
is first detected. All further calls to `update` with the same button reading
will return `NONE`, and when the button is released the `update` method will
return a `RELEASE` input. The `state` and `duration` methods can be called at
any time for further information about the button.

## Testing

This library includes unit tests that can be run on a host system (not on the
Arduino) using CMake. From the top-level of the library repo, these commands
should run the tests:

```
cd tests
cmake -S . -B build
cmake --build build && (cd build; ctest --output-on-failure)
```

The first `cmake` command above only needs to be run once, the second command
can be run each time the source is changed to check the test status.



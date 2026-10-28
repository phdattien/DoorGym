# DoorGym

Arduino Uno door alarm: an HC-SR04 ultrasonic sensor, a button and a buzzer.
User-facing behaviour (states, button use) is described in `README.md`.

## Commands

- `pio test -e native` runs the controller unit tests on the host. Run it after every logic change.
- `pio run` builds for the Uno; `pio run -t upload` flashes it.

## Architecture

- `src/modules/control.hpp`: the `Controller` state machine. It is **pure**: no `Arduino.h`, so it compiles in the native tests. Each tick it reads a `Controller::Inputs` snapshot and acts only through the `ControllerOutput` interface.
- `src/main.cpp`: hardware glue. Fills `Inputs` from the button and sensor every `loop()`, implements `ControllerOutput` with the buzzer and `Serial`.
- `src/modules/calibrator.hpp`: pure; finds a stable baseline distance.
- `src/modules/{button,distance,buzzer}.hpp`: Arduino drivers, header-only.
- `src/Config.hpp`: every tunable (timings, distances, frequencies, alarm melody). `src/IoMap.hpp`: pin numbers.
- `test/test_controller/test_main.cpp`: Unity tests driving `Controller` with a `FakeOutput` and simulated time.

## Rules

- Logic goes in the pure modules and gets a native test; Arduino calls stay in the drivers and `main.cpp`.
- Everything is non-blocking and driven by `millis()`. Measure time as `now - startMs >= intervalMs` so `millis()` rollover is safe.
- Button and sensor results are one-tick events (`pressed`, `longPressed`, `hasReading`); the controller reacts on the tick they are true.
- A distance of `0` means no echo, and is treated as neither blocked nor clear.
- Only a long-press calibration sets `_referenceBaseline`. After-alarm calibrations update `_activeBaseline` and must stay near the reference to re-arm.
- New constants go in `Config.hpp`. Wrap `Serial` string literals in `F()` to keep them out of the Uno's 2 KB RAM.
- When state transitions or `Config` values change, update the state diagram in `README.md`.
- Style: 4-space indent, `_member` prefix for private fields, short lowercase comments.

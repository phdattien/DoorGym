#pragma once

#include <Arduino.h>
#include "../IoMap.hpp"
#include "../Config.hpp"

class Button {
    public:
        void begin() {
            pinMode(IoMap::buttonPin, INPUT_PULLUP);
        }

        void update() {
            uint32_t now = millis();
            const bool raw = digitalRead(IoMap::buttonPin);
            _prevStable = _stable;

            if (raw != _lastRaw) {
                _lastRaw = raw;
                _lastChangeMs = now;
            }

            if (now - _lastChangeMs >= Config::Button::debounceMs) {
                _stable = _lastRaw;
            }

            if (pressed()) {
                _pressStartMs = now;
                _longReported = false;
            }

            _longPressEvent = false;
            if (!_stable && !_longReported && now - _pressStartMs >= Config::Button::longPressMs) {
                _longReported = true;
                _longPressEvent = true;
            }

            _shortPressEvent = released() && !_longReported;
        }

        // true once on the debounced press-down edge
        bool pressed() {
            return !_stable && _prevStable;
        }

        bool released() {
            return _stable && !_prevStable;
        }

        // true once on release, only if the press was shorter than Config::Button::longPressMs
        bool shortPressed() {
            return _shortPressEvent;
        }

        // true once when the button has been held for Config::Button::longPressMs
        bool longPressed() {
            return _longPressEvent;
        }

        bool getButtonState() {
            return _stable;
        }

    private:
        bool _lastRaw = HIGH;   // HIGH = released (INPUT_PULLUP)

        bool _stable = HIGH;
        bool _prevStable = HIGH;
        uint32_t _lastChangeMs = 0;

        uint32_t _pressStartMs = 0;
        bool _longReported = false;
        bool _longPressEvent = false;
        bool _shortPressEvent = false;
};



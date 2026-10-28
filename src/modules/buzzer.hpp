#pragma once

#include <Arduino.h>
#include "../IoMap.hpp"

class Buzzer {
    public:
        void begin() {
            pinMode(IoMap::buzzerPin, OUTPUT);
        }

        // non-blocking, tone() stops itself after durationMs
        void beep(uint16_t freqHz, uint32_t durationMs) {
            tone(IoMap::buzzerPin, freqHz, durationMs);
        }

        void silence() {
            noTone(IoMap::buzzerPin);
        }
};

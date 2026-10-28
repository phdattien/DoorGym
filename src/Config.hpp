#pragma once

#include <stdint.h>


namespace Config {
    namespace Button {
        const uint32_t debounceMs = 20;
        const uint32_t longPressMs = 2000;
    }

    namespace Distance {
        const uint32_t pingIntervalMs = 60;     // HC-SR04 needs ~60ms between pings
        const uint32_t blockMarginCm = 15;      // cm closer than baseline to count as blocked
        const uint8_t confirmReadings = 3;      // blocked readings in a row needed to alarm
    }

    namespace Calibration {
        const uint32_t stableToleranceCm = 10;  // every reading within anchor +-this
        const uint32_t stableWindowMs = 5000;   // how long readings must stay in tolerance
        const uint32_t tickIntervalMs = 1000;   // "still calibrating" tick
    }

    namespace Timing {
        const uint32_t armDelayMs = 5000;       // time to step away after calibrating
        const uint32_t alarmIntervalMs = 10000; // time from one alarm beep to the next
        const uint32_t sleepMs = 30000;
    }

    namespace Buzzer {
        const uint16_t bootFreqHz = 2000;
        const uint32_t bootMs = 100;

        const uint16_t tickFreqHz = 800;
        const uint32_t tickMs = 50;

        const uint16_t confirmFreqHz = 2000;
        const uint32_t confirmMs = 800;

        const uint16_t armedFreqHz = 3000;
        const uint32_t armedMs = 100;

        const uint16_t alarmFreqHz = 2500;
        const uint32_t alarmBeepMs = 200;

        const uint16_t sleepEndFreqHz = 1500;
        const uint32_t sleepEndMs = 100;
    }
}

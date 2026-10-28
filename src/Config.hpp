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
        const uint32_t afterAlarmTimeoutMs = 25000; // after-alarm calibration that never settles -> alarm
    }

    namespace Timing {
        const uint32_t armDelayMs = 2000;       // time to step away after calibrating
    }

    namespace Buzzer {
        const uint16_t bootFreqHz = 2000;
        const uint32_t bootMs = 100;

        const uint16_t confirmFreqHz = 1000;    // low and short: calibration done, not an alert
        const uint32_t confirmMs = 300;

        // alarm melody, played on repeat: a rising fanfare, then a pause (2 s per round)
        struct Note {
            uint16_t freqHz;                    // 0 = rest
            uint16_t durationMs;
        };
        const Note alarmMelody[] = {
            {1047, 150}, {1319, 150}, {1568, 150}, {2093, 300},     // C6 E6 G6 C7
            {0, 100},
            {1568, 150}, {2093, 450},                               // G6 C7
            {0, 550},
        };
        const uint8_t alarmMelodyLength = sizeof(alarmMelody) / sizeof(alarmMelody[0]);
        const uint16_t noteGapMs = 20;          // silence at the end of each note so repeats don't blur

        const uint16_t calibrateStartFreqHz = 1200; // long press registered or alarm stopped
        const uint32_t calibrateStartMs = 150;

        const uint16_t errorFreqHz = 400;       // baseline too short to detect anything
        const uint32_t errorMs = 500;
    }
}

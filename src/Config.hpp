#pragma once

#include <stdint.h>


namespace Config {
    namespace Button {
        const uint32_t debounceMs = 20;
        const uint32_t longPressMs = 1000;
    }

    namespace Distance {
        const uint32_t marginCm = 2;            // cm closer than baseline to count as blocked
        const uint32_t pingIntervalMs = 60;     // HC-SR04 needs ~60ms between pings
        const uint8_t confirmReadings = 2;      // readings in a row needed to change state
    }
}

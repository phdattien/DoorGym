#pragma once

#include <stdint.h>
#include "../Config.hpp"

// Finds a stable baseline distance: every reading must stay within
// anchor +-stableToleranceCm for stableWindowMs. No Arduino dependency.
class Calibrator {
    public:
        void begin() {
            _stable = false;
            _count = 0;
        }

        void update(uint32_t distanceCm, uint32_t now) {
            if (_stable) {
                return;
            }

            if (distanceCm == 0) {      // no echo, start over without an anchor
                _count = 0;
                return;
            }

            if (_count == 0 || !withinTolerance(distanceCm)) {
                _anchor = distanceCm;
                _windowStartMs = now;
                _sum = distanceCm;
                _count = 1;
                return;
            }

            _sum += distanceCm;
            _count++;
            _stable = now - _windowStartMs >= Config::Calibration::stableWindowMs;
        }

        bool isStable() {
            return _stable;
        }

        // rounded mean of the stable window, only meaningful once isStable()
        uint32_t baseline() {
            return _count == 0 ? 0 : (_sum + _count / 2) / _count;
        }

    private:
        bool withinTolerance(uint32_t cm) {
            const uint32_t diff = cm > _anchor ? cm - _anchor : _anchor - cm;
            return diff <= Config::Calibration::stableToleranceCm;
        }

        bool _stable = false;
        uint32_t _anchor = 0;
        uint32_t _windowStartMs = 0;
        uint32_t _sum = 0;
        uint16_t _count = 0;            // 0 = no anchor yet
};

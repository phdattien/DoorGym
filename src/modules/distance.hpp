#pragma once

#include <Arduino.h>
#include "../IoMap.hpp"
#include "../Config.hpp"



class DistanceSensor {
    public:
        void begin() {
            pinMode(IoMap::trigPin, OUTPUT);
            pinMode(IoMap::echoPin, INPUT);
        }

        uint32_t getDistance() {
            digitalWrite(IoMap::trigPin, LOW);
            delayMicroseconds(2);
            digitalWrite(IoMap::trigPin, HIGH);
            delayMicroseconds(10);
            digitalWrite(IoMap::trigPin, LOW);

            uint32_t duration = pulseIn(IoMap::echoPin, HIGH, 30000); // 0 = no echo
            uint32_t distance = duration / 58; // cm

            return distance;
        }

        // pings every Config::Distance::pingIntervalMs, true when a new reading was taken
        bool update() {
            const uint32_t now = millis();
            if (now - _lastPingMs < Config::Distance::pingIntervalMs) {
                return false;
            }
            _lastPingMs = now;
            _lastDistance = getDistance();
            return true;
        }

        uint32_t getLastDistance() {
            return _lastDistance;
        }

    private:
        uint32_t _lastPingMs = 0;
        uint32_t _lastDistance = 0;
};

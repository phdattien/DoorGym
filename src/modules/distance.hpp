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

        void setBaseline(uint32_t cm) {
            _baseline = cm;
        }

        uint32_t getBaseline() {
            return _baseline;
        }

        void update() {
            const uint32_t now = millis();
            if (now - _lastPingMs < Config::Distance::pingIntervalMs) {
                return;
            }
            _lastPingMs = now;

            const uint32_t distance = getDistance();
            _lastDistance = distance;
            const bool blocked = distance > 0 && distance + Config::Distance::marginCm < _baseline;

            if (blocked == _blocked) {
                _confirmCount = 0;
                return;
            }
            if (++_confirmCount < Config::Distance::confirmReadings) {
                return;
            }
            _confirmCount = 0;
            _blocked = blocked;
        }

        bool isBlocked() {
            return _blocked;
        }

        uint32_t getLastDistance() {
            return _lastDistance;
        }

    private:
        uint32_t _baseline = 0;                 // 0 = not calibrated, never triggers

        bool _blocked = false;
        uint8_t _confirmCount = 0;
        uint32_t _lastPingMs = 0;
        uint32_t _lastDistance = 0;
};


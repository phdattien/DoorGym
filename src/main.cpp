// tilt sensoer and buzzer, light sensor

#include <Arduino.h>
#include "pitches.h"


namespace IoMap {
    const int buzzerPin = 2;
    const int buttonPin = 3;
    const int trigPin = 4;
    const int echoPin = 5;
}


class Buzzer {
    public:
        void begin() {
            pinMode(IoMap::buzzerPin, OUTPUT);
        }

        void tone(int melody, int duration) {
            ::tone(IoMap::buzzerPin, melody, duration); // :: = Arduino's global tone()
        }
};


class Button {
    public:
        void begin() {
            pinMode(IoMap::buttonPin, INPUT_PULLUP);
        }

        bool isPressed() {
            return digitalRead(IoMap::buttonPin) == LOW;
        }

    private:
        size_t pressedCount_ = 0;
};


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

        // call every loop; returns true once each time something enters the gate
        bool update() {
            const uint32_t now = millis();
            if (now - _lastPingMs < _pingInterval) {
                return false;
            }
            _lastPingMs = now;

            const uint32_t distance = getDistance();
            _lastDistance = distance;
            const bool blocked = distance > 0 && distance + _margin < _baseline;

            if (blocked == _blocked) {
                _confirmCount = 0;
                return false;
            }
            if (++_confirmCount < _confirmReadings) {
                return false;
            }
            _confirmCount = 0;
            _blocked = blocked;
            return _blocked;
        }

        bool isBlocked() {
            return _blocked;
        }

        uint32_t getLastDistance() {
            return _lastDistance;
        }

    private:
        uint32_t _baseline = 0;                 // 0 = not calibrated, never triggers
        const uint32_t _margin = 2;            // cm closer than baseline to count as blocked
        const uint32_t _pingInterval = 60;      // ms, HC-SR04 needs ~60ms between pings
        const uint8_t _confirmReadings = 2;     // readings in a row needed to change state

        bool _blocked = false;
        uint8_t _confirmCount = 0;
        uint32_t _lastPingMs = 0;
        uint32_t _lastDistance = 0;
};

int melody = NOTE_C4;
Button button;
Buzzer buzzer;
DistanceSensor sensor;


void setup() {
    Serial.begin(9600);
    button.begin();
    buzzer.begin();
    sensor.begin();

    // TODO: proper calibration, for now a single reading of the empty gate
    delay(100);
    sensor.setBaseline(12);
    Serial.print("baseline: ");
    Serial.println(sensor.getBaseline());
}

void loop() {
    // if (button.isPressed()) {
    //     tone(IoMap::buzzerPin, melody, 1000);
    // }

    if (sensor.update()) {
        Serial.println("something passed");
        Serial.println(sensor.getLastDistance());
        buzzer.tone(melody, 200);
    }
    else {
        Serial.println("nothing passed");
        Serial.println(sensor.getLastDistance());
    }

    delay(10);
}

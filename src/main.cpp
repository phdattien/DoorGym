// ultrasonic door alarm: button, HC-SR04 distance sensor and buzzer

#include <Arduino.h>
#include "modules/button.hpp"
#include "modules/distance.hpp"
#include "modules/buzzer.hpp"
#include "modules/control.hpp"


// connects the pure controller to the buzzer and Serial log
class HardwareOutput : public ControllerOutput {
    public:
        explicit HardwareOutput(Buzzer& buzzer) : _buzzer(buzzer) {}

        void beep(uint16_t freqHz, uint32_t durationMs) override {
            _buzzer.beep(freqHz, durationMs);
        }

        void silence() override {
            _buzzer.silence();
        }

        void stateEntered(ControllerState state) override {
            switch (state) {
                case ControllerState::Uncalibrated: Serial.println(F("uncalibrated: long press to calibrate")); break;
                case ControllerState::Calibrating:  Serial.println(F("calibrating")); break;
                case ControllerState::Calibrated:   Serial.println(F("calibrated, arming")); break;
                case ControllerState::Idle:         Serial.println(F("idle")); break;
                case ControllerState::Alarm:        Serial.println(F("ALARM")); break;
            }
        }

        void baselineAccepted(uint32_t baselineCm, bool isReference) override {
            Serial.print(isReference ? F("reference baseline: ") : F("baseline: "));
            Serial.print(baselineCm);
            Serial.println(F(" cm"));
        }

        void baselineRejected(uint32_t baselineCm, uint32_t referenceCm) override {
            Serial.print(F("still blocked after alarm: "));
            Serial.print(baselineCm);
            Serial.print(F(" cm, reference "));
            Serial.print(referenceCm);
            Serial.println(F(" cm"));
        }

        void baselineTooShort(uint32_t baselineCm) override {
            Serial.print(F("baseline too short: "));
            Serial.print(baselineCm);
            Serial.println(F(" cm, recalibrating"));
        }

        void calibrationTimedOut() override {
            Serial.println(F("never settled after alarm"));
        }

    private:
        Buzzer& _buzzer;
};


Button button;
DistanceSensor sensor;
Buzzer buzzer;
HardwareOutput output(buzzer);
Controller controller(output);


void setup() {
    Serial.begin(9600);
    button.begin();
    sensor.begin();
    buzzer.begin();
    controller.begin(millis());
}

void loop() {
    button.update();

    Controller::Inputs in;
    in.now = millis();
    in.pressed = button.pressed();
    in.shortPressed = button.shortPressed();
    in.longPressed = button.longPressed();
    in.hasReading = sensor.update();
    in.distanceCm = sensor.getLastDistance();

    controller.update(in);
}

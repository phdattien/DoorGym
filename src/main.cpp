// tilt sensoer and buzzer, light sensor

#include <Arduino.h>
#include "pitches.h"
#include "modules/button.hpp"
#include "modules/distance.hpp"


Button button;
DistanceSensor sensor;


void setup() {
    Serial.begin(9600);
    button.begin();
    sensor.begin();
}

void loop() {
    button.update();

    if (button.pressed()) {
        Serial.println("button pressed");
    }
}

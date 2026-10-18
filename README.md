# DoorGym

An Arduino-based alarm that sounds an active buzzer while a door is open.

## Hardware

- Arduino Uno
- Normally closed magnetic reed switch between digital pin 2 and GND
- Active buzzer signal connected to digital pin 8 and GND (use a driver if the
  buzzer exceeds the GPIO pin's current rating)

With this wiring, the reed switch holds pin 2 LOW while the door is closed.
Opening the door makes it HIGH and turns on the buzzer.

## Build and upload

Install [PlatformIO Core](https://platformio.org/install/cli), connect the Uno,
then run these commands from the project directory:

```sh
pio run
pio run --target upload
```

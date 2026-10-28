#pragma once

#include <stdint.h>
#include "../Config.hpp"
#include "calibrator.hpp"

// Pure state machine: no Arduino dependency, so it runs in native tests.
// Hardware is wired in by the sketch through Inputs and ControllerOutput.

enum class ControllerState { Uncalibrated, Calibrating, Calibrated, Idle, Alarm, Sleep };

class ControllerOutput {
    public:
        virtual void beep(uint16_t freqHz, uint32_t durationMs) = 0;
        virtual void silence() = 0;
        virtual void stateEntered(ControllerState state) = 0;
        virtual void baselineAccepted(uint32_t baselineCm, bool isReference) = 0;
        virtual void baselineRejected(uint32_t baselineCm, uint32_t referenceCm) = 0;
};

class Controller {
    public:
        struct Inputs {
            uint32_t now = 0;
            bool pressed = false;       // press-down edge
            bool shortPressed = false;
            bool longPressed = false;
            bool hasReading = false;    // distanceCm is a new reading this tick
            uint32_t distanceCm = 0;    // 0 = no echo
        };

        explicit Controller(ControllerOutput& out) : _out(out) {}

        void begin(uint32_t now) {
            setState(ControllerState::Uncalibrated, now);
        }

        void update(const Inputs& in) {
            switch (_state) {
                case ControllerState::Uncalibrated:
                    if (in.longPressed) startCalibration(false, in.now);
                    break;

                case ControllerState::Calibrating:
                    if (in.longPressed) {
                        startCalibration(false, in.now);
                        break;
                    }
                    if (in.hasReading) {
                        _calibrator.update(in.distanceCm, in.now);
                        if (_calibrator.isStable()) {
                            finishCalibration(in.now);
                            break;
                        }
                    }
                    if (in.now - _lastTickMs >= Config::Calibration::tickIntervalMs) {
                        _lastTickMs = in.now;
                        _out.beep(Config::Buzzer::tickFreqHz, Config::Buzzer::tickMs);
                    }
                    break;

                case ControllerState::Calibrated:
                    if (in.now - _enteredMs >= Config::Timing::armDelayMs) {
                        setState(ControllerState::Idle, in.now);
                    }
                    break;

                case ControllerState::Idle:
                    if (in.longPressed) {
                        startCalibration(false, in.now);
                        break;
                    }
                    if (in.hasReading && isBlocked(in.distanceCm)) {
                        setState(ControllerState::Alarm, in.now);
                    }
                    break;

                case ControllerState::Alarm:
                    if (in.pressed || in.shortPressed || in.longPressed) {
                        setState(ControllerState::Sleep, in.now);
                        break;
                    }
                    if (in.now - _lastAlarmBeepMs >= Config::Timing::alarmIntervalMs) {
                        alarmBeep(in.now);
                    }
                    break;

                case ControllerState::Sleep:
                    if (in.now - _enteredMs >= Config::Timing::sleepMs) {
                        _out.beep(Config::Buzzer::sleepEndFreqHz, Config::Buzzer::sleepEndMs);
                        startCalibration(true, in.now);
                    }
                    break;
            }
        }

        ControllerState state() {
            return _state;
        }

        uint32_t activeBaseline() {
            return _activeBaseline;
        }

        uint32_t referenceBaseline() {
            return _referenceBaseline;
        }

    private:
        // entry actions for each state
        void setState(ControllerState next, uint32_t now) {
            _state = next;
            _enteredMs = now;
            _out.stateEntered(_state);

            switch (_state) {
                case ControllerState::Uncalibrated:
                    _out.beep(Config::Buzzer::bootFreqHz, Config::Buzzer::bootMs);
                    break;

                case ControllerState::Calibrating:
                    _calibrator.begin();
                    _lastTickMs = now;      // first tick one interval after entry
                    break;

                case ControllerState::Calibrated:
                    _out.beep(Config::Buzzer::confirmFreqHz, Config::Buzzer::confirmMs);
                    break;

                case ControllerState::Idle:
                    _blockedCount = 0;
                    _out.beep(Config::Buzzer::armedFreqHz, Config::Buzzer::armedMs);
                    break;

                case ControllerState::Alarm:
                    alarmBeep(now);
                    break;

                case ControllerState::Sleep:
                    _out.silence();
                    break;
            }
        }

        // postSleep calibrations are checked against the reference and never replace it
        void startCalibration(bool postSleep, uint32_t now) {
            _postSleep = postSleep;
            setState(ControllerState::Calibrating, now);
        }

        void finishCalibration(uint32_t now) {
            const uint32_t cm = _calibrator.baseline();

            if (!_postSleep) {
                _referenceBaseline = cm;
                _activeBaseline = cm;
                _out.baselineAccepted(cm, true);
                setState(ControllerState::Calibrated, now);
                return;
            }

            if (cm + Config::Distance::blockMarginCm < _referenceBaseline) {
                _out.baselineRejected(cm, _referenceBaseline);
                setState(ControllerState::Alarm, now);
                return;
            }

            _activeBaseline = cm;
            _out.baselineAccepted(cm, false);
            setState(ControllerState::Calibrated, now);
        }

        // level-triggered: confirmReadings blocked readings in a row
        bool isBlocked(uint32_t cm) {
            const bool blocked = cm > 0 && cm + Config::Distance::blockMarginCm < _activeBaseline;
            _blockedCount = blocked ? _blockedCount + 1 : 0;
            return _blockedCount >= Config::Distance::confirmReadings;
        }

        void alarmBeep(uint32_t now) {
            _lastAlarmBeepMs = now;
            _out.beep(Config::Buzzer::alarmFreqHz, Config::Buzzer::alarmBeepMs);
        }

        ControllerOutput& _out;
        Calibrator _calibrator;

        ControllerState _state = ControllerState::Uncalibrated;
        uint32_t _enteredMs = 0;
        bool _postSleep = false;

        uint32_t _referenceBaseline = 0;    // set only by a long-press calibration
        uint32_t _activeBaseline = 0;       // what detection compares against

        uint8_t _blockedCount = 0;
        uint32_t _lastTickMs = 0;
        uint32_t _lastAlarmBeepMs = 0;
};

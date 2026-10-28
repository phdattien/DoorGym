#pragma once

#include <stdint.h>
#include "../Config.hpp"
#include "calibrator.hpp"

// Pure state machine: no Arduino dependency, so it runs in native tests.
// Hardware is wired in by the sketch through Inputs and ControllerOutput.

enum class ControllerState { Uncalibrated, Calibrating, Calibrated, Idle, Alarm };

class ControllerOutput {
    public:
        virtual void beep(uint16_t freqHz, uint32_t durationMs) = 0;
        virtual void silence() = 0;
        virtual void stateEntered(ControllerState state) = 0;
        virtual void baselineAccepted(uint32_t baselineCm, bool isReference) = 0;
        virtual void baselineRejected(uint32_t baselineCm, uint32_t referenceCm) = 0;
        virtual void baselineTooShort(uint32_t baselineCm) = 0;
        virtual void calibrationTimedOut() = 0;

    protected:
        ~ControllerOutput() = default;  // never deleted through the interface
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
            if (in.pressed) _stopPressHeld = false;
            const bool longPressed = in.longPressed && !_stopPressHeld;

            switch (_state) {
                case ControllerState::Uncalibrated:
                    if (longPressed) startCalibration(false, in.now);
                    break;

                case ControllerState::Calibrating:
                    if (longPressed) {
                        startCalibration(false, in.now);
                        break;
                    }
                    if (_afterAlarm && in.now - _enteredMs >= Config::Calibration::afterAlarmTimeoutMs) {
                        _out.calibrationTimedOut();
                        setState(ControllerState::Alarm, in.now);
                        break;
                    }
                    if (in.hasReading) {
                        _calibrator.update(in.distanceCm, in.now);
                        if (_calibrator.isStable()) finishCalibration(in.now);
                    }
                    break;

                case ControllerState::Calibrated:
                    if (in.now - _enteredMs >= Config::Timing::armDelayMs) {
                        setState(ControllerState::Idle, in.now);
                    }
                    break;

                case ControllerState::Idle:
                    if (longPressed) {
                        startCalibration(false, in.now);
                        break;
                    }
                    if (in.hasReading && isBlocked(in.distanceCm)) {
                        setState(ControllerState::Alarm, in.now);
                    }
                    break;

                case ControllerState::Alarm:
                    if (in.pressed || in.shortPressed || in.longPressed) {
                        _stopPressHeld = true;      // holding on must not count as a recalibrate
                        startCalibration(true, in.now);
                        break;
                    }
                    if (every(Config::Buzzer::alarmMelody[_note].durationMs, in.now)) {
                        _note = (_note + 1) % Config::Buzzer::alarmMelodyLength;
                        playNote();
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
            _lastPeriodMs = now;
            _out.stateEntered(_state);

            switch (_state) {
                case ControllerState::Uncalibrated:
                    _out.beep(Config::Buzzer::bootFreqHz, Config::Buzzer::bootMs);
                    break;

                case ControllerState::Calibrating:
                    if (_afterAlarm) _out.silence();
                    _out.beep(Config::Buzzer::calibrateStartFreqHz, Config::Buzzer::calibrateStartMs);
                    _calibrator.begin();
                    break;

                case ControllerState::Calibrated:
                    _out.beep(Config::Buzzer::confirmFreqHz, Config::Buzzer::confirmMs);
                    break;

                case ControllerState::Idle:
                    _blockedCount = 0;
                    break;

                case ControllerState::Alarm:
                    _note = 0;
                    playNote();
                    break;
            }
        }

        // afterAlarm calibrations are checked against the reference and never replace it
        void startCalibration(bool afterAlarm, uint32_t now) {
            _afterAlarm = afterAlarm;
            setState(ControllerState::Calibrating, now);
        }

        // an unusable baseline restarts the calibrator and stays in Calibrating
        void finishCalibration(uint32_t now) {
            const uint32_t cm = _calibrator.baseline();

            if (_afterAlarm && closerThan(cm, _referenceBaseline)) {
                _out.baselineRejected(cm, _referenceBaseline);
                setState(ControllerState::Alarm, now);
                return;
            }

            if (cm <= Config::Distance::blockMarginCm) {    // nothing could ever be closer
                _out.baselineTooShort(cm);
                _out.beep(Config::Buzzer::errorFreqHz, Config::Buzzer::errorMs);
                _calibrator.begin();
                return;
            }

            if (!_afterAlarm) _referenceBaseline = cm;
            _activeBaseline = cm;
            _out.baselineAccepted(cm, !_afterAlarm);
            setState(ControllerState::Calibrated, now);
        }

        // level-triggered: confirmReadings blocked readings in a row
        bool isBlocked(uint32_t cm) {
            if (cm == 0) {      // no echo: neither blocked nor clear, keep the count
                return false;
            }
            _blockedCount = closerThan(cm, _activeBaseline) ? _blockedCount + 1 : 0;
            return _blockedCount >= Config::Distance::confirmReadings;
        }

        // more than blockMarginCm closer than baselineCm
        static bool closerThan(uint32_t cm, uint32_t baselineCm) {
            return cm + Config::Distance::blockMarginCm < baselineCm;
        }

        // true once intervalMs has passed since entering the state or the last time this returned true
        bool every(uint32_t intervalMs, uint32_t now) {
            if (now - _lastPeriodMs < intervalMs) {
                return false;
            }
            _lastPeriodMs = now;
            return true;
        }

        void playNote() {
            const Config::Buzzer::Note& note = Config::Buzzer::alarmMelody[_note];
            if (note.freqHz != 0) {
                _out.beep(note.freqHz, note.durationMs - Config::Buzzer::noteGapMs);
            }
        }

        ControllerOutput& _out;
        Calibrator _calibrator;

        ControllerState _state = ControllerState::Uncalibrated;
        uint32_t _enteredMs = 0;
        bool _afterAlarm = false;
        bool _stopPressHeld = false;        // the press that stopped the alarm is still down

        uint32_t _referenceBaseline = 0;    // set only by a long-press calibration
        uint32_t _activeBaseline = 0;       // what detection compares against

        uint8_t _blockedCount = 0;
        uint32_t _lastPeriodMs = 0;
        uint8_t _note = 0;                  // current alarm melody note
};

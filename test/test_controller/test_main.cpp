#include <unity.h>
#include <vector>
#include "modules/control.hpp"

namespace B = Config::Buzzer;

struct Beep {
    uint32_t at;
    uint16_t freqHz;
    uint32_t durationMs;
};

class FakeOutput : public ControllerOutput {
    public:
        void beep(uint16_t freqHz, uint32_t durationMs) override {
            beeps.push_back({*now, freqHz, durationMs});
        }

        void silence() override {
            silences++;
        }

        void stateEntered(ControllerState state) override {
            states.push_back(state);
        }

        void baselineAccepted(uint32_t baselineCm, bool isReference) override {
            accepted.push_back(baselineCm);
            lastAcceptedIsReference = isReference;
        }

        void baselineRejected(uint32_t baselineCm, uint32_t referenceCm) override {
            rejectedCm = baselineCm;
            rejectedReferenceCm = referenceCm;
        }

        void baselineTooShort(uint32_t baselineCm) override {
            tooShortCm = baselineCm;
        }

        void calibrationTimedOut() override {
            timedOut = true;
        }

        size_t count(uint16_t freqHz) {
            size_t n = 0;
            for (const Beep& b : beeps) n += b.freqHz == freqHz;
            return n;
        }

        const uint32_t* now = nullptr;
        std::vector<Beep> beeps;
        std::vector<ControllerState> states;
        std::vector<uint32_t> accepted;
        bool lastAcceptedIsReference = false;
        uint32_t rejectedCm = 0;
        uint32_t rejectedReferenceCm = 0;
        uint32_t tooShortCm = 0;
        bool timedOut = false;
        int silences = 0;
};

// drives the controller with a scripted timeline, one update per step
struct Sim {
    static const uint32_t stepMs = 50;

    FakeOutput out;
    Controller controller{out};
    uint32_t now = 0;

    Sim() {
        out.now = &now;
        controller.begin(now);
    }

    void update(Controller::Inputs in) {
        in.now = now;
        controller.update(in);
    }

    // advances time by one step, then updates
    void step(Controller::Inputs in) {
        now += stepMs;
        update(in);
    }

    // one reading per step for durationMs
    void feed(uint32_t cm, uint32_t durationMs) {
        Controller::Inputs in;
        in.hasReading = true;
        in.distanceCm = cm;
        for (uint32_t t = 0; t < durationMs; t += stepMs) {
            step(in);
        }
    }

    // cycles through readings, one per step, for durationMs
    void feedPattern(std::vector<uint32_t> cms, uint32_t durationMs) {
        size_t i = 0;
        for (uint32_t t = 0; t < durationMs; t += stepMs) {
            feed(cms[i++ % cms.size()], stepMs);
        }
    }

    void wait(uint32_t durationMs) {
        for (uint32_t t = 0; t < durationMs; t += stepMs) {
            step(Controller::Inputs());
        }
    }

    // a single button event at the current time
    void press(bool Controller::Inputs::*event) {
        Controller::Inputs in;
        in.*event = true;
        update(in);
    }

    void pressDown()  { press(&Controller::Inputs::pressed); }
    void shortPress() { press(&Controller::Inputs::shortPressed); }
    void longPress()  { press(&Controller::Inputs::longPressed); }

    ControllerState state() {
        return controller.state();
    }

    void calibrateToIdle(uint32_t cm) {
        longPress();
        feed(cm, Config::Calibration::stableWindowMs + 200);
        wait(Config::Timing::armDelayMs);
    }

    void alarmFromIdle() {
        feed(controller.activeBaseline() - 30, Config::Distance::confirmReadings * stepMs);
    }

    // calibrated at 100 cm, alarmed, stopped: now after-alarm calibrating
    void stopAlarm() {
        calibrateToIdle(100);
        alarmFromIdle();
        pressDown();
    }
};

#define ASSERT_STATE(expected, sim) TEST_ASSERT_EQUAL_INT((int)(expected), (int)(sim).state())

void setUp() {}
void tearDown() {}

void test_boot_beeps_once_and_stays_uncalibrated() {
    Sim sim;
    TEST_ASSERT_EQUAL(1, sim.out.beeps.size());
    TEST_ASSERT_EQUAL(B::bootFreqHz, sim.out.beeps[0].freqHz);

    sim.shortPress();
    sim.pressDown();
    sim.feed(20, 10000);
    sim.feed(0, 1000);

    ASSERT_STATE(ControllerState::Uncalibrated, sim);
    TEST_ASSERT_EQUAL(1, sim.out.beeps.size());
}

void test_long_press_calibrates_then_arms_after_delay() {
    Sim sim;
    sim.longPress();
    ASSERT_STATE(ControllerState::Calibrating, sim);
    TEST_ASSERT_EQUAL(B::calibrateStartFreqHz, sim.out.beeps.back().freqHz);

    sim.feedPattern({100, 95, 105}, 5100);
    ASSERT_STATE(ControllerState::Calibrated, sim);
    TEST_ASSERT_EQUAL(1, sim.out.accepted.size());
    TEST_ASSERT_UINT32_WITHIN(1, 100, sim.out.accepted[0]);
    TEST_ASSERT_TRUE(sim.out.lastAcceptedIsReference);
    TEST_ASSERT_EQUAL(B::confirmFreqHz, sim.out.beeps.back().freqHz);
    TEST_ASSERT_EQUAL(B::confirmMs, sim.out.beeps.back().durationMs);

    const size_t beeps = sim.out.beeps.size();
    sim.wait(Config::Timing::armDelayMs - 100);
    ASSERT_STATE(ControllerState::Calibrated, sim);
    sim.wait(200);
    ASSERT_STATE(ControllerState::Idle, sim);
    TEST_ASSERT_EQUAL(beeps, sim.out.beeps.size());     // arming is silent
}

void test_out_of_band_reading_restarts_window() {
    Sim sim;
    sim.longPress();
    sim.feed(100, 4900);
    sim.feed(120, 4900);     // first 120 is a new anchor
    ASSERT_STATE(ControllerState::Calibrating, sim);

    sim.feed(120, 200);
    ASSERT_STATE(ControllerState::Calibrated, sim);
    TEST_ASSERT_EQUAL(120, sim.out.accepted[0]);
}

void test_no_echo_restarts_window() {
    Sim sim;
    sim.longPress();
    sim.feed(100, 4900);
    sim.feed(0, 50);
    sim.feed(100, 4900);
    ASSERT_STATE(ControllerState::Calibrating, sim);

    sim.feed(100, 200);
    ASSERT_STATE(ControllerState::Calibrated, sim);
}

void test_calibrating_is_silent_and_never_times_out() {
    Sim sim;
    sim.longPress();
    const size_t before = sim.out.beeps.size();
    sim.feedPattern({50, 150}, 60000);

    ASSERT_STATE(ControllerState::Calibrating, sim);
    TEST_ASSERT_EQUAL(before, sim.out.beeps.size());
}

void test_baseline_too_short_errors_and_keeps_calibrating() {
    Sim sim;
    sim.longPress();
    sim.feed(10, 5200);
    ASSERT_STATE(ControllerState::Calibrating, sim);
    TEST_ASSERT_EQUAL(10, sim.out.tooShortCm);
    TEST_ASSERT_EQUAL(1, sim.out.count(B::errorFreqHz));
    TEST_ASSERT_EQUAL(0, sim.out.accepted.size());

    sim.feed(100, 5200);
    ASSERT_STATE(ControllerState::Calibrated, sim);
    TEST_ASSERT_EQUAL(100, sim.out.accepted[0]);
}

void test_short_press_ignored_while_calibrating() {
    Sim sim;
    sim.longPress();
    sim.feed(100, 2000);
    sim.shortPress();
    sim.feed(100, 3200);
    ASSERT_STATE(ControllerState::Calibrated, sim);
}

void test_long_press_restarts_calibration() {
    Sim sim;
    sim.longPress();
    sim.feed(100, 4000);
    sim.longPress();
    sim.feed(100, 4000);
    ASSERT_STATE(ControllerState::Calibrating, sim);

    sim.feed(100, 1200);
    ASSERT_STATE(ControllerState::Calibrated, sim);
}

void test_button_ignored_during_arm_delay() {
    Sim sim;
    sim.longPress();
    sim.feed(100, 5200);
    sim.shortPress();
    sim.longPress();
    ASSERT_STATE(ControllerState::Calibrated, sim);
}

void test_blocked_needs_three_readings_in_a_row() {
    Sim sim;
    sim.calibrateToIdle(100);
    ASSERT_STATE(ControllerState::Idle, sim);

    sim.feed(80, 100);       // two readings
    sim.feed(100, 50);
    ASSERT_STATE(ControllerState::Idle, sim);

    sim.feed(80, 150);       // three readings
    ASSERT_STATE(ControllerState::Alarm, sim);
    TEST_ASSERT_EQUAL(B::alarmMelody[0].freqHz, sim.out.beeps.back().freqHz);
}

void test_small_changes_and_no_echo_never_alarm() {
    Sim sim;
    sim.calibrateToIdle(100);
    sim.feed(90, 5000);      // within the 15 cm margin
    sim.feed(85, 5000);      // exactly the margin, not more
    sim.feed(0, 5000);
    sim.shortPress();
    ASSERT_STATE(ControllerState::Idle, sim);
}

void test_no_echo_between_blocked_readings_still_alarms() {
    Sim sim;
    sim.calibrateToIdle(100);
    sim.feedPattern({80, 0}, 250);      // 80, 0, 80, 0, 80
    ASSERT_STATE(ControllerState::Alarm, sim);
}

void test_alarm_plays_melody_on_repeat_until_press() {
    Sim sim;
    sim.calibrateToIdle(100);
    const size_t before = sim.out.beeps.size();
    sim.alarmFromIdle();
    const uint32_t start = sim.now;

    uint32_t roundMs = 0;
    std::vector<uint16_t> tune;
    for (const B::Note& note : B::alarmMelody) {
        roundMs += note.durationMs;
        if (note.freqHz != 0) tune.push_back(note.freqHz);
    }

    sim.wait(2 * roundMs - 50);         // two full rounds
    std::vector<uint16_t> played;
    for (size_t i = before; i < sim.out.beeps.size(); i++) played.push_back(sim.out.beeps[i].freqHz);

    TEST_ASSERT_EQUAL(2 * tune.size(), played.size());
    for (size_t i = 0; i < played.size(); i++) {
        TEST_ASSERT_EQUAL(tune[i % tune.size()], played[i]);
    }
    TEST_ASSERT_EQUAL(start + roundMs, sim.out.beeps[before + tune.size()].at);   // second round on time

    sim.pressDown();
    ASSERT_STATE(ControllerState::Calibrating, sim);
    TEST_ASSERT_EQUAL(1, sim.out.silences);
    TEST_ASSERT_EQUAL(B::calibrateStartFreqHz, sim.out.beeps.back().freqHz);
}

void test_holding_the_stopping_press_does_not_replace_reference() {
    Sim sim;
    sim.stopAlarm();
    sim.longPress();         // same press, still held for longPressMs
    sim.feed(150, 5200);
    TEST_ASSERT_FALSE(sim.out.lastAcceptedIsReference);
    TEST_ASSERT_EQUAL(100, sim.controller.referenceBaseline());

    sim.wait(Config::Timing::armDelayMs);
    sim.pressDown();         // a new press can recalibrate again
    sim.longPress();
    sim.feed(150, 5200);
    TEST_ASSERT_TRUE(sim.out.lastAcceptedIsReference);
    TEST_ASSERT_EQUAL(150, sim.controller.referenceBaseline());
}

void test_after_alarm_still_blocked_goes_back_to_alarm() {
    Sim sim;
    sim.stopAlarm();

    sim.feed(70, 5200);
    ASSERT_STATE(ControllerState::Alarm, sim);
    TEST_ASSERT_EQUAL(70, sim.out.rejectedCm);
    TEST_ASSERT_EQUAL(100, sim.out.rejectedReferenceCm);
    TEST_ASSERT_EQUAL(100, sim.controller.activeBaseline());
}

void test_after_alarm_drift_accepted_without_moving_reference() {
    Sim sim;
    sim.stopAlarm();

    sim.feed(95, 5200);
    ASSERT_STATE(ControllerState::Calibrated, sim);
    TEST_ASSERT_FALSE(sim.out.lastAcceptedIsReference);
    TEST_ASSERT_EQUAL(95, sim.controller.activeBaseline());
    TEST_ASSERT_EQUAL(100, sim.controller.referenceBaseline());

    sim.wait(5000);
    ASSERT_STATE(ControllerState::Idle, sim);
}

void test_after_alarm_calibration_that_never_settles_alarms() {
    Sim sim;
    sim.stopAlarm();
    sim.feedPattern({50, 150}, Config::Calibration::afterAlarmTimeoutMs - 100);
    ASSERT_STATE(ControllerState::Calibrating, sim);

    sim.feedPattern({50, 150}, 200);
    ASSERT_STATE(ControllerState::Alarm, sim);
    TEST_ASSERT_TRUE(sim.out.timedOut);
}

void test_still_blocked_on_return_to_idle_alarms_again() {
    Sim sim;
    sim.stopAlarm();
    sim.feed(100, 5200);
    sim.wait(5000);
    ASSERT_STATE(ControllerState::Idle, sim);

    sim.feed(60, 150);
    ASSERT_STATE(ControllerState::Alarm, sim);
}

void test_long_press_in_idle_replaces_reference() {
    Sim sim;
    sim.calibrateToIdle(100);
    sim.shortPress();
    ASSERT_STATE(ControllerState::Idle, sim);

    sim.longPress();
    ASSERT_STATE(ControllerState::Calibrating, sim);
    sim.feed(150, 5200);
    TEST_ASSERT_TRUE(sim.out.lastAcceptedIsReference);
    TEST_ASSERT_EQUAL(150, sim.controller.referenceBaseline());
    TEST_ASSERT_EQUAL(150, sim.controller.activeBaseline());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_boot_beeps_once_and_stays_uncalibrated);
    RUN_TEST(test_long_press_calibrates_then_arms_after_delay);
    RUN_TEST(test_out_of_band_reading_restarts_window);
    RUN_TEST(test_no_echo_restarts_window);
    RUN_TEST(test_calibrating_is_silent_and_never_times_out);
    RUN_TEST(test_baseline_too_short_errors_and_keeps_calibrating);
    RUN_TEST(test_short_press_ignored_while_calibrating);
    RUN_TEST(test_long_press_restarts_calibration);
    RUN_TEST(test_button_ignored_during_arm_delay);
    RUN_TEST(test_blocked_needs_three_readings_in_a_row);
    RUN_TEST(test_small_changes_and_no_echo_never_alarm);
    RUN_TEST(test_no_echo_between_blocked_readings_still_alarms);
    RUN_TEST(test_alarm_plays_melody_on_repeat_until_press);
    RUN_TEST(test_holding_the_stopping_press_does_not_replace_reference);
    RUN_TEST(test_after_alarm_still_blocked_goes_back_to_alarm);
    RUN_TEST(test_after_alarm_drift_accepted_without_moving_reference);
    RUN_TEST(test_after_alarm_calibration_that_never_settles_alarms);
    RUN_TEST(test_still_blocked_on_return_to_idle_alarms_again);
    RUN_TEST(test_long_press_in_idle_replaces_reference);
    return UNITY_END();
}

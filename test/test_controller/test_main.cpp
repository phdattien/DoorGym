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

    // one reading per step for durationMs
    void feed(uint32_t cm, uint32_t durationMs) {
        for (uint32_t t = 0; t < durationMs; t += stepMs) {
            now += stepMs;
            Controller::Inputs in;
            in.hasReading = true;
            in.distanceCm = cm;
            update(in);
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
            now += stepMs;
            update(Controller::Inputs());
        }
    }

    void pressDown() {
        Controller::Inputs in;
        in.pressed = true;
        update(in);
    }

    void shortPress() {
        Controller::Inputs in;
        in.shortPressed = true;
        update(in);
    }

    void longPress() {
        Controller::Inputs in;
        in.longPressed = true;
        update(in);
    }

    ControllerState state() {
        return controller.state();
    }

    void calibrateToIdle(uint32_t cm) {
        longPress();
        feed(cm, Config::Calibration::stableWindowMs + 200);
        wait(Config::Timing::armDelayMs);
    }

    void alarmFromIdle() {
        feed(controller.activeBaseline() - 30, 3 * stepMs);
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

    sim.feedPattern({100, 95, 105}, 5100);
    ASSERT_STATE(ControllerState::Calibrated, sim);
    TEST_ASSERT_EQUAL(1, sim.out.accepted.size());
    TEST_ASSERT_UINT32_WITHIN(1, 100, sim.out.accepted[0]);
    TEST_ASSERT_TRUE(sim.out.lastAcceptedIsReference);
    TEST_ASSERT_EQUAL(B::confirmFreqHz, sim.out.beeps.back().freqHz);
    TEST_ASSERT_EQUAL(B::confirmMs, sim.out.beeps.back().durationMs);

    sim.wait(4900);
    ASSERT_STATE(ControllerState::Calibrated, sim);
    sim.wait(200);
    ASSERT_STATE(ControllerState::Idle, sim);
    TEST_ASSERT_EQUAL(B::armedFreqHz, sim.out.beeps.back().freqHz);
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

void test_calibrating_ticks_every_second_and_never_times_out() {
    Sim sim;
    sim.longPress();
    sim.feedPattern({50, 150}, 60000);

    ASSERT_STATE(ControllerState::Calibrating, sim);
    TEST_ASSERT_EQUAL(60, sim.out.count(B::tickFreqHz));
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
    TEST_ASSERT_EQUAL(B::alarmFreqHz, sim.out.beeps.back().freqHz);
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

void test_alarm_beeps_every_10s_until_press() {
    Sim sim;
    sim.calibrateToIdle(100);
    sim.alarmFromIdle();
    const uint32_t start = sim.now;
    const size_t before = sim.out.count(B::alarmFreqHz);

    sim.wait(25000);
    TEST_ASSERT_EQUAL(before + 2, sim.out.count(B::alarmFreqHz));
    TEST_ASSERT_EQUAL(start + 20000, sim.out.beeps.back().at);

    sim.pressDown();
    ASSERT_STATE(ControllerState::Sleep, sim);
    TEST_ASSERT_EQUAL(1, sim.out.silences);
}

void test_sleep_ignores_input_then_recalibrates() {
    Sim sim;
    sim.calibrateToIdle(100);
    sim.alarmFromIdle();
    sim.pressDown();
    const size_t before = sim.out.beeps.size();

    sim.shortPress();        // release of the stopping press
    sim.longPress();
    sim.feed(40, 29900);
    ASSERT_STATE(ControllerState::Sleep, sim);
    TEST_ASSERT_EQUAL(before, sim.out.beeps.size());

    sim.wait(100);
    ASSERT_STATE(ControllerState::Calibrating, sim);
    TEST_ASSERT_EQUAL(B::sleepEndFreqHz, sim.out.beeps.back().freqHz);
}

void test_post_sleep_still_blocked_goes_back_to_alarm() {
    Sim sim;
    sim.calibrateToIdle(100);
    sim.alarmFromIdle();
    sim.pressDown();
    sim.wait(30000);

    sim.feed(70, 5200);
    ASSERT_STATE(ControllerState::Alarm, sim);
    TEST_ASSERT_EQUAL(70, sim.out.rejectedCm);
    TEST_ASSERT_EQUAL(100, sim.out.rejectedReferenceCm);
    TEST_ASSERT_EQUAL(100, sim.controller.activeBaseline());
}

void test_post_sleep_drift_accepted_without_moving_reference() {
    Sim sim;
    sim.calibrateToIdle(100);
    sim.alarmFromIdle();
    sim.pressDown();
    sim.wait(30000);

    sim.feed(95, 5200);
    ASSERT_STATE(ControllerState::Calibrated, sim);
    TEST_ASSERT_FALSE(sim.out.lastAcceptedIsReference);
    TEST_ASSERT_EQUAL(95, sim.controller.activeBaseline());
    TEST_ASSERT_EQUAL(100, sim.controller.referenceBaseline());

    sim.wait(5000);
    ASSERT_STATE(ControllerState::Idle, sim);
}

void test_still_blocked_on_return_to_idle_alarms_again() {
    Sim sim;
    sim.calibrateToIdle(100);
    sim.alarmFromIdle();
    sim.pressDown();
    sim.wait(30000);
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
    RUN_TEST(test_calibrating_ticks_every_second_and_never_times_out);
    RUN_TEST(test_short_press_ignored_while_calibrating);
    RUN_TEST(test_long_press_restarts_calibration);
    RUN_TEST(test_button_ignored_during_arm_delay);
    RUN_TEST(test_blocked_needs_three_readings_in_a_row);
    RUN_TEST(test_small_changes_and_no_echo_never_alarm);
    RUN_TEST(test_alarm_beeps_every_10s_until_press);
    RUN_TEST(test_sleep_ignores_input_then_recalibrates);
    RUN_TEST(test_post_sleep_still_blocked_goes_back_to_alarm);
    RUN_TEST(test_post_sleep_drift_accepted_without_moving_reference);
    RUN_TEST(test_still_blocked_on_return_to_idle_alarms_again);
    RUN_TEST(test_long_press_in_idle_replaces_reference);
    return UNITY_END();
}

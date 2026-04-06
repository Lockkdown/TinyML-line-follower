// file: firmware/src/cnn_recovery.cpp
// purpose: maneuver/pause recovery cycles when class is nothing (3)
// dependencies: cnn_recovery.h, controller.h, motor.h

#include "cnn_recovery.h"

#include <Arduino.h>

#include "controller.h"
#include "motor.h"

namespace {

enum class Phase : uint8_t { MANEUVER, PAUSE };

struct {
    int lost_line_counter;
    Phase phase;
    int phase_counter;
    int cycle_idx;
    bool permanent_stop;
} g;

void reset_internal(void) {
    g.lost_line_counter = 0;
    g.phase = Phase::MANEUVER;
    g.phase_counter = 0;
    g.cycle_idx = 0;
    g.permanent_stop = false;
}

CnnRecoveryResult run_maneuver(float conf) {
    classToAction(3, conf);
    g.phase_counter++;
    if (g.phase_counter >= RECOVERY_MANEUVER_FRAMES) {
        g.phase = Phase::PAUSE;
        g.phase_counter = 0;
        setMotor(0, 0);
    }
    return CnnRecoveryResult::HandledReturn;
}

CnnRecoveryResult handle_coast(int* cls, int prev_class) {
    *cls = prev_class;
    if constexpr (HYST_VERBOSE_LOG) {
        Serial.printf(
            "[HYST] Ignoring noise cls=3 (lost %d/%d), coasting as cls=%d\n",
            g.lost_line_counter,
            RECOVERY_HOLD_FRAMES,
            *cls);
    }
    return CnnRecoveryResult::ProceedWithHysteresis;
}

CnnRecoveryResult run_pause(void) {
    setMotor(0, 0);
    g.phase_counter++;
    if (g.phase_counter < RECOVERY_PAUSE_FRAMES) {
        return CnnRecoveryResult::HandledReturn;
    }
    g.cycle_idx++;
    if (g.cycle_idx >= RECOVERY_MAX_CYCLES) {
        g.permanent_stop = true;
        setMotor(0, 0);
        if constexpr (HYST_VERBOSE_LOG) {
            Serial.printf("[HYST] RECOVERY max cycles (%d). Stopping motors.\n", RECOVERY_MAX_CYCLES);
        }
        return CnnRecoveryResult::HandledReturn;
    }
    g.phase = Phase::MANEUVER;
    g.phase_counter = 0;
    return CnnRecoveryResult::HandledReturn;
}

}  // namespace

void cnn_recovery_reset(void) {
    reset_internal();
}

bool cnn_recovery_should_grace_before_reset(void) {
    return g.lost_line_counter >= RECOVERY_HOLD_FRAMES;
}

CnnRecoveryResult cnn_recovery_process_class3(int* cls, int prev_class, float conf) {
    g.lost_line_counter++;
    if (g.lost_line_counter < RECOVERY_HOLD_FRAMES) {
        return handle_coast(cls, prev_class);
    }
    g.lost_line_counter = RECOVERY_HOLD_FRAMES;
    if (g.permanent_stop) {
        setMotor(0, 0);
        return CnnRecoveryResult::HandledReturn;
    }
    if (g.phase == Phase::MANEUVER) {
        return run_maneuver(conf);
    }
    return run_pause();
}

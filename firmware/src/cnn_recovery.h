// file: firmware/src/cnn_recovery.h
// purpose: CNN line-loss recovery state machine (maneuver / pause cycles)
// dependencies: controller.h

#pragma once

#include <stdint.h>

enum class CnnRecoveryResult : uint8_t {
    ProceedWithHysteresis,
    HandledReturn,
};

void cnn_recovery_reset(void);
bool cnn_recovery_should_grace_before_reset(void);
CnnRecoveryResult cnn_recovery_process_class3(int* cls, int prev_class, float conf);

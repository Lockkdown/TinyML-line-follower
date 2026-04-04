// file: firmware/src/controller.h
// purpose: declarations for CNN class to motor action mapping
// dependencies: motor.h

#pragma once

#include <stdint.h>

constexpr int CNN_SPEED_FORWARD          = 100;
constexpr int CNN_SPEED_TURN_LEFT_OUTER  = 165;
constexpr int CNN_SPEED_TURN_LEFT_INNER  = 58;
constexpr int CNN_SPEED_TURN_RIGHT_OUTER = 130;
constexpr int CNN_SPEED_TURN_RIGHT_INNER = 82;
constexpr int CNN_SPEED_COAST            = 65;
constexpr int CNN_MOTOR_RAMP_STEP        = 50;
constexpr float CNN_CONFIDENCE_THRESHOLD = 0.40f;
constexpr float CNN_LEFT_TRIM            = 1.15f;
constexpr float CNN_RIGHT_TRIM           = 1.0f;

// Reduced from 2 → 1: at ~38ms/frame, HOLD_FRAMES=2 adds 76ms lag before turning
// which is enough to overshoot a 90° junction.  With HOLD_FRAMES=1 lag is ~38ms.
constexpr int HOLD_FRAMES       = 1;
constexpr int CNN_INDICATOR_LED = 48;   // GPIO48 — verify on GOOUUU ESP32-S3-CAM before flash
constexpr int CNN_CONF_PWM_MIN  = 120;
constexpr int CNN_CONF_PWM_MAX  = 200;

// 1 = log mỗi frame (giống trước, nặng UART); tăng lên 5–10 nếu cần FPS cao hơn
constexpr int CNN_LOG_PREDICTION_EVERY_FRAMES = 1;

void classToAction(int cls, float conf);
void setCnnBaseSpeed(int speed);
int getCnnBaseSpeed();

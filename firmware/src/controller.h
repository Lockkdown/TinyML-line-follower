// file: firmware/src/controller.h
// purpose: declarations for CNN class to motor action mapping
// dependencies: motor.h

#pragma once

#include <stdint.h>

constexpr int CNN_SPEED_FORWARD          = 100;
constexpr float CNN_CONFIDENCE_THRESHOLD = 0.40f;
constexpr float CNN_LEFT_TRIM            = 1.15f;
constexpr float CNN_RIGHT_TRIM           = 1.0f;

// Confidence-proportional differential speed
constexpr int   BASE_SPEED = 160;   // legacy ref; runtime outer wheel uses setCnnBaseSpeed() (default 120)
constexpr int   INNER_MIN  = 50;    // inner wheel at max turn (high conf)
constexpr int   INNER_MAX  = 130;   // inner wheel at min turn (low conf)
constexpr float CONF_MIN   = 0.45f;
constexpr float CONF_MAX   = 1.0f;

// Recovery speeds when line is lost (class 3)
constexpr int RECOVERY_SPEED_FWD  = -130; // Back up if lost while going straight (needs enough power to overcome friction)
constexpr int RECOVERY_SPEED_TURN = 140;  // Spin in place speed if lost while turning

// Consecutive 'nothing' frames before confirming line loss (~105ms @ 28 FPS).
constexpr int RECOVERY_HOLD_FRAMES = 3;

// Backup/spin duration per recovery cycle (~535ms @ 28 FPS).
constexpr int RECOVERY_MANEUVER_FRAMES = 15;

// Full stop between maneuver attempts (~715ms @ 28 FPS).
constexpr int RECOVERY_PAUSE_FRAMES = 20;

// Number of maneuver+pause cycles before permanent stop.
constexpr int RECOVERY_MAX_CYCLES = 3;

// Frames to ignore 'nothing' after re-acquiring line (~360ms @ 28 FPS).
constexpr int RECOVERY_GRACE_FRAMES = 10;

// Reduced from 2 → 1: at ~38ms/frame, HOLD_FRAMES=2 adds 76ms lag before turning
// which is enough to overshoot a 90° junction.  With HOLD_FRAMES=1 lag is ~38ms.
constexpr int HOLD_FRAMES       = 1;
constexpr int CNN_INDICATOR_LED = 48;   // GPIO48 — verify on GOOUUU ESP32-S3-CAM before flash

// 1 = mỗi frame (nặng UART). ~3 ≈ 105ms @ 28 FPS; tăng để giảm chặn serial.
constexpr int CNN_LOG_PREDICTION_EVERY_FRAMES = 3;

// true = Serial sau mỗi Invoke (nặng UART; tắt khi cần FPS cao).
constexpr bool CNN_LOG_INFERENCE_EVERY_FRAME = false;

// Set true to print left/right PWM each classToAction (UART-heavy; use when diagnosing motor vs CNN).
constexpr bool CNN_VERBOSE_MOTOR_LOG = false;

// Set true for [HYST] Serial lines in CNN loop (UART-heavy; keep false for steady FPS).
constexpr bool HYST_VERBOSE_LOG = false;

void classToAction(int cls, float conf);
void setCnnBaseSpeed(int speed);
int getCnnBaseSpeed();

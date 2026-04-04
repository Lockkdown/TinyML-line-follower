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
constexpr int   BASE_SPEED = 160;   // forward & outer wheel speed
constexpr int   INNER_MIN  = 50;    // inner wheel at max turn (high conf)
constexpr int   INNER_MAX  = 130;   // inner wheel at min turn (low conf)
constexpr float CONF_MIN   = 0.45f;
constexpr float CONF_MAX   = 1.0f;

// Recovery speeds when line is lost (class 3)
constexpr int RECOVERY_SPEED_FWD  = -130; // Back up if lost while going straight (needs enough power to overcome friction)
constexpr int RECOVERY_SPEED_TURN = 140;  // Spin in place speed if lost while turning

// Number of consecutive 'nothing' frames required to trigger recovery mode.
// Filters out noise/glitches (1 frame) while still reacting to real line loss.
constexpr int RECOVERY_HOLD_FRAMES = 5;

// Maximum frames to stay in recovery mode before giving up and stopping (e.g., 50 frames ~ 1.5s)
constexpr int RECOVERY_TIMEOUT_FRAMES = 50;

// Frames to ignore 'nothing' class immediately after finding a line, to allow the robot to stabilize
constexpr int RECOVERY_GRACE_FRAMES = 15;

// Reduced from 2 → 1: at ~38ms/frame, HOLD_FRAMES=2 adds 76ms lag before turning
// which is enough to overshoot a 90° junction.  With HOLD_FRAMES=1 lag is ~38ms.
constexpr int HOLD_FRAMES       = 1;
constexpr int CNN_INDICATOR_LED = 48;   // GPIO48 — verify on GOOUUU ESP32-S3-CAM before flash

// 1 = log mỗi frame (giống trước, nặng UART); tăng lên 5–10 nếu cần FPS cao hơn
constexpr int CNN_LOG_PREDICTION_EVERY_FRAMES = 1;

void classToAction(int cls, float conf);
void setCnnBaseSpeed(int speed);
int getCnnBaseSpeed();

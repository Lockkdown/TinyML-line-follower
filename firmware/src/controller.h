// file: firmware/src/controller.h
// purpose: declarations for CNN class to motor action mapping
// dependencies: motor.h

#pragma once

#include <stdint.h>

constexpr int CNN_SPEED_FORWARD = 135;
constexpr int CNN_SPEED_TURN_LEFT_OUTER = 165;
constexpr int CNN_SPEED_TURN_LEFT_INNER = 58;
constexpr int CNN_SPEED_TURN_RIGHT_OUTER = 130;
constexpr int CNN_SPEED_TURN_RIGHT_INNER = 82;
constexpr int CNN_SPEED_COAST = 65;
constexpr int CNN_MOTOR_RAMP_STEP = 50;
constexpr int CNN_MOMENTUM_FRAMES = 3;
constexpr float CNN_CONFIDENCE_THRESHOLD = 0.40f;
constexpr float CNN_LEFT_TRIM = 1.15f;
constexpr float CNN_RIGHT_TRIM = 1.0f;

void classToAction(int classIndex);
void setCnnBaseSpeed(int speed);
int getCnnBaseSpeed();

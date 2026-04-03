#pragma once

// Motor pin assignments — match wiring_diagram.md §3.4 and codebase-context.md §8 exactly
constexpr int MOTOR_LEFT_IN1  = 1;
constexpr int MOTOR_LEFT_IN2  = 2;
constexpr int MOTOR_LEFT_ENA  = 14;
constexpr int MOTOR_RIGHT_IN3 = 42;
constexpr int MOTOR_RIGHT_IN4 = 41;
constexpr int MOTOR_RIGHT_ENB = 21;

// PWM config — 1000Hz, 8-bit resolution (0–255)
constexpr int PWM_FREQ          = 1000;
constexpr int PWM_RESOLUTION    = 8;
constexpr int PWM_CHANNEL_LEFT  = 2;
constexpr int PWM_CHANNEL_RIGHT = 3;

void initMotor();
void setMotor(int left_speed, int right_speed);
void setMotorSmooth(int left_speed, int right_speed, int ramp_step);
void setMotorTrim(float left_trim, float right_trim);
float getLeftTrim();
float getRightTrim();

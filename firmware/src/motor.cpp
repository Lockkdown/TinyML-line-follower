#include "motor.h"
#include <Arduino.h>

static int clampSpeed(int speed) {
    if (speed >  255) return  255;
    if (speed < -255) return -255;
    return speed;
}

static float g_left_trim = 1.15f;
static float g_right_trim = 1.0f;
static int g_last_left_speed = 0;
static int g_last_right_speed = 0;

static float clampTrim(float trim) {
    if (trim < 0.5f) return 0.5f;
    if (trim > 2.0f) return 2.0f;
    return trim;
}

static int applyTrim(int speed, float trim) {
    int trimmed = (int)(speed * trim);
    return clampSpeed(trimmed);
}

static void setLeftMotor(int speed) {
    if (speed > 0) {
        digitalWrite(MOTOR_LEFT_IN1, HIGH);
        digitalWrite(MOTOR_LEFT_IN2, LOW);
        ledcWrite(PWM_CHANNEL_LEFT, speed);
    } else if (speed < 0) {
        digitalWrite(MOTOR_LEFT_IN1, LOW);
        digitalWrite(MOTOR_LEFT_IN2, HIGH);
        ledcWrite(PWM_CHANNEL_LEFT, -speed);
    } else {
        digitalWrite(MOTOR_LEFT_IN1, LOW);
        digitalWrite(MOTOR_LEFT_IN2, LOW);
        ledcWrite(PWM_CHANNEL_LEFT, 0);
    }
}

static void setRightMotor(int speed) {
    if (speed > 0) {
        digitalWrite(MOTOR_RIGHT_IN3, HIGH);
        digitalWrite(MOTOR_RIGHT_IN4, LOW);
        ledcWrite(PWM_CHANNEL_RIGHT, speed);
    } else if (speed < 0) {
        digitalWrite(MOTOR_RIGHT_IN3, LOW);
        digitalWrite(MOTOR_RIGHT_IN4, HIGH);
        ledcWrite(PWM_CHANNEL_RIGHT, -speed);
    } else {
        digitalWrite(MOTOR_RIGHT_IN3, LOW);
        digitalWrite(MOTOR_RIGHT_IN4, LOW);
        ledcWrite(PWM_CHANNEL_RIGHT, 0);
    }
}

void initMotor() {
    pinMode(MOTOR_LEFT_IN1,  OUTPUT);
    pinMode(MOTOR_LEFT_IN2,  OUTPUT);
    pinMode(MOTOR_RIGHT_IN3, OUTPUT);
    pinMode(MOTOR_RIGHT_IN4, OUTPUT);

    ledcSetup(PWM_CHANNEL_LEFT,  PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(MOTOR_LEFT_ENA,  PWM_CHANNEL_LEFT);
    ledcSetup(PWM_CHANNEL_RIGHT, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(MOTOR_RIGHT_ENB, PWM_CHANNEL_RIGHT);

    setMotor(0, 0);
}

void setMotor(int left_speed, int right_speed) {
    int clamped_left = clampSpeed(left_speed);
    int clamped_right = clampSpeed(right_speed);
    setLeftMotor(applyTrim(clamped_left, g_left_trim));
    setRightMotor(applyTrim(clamped_right, g_right_trim));
    g_last_left_speed = clamped_left;
    g_last_right_speed = clamped_right;
}

void setMotorSmooth(int left_speed, int right_speed, int ramp_step) {
    int step = ramp_step;
    if (step < 1) {
        step = 1;
    }

    int target_left = clampSpeed(left_speed);
    int target_right = clampSpeed(right_speed);
    int next_left = g_last_left_speed;
    int next_right = g_last_right_speed;

    if (next_left < target_left) {
        next_left += step;
        if (next_left > target_left) {
            next_left = target_left;
        }
    } else if (next_left > target_left) {
        next_left -= step;
        if (next_left < target_left) {
            next_left = target_left;
        }
    }

    if (next_right < target_right) {
        next_right += step;
        if (next_right > target_right) {
            next_right = target_right;
        }
    } else if (next_right > target_right) {
        next_right -= step;
        if (next_right < target_right) {
            next_right = target_right;
        }
    }

    setMotor(next_left, next_right);
}

void setMotorTrim(float left_trim, float right_trim) {
    g_left_trim = clampTrim(left_trim);
    g_right_trim = clampTrim(right_trim);
}

float getLeftTrim() {
    return g_left_trim;
}

float getRightTrim() {
    return g_right_trim;
}

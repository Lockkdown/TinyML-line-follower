#include "motor.h"
#include <Arduino.h>

static int clampSpeed(int speed) {
    if (speed >  255) return  255;
    if (speed < -255) return -255;
    return speed;
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
    setLeftMotor(clampSpeed(left_speed));
    setRightMotor(clampSpeed(right_speed));
}

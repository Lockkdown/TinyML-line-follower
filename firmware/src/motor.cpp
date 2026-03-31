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
        ledcWrite(MOTOR_LEFT_ENA, speed);
    } else if (speed < 0) {
        digitalWrite(MOTOR_LEFT_IN1, LOW);
        digitalWrite(MOTOR_LEFT_IN2, HIGH);
        ledcWrite(MOTOR_LEFT_ENA, -speed);
    } else {
        digitalWrite(MOTOR_LEFT_IN1, LOW);
        digitalWrite(MOTOR_LEFT_IN2, LOW);
        ledcWrite(MOTOR_LEFT_ENA, 0);
    }
}

static void setRightMotor(int speed) {
    if (speed > 0) {
        digitalWrite(MOTOR_RIGHT_IN3, HIGH);
        digitalWrite(MOTOR_RIGHT_IN4, LOW);
        ledcWrite(MOTOR_RIGHT_ENB, speed);
    } else if (speed < 0) {
        digitalWrite(MOTOR_RIGHT_IN3, LOW);
        digitalWrite(MOTOR_RIGHT_IN4, HIGH);
        ledcWrite(MOTOR_RIGHT_ENB, -speed);
    } else {
        digitalWrite(MOTOR_RIGHT_IN3, LOW);
        digitalWrite(MOTOR_RIGHT_IN4, LOW);
        ledcWrite(MOTOR_RIGHT_ENB, 0);
    }
}

void initMotor() {
    pinMode(MOTOR_LEFT_IN1,  OUTPUT);
    pinMode(MOTOR_LEFT_IN2,  OUTPUT);
    pinMode(MOTOR_RIGHT_IN3, OUTPUT);
    pinMode(MOTOR_RIGHT_IN4, OUTPUT);

    // Arduino core 3.x API — pin-based (no manual channel assignment)
    ledcAttach(MOTOR_LEFT_ENA,  PWM_FREQ, PWM_RESOLUTION);
    ledcAttach(MOTOR_RIGHT_ENB, PWM_FREQ, PWM_RESOLUTION);

    setMotor(0, 0);
}

void setMotor(int left_speed, int right_speed) {
    setLeftMotor(clampSpeed(left_speed));
    setRightMotor(clampSpeed(right_speed));
}

// file: firmware/src/controller.cpp
// purpose: map CNN class index to motor actions
// dependencies: controller.h, motor.h

#include <Arduino.h>
#include "controller.h"
#include "motor.h"

// Reduced from 180 → 120: lower speed gives more physical time to
// complete turns before the robot overshoots a 90° junction.
static int g_cnn_base_speed = 120;

void setCnnBaseSpeed(int speed) {
    if (speed < CNN_SPEED_FORWARD) speed = CNN_SPEED_FORWARD;
    if (speed > 255) speed = 255;
    g_cnn_base_speed = speed;
}

int getCnnBaseSpeed() {
    return g_cnn_base_speed;
}

void classToAction(int cls, float conf) {
    static bool trimInitialized = false;
    if (!trimInitialized) {
        setMotorTrim(CNN_LEFT_TRIM, CNN_RIGHT_TRIM);
        trimInitialized = true;
    }

    const int turn_outer = constrain(
        (int)(CNN_CONF_PWM_MIN + conf * 80), CNN_CONF_PWM_MIN, CNN_CONF_PWM_MAX);

    int left_speed = 0;
    int right_speed = 0;
    switch (cls) {
        case 0: // forward
            left_speed = g_cnn_base_speed;
            right_speed = g_cnn_base_speed;
            break;
        case 1: // left — inner wheel slower for proportional turn
            left_speed = CNN_SPEED_TURN_LEFT_INNER;
            right_speed = turn_outer;
            break;
        case 2: // right — inner wheel slower for proportional turn
            left_speed = turn_outer;
            right_speed = CNN_SPEED_TURN_RIGHT_INNER;
            break;
        default: // nothing or unknown
            break;
    }
    Serial.printf("[CNN] motor left=%d right=%d\n", left_speed, right_speed);
    setMotor(left_speed, right_speed);
}

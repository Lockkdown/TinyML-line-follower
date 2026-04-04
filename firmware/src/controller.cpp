// file: firmware/src/controller.cpp
// purpose: map CNN class index to motor actions
// dependencies: controller.h, motor.h

#include <Arduino.h>
#include "controller.h"
#include "motor.h"

static int g_cnn_base_speed = 180;

void setCnnBaseSpeed(int speed) {
    if (speed < CNN_SPEED_FORWARD) speed = CNN_SPEED_FORWARD;
    if (speed > 255) speed = 255;
    g_cnn_base_speed = speed;
}

int getCnnBaseSpeed() {
    return g_cnn_base_speed;
}

void classToAction(int classIndex) {
    static bool trimInitialized = false;
    if (!trimInitialized) {
        setMotorTrim(CNN_LEFT_TRIM, CNN_RIGHT_TRIM);
        trimInitialized = true;
    }

    const int base = g_cnn_base_speed;
    const int coast = (base / 2 > CNN_SPEED_COAST) ? base / 2 : CNN_SPEED_COAST;

    switch (classIndex) {
        case 0: // forward
            setMotorSmooth(base, base, CNN_MOTOR_RAMP_STEP);
            break;
        case 1: // left
            setMotorSmooth(CNN_SPEED_TURN_LEFT_INNER, CNN_SPEED_TURN_LEFT_OUTER, CNN_MOTOR_RAMP_STEP);
            break;
        case 2: // right
            setMotorSmooth(CNN_SPEED_TURN_RIGHT_OUTER, CNN_SPEED_TURN_RIGHT_INNER, CNN_MOTOR_RAMP_STEP);
            break;
        case 3: // nothing
            setMotorSmooth(coast, coast, CNN_MOTOR_RAMP_STEP);
            break;
        default:
            setMotor(0, 0);
            break;
    }
}

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

    switch (classIndex) {
        case 0: // forward
            setMotor(base, base);
            break;
        case 1: // left
            setMotor(CNN_SPEED_TURN_LEFT_INNER, CNN_SPEED_TURN_LEFT_OUTER);
            break;
        case 2: // right
            setMotor(CNN_SPEED_TURN_RIGHT_OUTER, CNN_SPEED_TURN_RIGHT_INNER);
            break;
        case 3: // nothing
            setMotor(0, 0);
            break;
        default:
            setMotor(0, 0);
            break;
    }
}

// file: firmware/src/controller.cpp
// purpose: map CNN class index to motor actions
// dependencies: controller.h, motor.h

#include <Arduino.h>
#include "controller.h"
#include "motor.h"

void classToAction(int classIndex) {
    static bool trimInitialized = false;
    if (!trimInitialized) {
        setMotorTrim(CNN_LEFT_TRIM, CNN_RIGHT_TRIM);
        trimInitialized = true;
    }

    switch (classIndex) {
        case 0: // forward
            setMotorSmooth(CNN_SPEED_FORWARD, CNN_SPEED_FORWARD, CNN_MOTOR_RAMP_STEP);
            break;
        case 1: // left
            setMotorSmooth(CNN_SPEED_TURN_INNER, CNN_SPEED_TURN_OUTER, CNN_MOTOR_RAMP_STEP);
            break;
        case 2: // right
            setMotorSmooth(CNN_SPEED_TURN_OUTER, CNN_SPEED_TURN_INNER, CNN_MOTOR_RAMP_STEP);
            break;
        case 3: // nothing
            setMotorSmooth(CNN_SPEED_COAST, CNN_SPEED_COAST, CNN_MOTOR_RAMP_STEP);
            break;
        default:
            setMotor(0, 0);
            break;
    }
}

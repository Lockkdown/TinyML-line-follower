// file: firmware/src/controller.cpp
// purpose: map CNN class index to motor actions
// dependencies: controller.h, motor.h

#include <Arduino.h>
#include "controller.h"
#include "motor.h"

void classToAction(int classIndex) {
    switch (classIndex) {
        case 0: // forward
            setMotor(CNN_SPEED, CNN_SPEED);
            break;
        case 1: // left
            setMotor(CNN_SPEED / 2, CNN_SPEED);
            break;
        case 2: // right
            setMotor(CNN_SPEED, CNN_SPEED / 2);
            break;
        case 3: // nothing
            setMotor(0, 0);
            break;
        default:
            setMotor(0, 0);
            break;
    }
}

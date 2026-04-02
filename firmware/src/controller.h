// file: firmware/src/controller.h
// purpose: declarations for CNN class to motor action mapping
// dependencies: motor.h

#pragma once

#include <stdint.h>

constexpr int CNN_SPEED = 160;

void classToAction(int classIndex);

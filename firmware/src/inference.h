// file: firmware/src/inference.h
// purpose: declarations for TFLite Micro inference runner
// dependencies: model_data.cc

#pragma once

#include <stdint.h>

bool initInference();
int runInference();
int getLastClass();
float getLastConfidence();

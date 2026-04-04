// file: firmware/src/inference.h
// purpose: declarations for TFLite Micro inference runner
// dependencies: model_data.cc

#pragma once

#include <stdint.h>

struct InferenceTimings {
    uint64_t cam_us;   // captureFrame() duration
    uint64_t prep_us;  // pixel copy into input tensor duration
    uint64_t inf_us;   // interpreter->Invoke() duration
};

bool initInference();
int runInference();
int getLastClass();
float getLastConfidence();
const InferenceTimings& getLastInferenceTimings();

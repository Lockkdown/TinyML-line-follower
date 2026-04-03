// file: firmware/src/inference.cpp
// purpose: TFLite Micro inference runner with INT8 model
// dependencies: inference.h, model_data.cc, esp_camera.h

#include <Arduino.h>
#include <esp_camera.h>
#include <img_converters.h>
#include "camera.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"

const unsigned char* getModelData();
unsigned int getModelDataLen();

// Tensor arena sizes to try in order
static constexpr int ARENA_SIZES[] = {60 * 1024, 90 * 1024, 120 * 1024, 150 * 1024};
static constexpr int NUM_ARENA_SIZES = sizeof(ARENA_SIZES) / sizeof(ARENA_SIZES[0]);

// Global objects
static uint8_t* tensor_arena = nullptr;
static int g_tensorArenaSize = 0;
static tflite::MicroInterpreter* interpreter = nullptr;
static TfLiteTensor* input_tensor = nullptr;
static TfLiteTensor* output_tensor = nullptr;

// Last inference result
static int lastClass = -1;
static float lastConfidence = 0.0f;
static uint8_t* jpegRgbBuffer = nullptr;
static size_t jpegRgbBufferSize = 0;

static bool hasModelData() {
    return getModelData() != nullptr && getModelDataLen() > 0;
}

static bool ensureJpegRgbBuffer(size_t requiredSize) {
    if (jpegRgbBuffer != nullptr && jpegRgbBufferSize >= requiredSize) {
        return true;
    }

    if (jpegRgbBuffer != nullptr) {
        heap_caps_free(jpegRgbBuffer);
        jpegRgbBuffer = nullptr;
        jpegRgbBufferSize = 0;
    }

    jpegRgbBuffer = (uint8_t*)heap_caps_malloc(requiredSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!jpegRgbBuffer) {
        jpegRgbBuffer = (uint8_t*)heap_caps_malloc(requiredSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!jpegRgbBuffer) {
        return false;
    }

    jpegRgbBufferSize = requiredSize;
    return true;
}

bool initInference() {
    Serial.println("[INFERENCE] initInference() start");
    if (!hasModelData()) {
        Serial.println("[INFERENCE] FAILED: model data not available");
        return false;
    }
    Serial.println("[INFERENCE] Model data OK");

    // Map model
    Serial.println("[INFERENCE] Mapping model...");
    const unsigned char* modelData = getModelData();
    Serial.printf("[INFERENCE] Model data pointer: %p\n", modelData);
    
    // Validate model data pointer
    if (!modelData || ((uintptr_t)modelData & 0x3) != 0) {
        Serial.println("[INFERENCE] FAILED: model data pointer is null or not 4-byte aligned");
        return false;
    }
    
    // Check model data length
    unsigned int modelLen = getModelDataLen();
    Serial.printf("[INFERENCE] Model data length: %u bytes\n", modelLen);
    if (modelLen < 4) {
        Serial.println("[INFERENCE] FAILED: model data too small");
        return false;
    }
    
    // Check flatbuffer magic number (starts at byte 4)
    char magic[5] = {0};
    for (int i = 0; i < 4; i++) magic[i] = (char)modelData[4 + i];
    Serial.printf("[INFERENCE] Flatbuffer magic: %s\n", magic);
    if (strcmp(magic, "TFL3") != 0) {
        Serial.println("[INFERENCE] FAILED: invalid flatbuffer magic number (expected TFL3)");
        return false;
    }
    
    const tflite::Model* model = tflite::GetModel(modelData);
    if (!model) {
        Serial.println("[INFERENCE] FAILED: GetModel returned null");
        return false;
    }
    Serial.printf("[INFERENCE] Model mapped, version %d\n", model->version());
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("[INFERENCE] FAILED: model schema version %d != %d\n",
                     model->version(), TFLITE_SCHEMA_VERSION);
        return false;
    }

    // Create op resolver
    Serial.println("[INFERENCE] Creating op resolver...");
    static tflite::AllOpsResolver resolver;
    Serial.println("[INFERENCE] Op resolver OK");

    // Try each arena size
    Serial.println("[INFERENCE] Creating interpreter...");
    static tflite::MicroErrorReporter micro_error_reporter;
    tflite::ErrorReporter* error_reporter = &micro_error_reporter;

    for (int i = 0; i < NUM_ARENA_SIZES; ++i) {
        Serial.printf("[INFERENCE] Trying arena size %d KB...\n", ARENA_SIZES[i] / 1024);

        if (tensor_arena != nullptr) {
            heap_caps_free(tensor_arena);
            tensor_arena = nullptr;
        }
        if (interpreter != nullptr) {
            delete interpreter;
            interpreter = nullptr;
        }

        tensor_arena = (uint8_t*)heap_caps_malloc(ARENA_SIZES[i], MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!tensor_arena) {
            tensor_arena = (uint8_t*)heap_caps_malloc(ARENA_SIZES[i], MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        }
        if (!tensor_arena) {
            Serial.printf("[INFERENCE] arena %d KB allocation failed\n", ARENA_SIZES[i] / 1024);
            continue;
        }
        memset(tensor_arena, 0, ARENA_SIZES[i]);

        interpreter = new tflite::MicroInterpreter(
            model, resolver, tensor_arena, ARENA_SIZES[i], error_reporter);
            
        Serial.println("[INFERENCE] Interpreter created, allocating tensors...");

        TfLiteStatus allocate_status = interpreter->AllocateTensors();
        if (allocate_status != kTfLiteOk) {
            Serial.printf("[INFERENCE] arena %d KB failed allocation\n", ARENA_SIZES[i] / 1024);
            continue;
        }
        Serial.printf("[INFERENCE] Tensors allocated with arena %d KB\n", ARENA_SIZES[i] / 1024);

        // Verify input shape and type
        Serial.println("[INFERENCE] Checking input tensor...");
        input_tensor = interpreter->input(0);
        if (!input_tensor) {
            Serial.println("[INFERENCE] FAILED: input tensor is null");
            continue;
        }
        Serial.printf("[INFERENCE] Input dims: %d, type: %d\n", input_tensor->dims->size, input_tensor->type);
        if (input_tensor->dims->size != 4 ||
            input_tensor->dims->data[0] != 1 ||
            input_tensor->dims->data[1] != 96 ||
            input_tensor->dims->data[2] != 96 ||
            input_tensor->dims->data[3] != 1 ||
            input_tensor->type != kTfLiteInt8) {
            Serial.println("[INFERENCE] FAILED: input must be (1,96,96,1) int8");
            continue;
        }

        // Verify output shape
        Serial.println("[INFERENCE] Checking output tensor...");
        output_tensor = interpreter->output(0);
        if (!output_tensor) {
            Serial.println("[INFERENCE] FAILED: output tensor is null");
            continue;
        }
        Serial.printf("[INFERENCE] Output dims: %d, type: %d\n", output_tensor->dims->size, output_tensor->type);
        if (output_tensor->dims->size != 2 ||
            output_tensor->dims->data[0] != 1 ||
            output_tensor->dims->data[1] != 4 ||
            output_tensor->type != kTfLiteInt8) {
            Serial.println("[INFERENCE] FAILED: output must be (1,4) int8");
            continue;
        }

        Serial.printf("[INFERENCE] OK with arena %d KB\n", ARENA_SIZES[i] / 1024);
        g_tensorArenaSize = ARENA_SIZES[i];
        Serial.printf("[INFERENCE] Arena final size: %d bytes\n", g_tensorArenaSize);
        Serial.println("[INFERENCE] initInference() SUCCESS");
        return true;
    }

    Serial.println("[INFERENCE] FAILED: all arena sizes failed");
    heap_caps_free(tensor_arena);
    tensor_arena = nullptr;
    g_tensorArenaSize = 0;
    interpreter = nullptr;
    return false;
}

int runInference() {
    if (!interpreter || !input_tensor || !output_tensor) {
        Serial.println("[INFERENCE] not initialized");
        return -1;
    }

    // Get frame from camera
    camera_fb_t* fb = nullptr;
    if (!captureFrame(&fb) || !fb || fb->len == 0) {
        Serial.println("[INFERENCE] failed to get frame");
        if (fb) esp_camera_fb_return(fb);
        return -1;
    }

    // Check if frame is grayscale and 96x96
    // If not grayscale, we need to convert (simple luminance)
    // If not 96x96, we need to downsample (nearest neighbor)
    bool isFastPathGrayscale96 = (fb->width == 96 && fb->height == 96 && fb->format == PIXFORMAT_GRAYSCALE);
    bool needResize = (fb->width != 96 || fb->height != 96);
    bool needGrayscale = (fb->format != PIXFORMAT_GRAYSCALE);

    // Temporary buffer for processed image
    static uint8_t processed[96 * 96];
    uint8_t* src = (uint8_t*)fb->buf;
    uint8_t* jpegRgb = nullptr;

    if (isFastPathGrayscale96) {
        src = (uint8_t*)fb->buf;
    } else if (needResize || needGrayscale) {
        if (fb->format == PIXFORMAT_JPEG) {
            size_t requiredSize = fb->width * fb->height * 3;
            if (!ensureJpegRgbBuffer(requiredSize)) {
                Serial.println("[INFERENCE] failed to allocate JPEG decode buffer");
                esp_camera_fb_return(fb);
                return -1;
            }
            if (!fmt2rgb888(fb->buf, fb->len, fb->format, jpegRgbBuffer)) {
                Serial.println("[INFERENCE] JPEG decode failed");
                esp_camera_fb_return(fb);
                return -1;
            }
            jpegRgb = jpegRgbBuffer;
        }

        // Simple nearest neighbor downsampling + grayscale conversion
        for (int y = 0; y < 96; ++y) {
            for (int x = 0; x < 96; ++x) {
                // Map to source coordinates
                int srcX = (x * fb->width) / 96;
                int srcY = (y * fb->height) / 96;
                int srcIdx = srcY * fb->width + srcX;

                uint8_t gray;
                if (needGrayscale && fb->format == PIXFORMAT_RGB565) {
                    // Convert RGB565 to grayscale
                    uint16_t pixel = ((uint16_t*)src)[srcIdx];
                    uint8_t r5 = (pixel >> 11) & 0x1F;
                    uint8_t g6 = (pixel >> 5) & 0x3F;
                    uint8_t b5 = pixel & 0x1F;
                    uint8_t r8 = (r5 * 255) / 31;
                    uint8_t g8 = (g6 * 255) / 63;
                    uint8_t b8 = (b5 * 255) / 31;
                    gray = (uint8_t)(0.299f * r8 + 0.587f * g8 + 0.114f * b8);
                } else if (needGrayscale && fb->format == PIXFORMAT_YUV422) {
                    // YUV422: Y component is first byte of each pair
                    gray = src[srcIdx * 2];
                } else if (needGrayscale && fb->format == PIXFORMAT_JPEG) {
                    int rgbIdx = srcIdx * 3;
                    uint8_t r8 = jpegRgb[rgbIdx];
                    uint8_t g8 = jpegRgb[rgbIdx + 1];
                    uint8_t b8 = jpegRgb[rgbIdx + 2];
                    gray = (uint8_t)(0.299f * r8 + 0.587f * g8 + 0.114f * b8);
                } else {
                    // Already grayscale
                    gray = src[srcIdx];
                }
                processed[y * 96 + x] = gray;
            }
        }
        src = processed;
    }

    // Copy and quantize to input tensor (uint8 [0,255] -> int8 [-128,127])
    int8_t* input = input_tensor->data.int8;
    for (int i = 0; i < 96 * 96; ++i) {
        input[i] = (int8_t)(src[i] - 128);
    }

    esp_camera_fb_return(fb);

    // Run inference
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        Serial.println("[INFERENCE] invoke failed");
        return -1;
    }

    // Get output (int8), find argmax, dequantize confidence
    int8_t* output = output_tensor->data.int8;
    float scale = output_tensor->params.scale;
    int zero_point = output_tensor->params.zero_point;

    int maxIdx = 0;
    float maxVal = (output[0] - zero_point) * scale;
    for (int i = 1; i < 4; ++i) {
        float val = (output[i] - zero_point) * scale;
        if (val > maxVal) {
            maxVal = val;
            maxIdx = i;
        }
    }

    lastClass = maxIdx;
    lastConfidence = maxVal;

    return maxIdx;
}

int getLastClass() {
    return lastClass;
}

float getLastConfidence() {
    return lastConfidence;
}

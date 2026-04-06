// file: firmware/src/inference.cpp
// purpose: TFLite Micro inference runner with INT8 model
// dependencies: inference.h, model_data.cc, esp_camera.h

#include <Arduino.h>
#include <esp_camera.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <img_converters.h>
#include "camera.h"
#include "controller.h"
#include "inference.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#ifndef TENSOR_ARENA_BYTES
#define TENSOR_ARENA_BYTES (2 * 1024 * 1024)
#endif
#ifndef CNN_VERBOSE_INFERENCE_LOG
#define CNN_VERBOSE_INFERENCE_LOG 0
#endif

#ifdef CONFIG_ESP_NN_ANSI_C
#warning "[INFERENCE] CONFIG_ESP_NN_ANSI_C is defined — esp-nn SIMD disabled, ~88ms expected. Add -UCONFIG_ESP_NN_ANSI_C to build_flags."
#endif

const unsigned char* getModelData();
unsigned int getModelDataLen();

// Size from platformio.ini (-DTENSOR_ARENA_BYTES). initInference() runs after CNN camera init
// (smaller DMA alloc first) so heap is not fragmented by arena before esp_camera_init.
static constexpr int TENSOR_ARENA_SIZE = TENSOR_ARENA_BYTES;
static constexpr size_t TENSOR_ARENA_ALIGN = 16;

static uint8_t* tensor_arena = nullptr;
static uint8_t* tensor_arena_alloc = nullptr;

static bool ensureTensorArena() {
    if (tensor_arena != nullptr) {
        memset(tensor_arena, 0, TENSOR_ARENA_SIZE);
        return true;
    }
    const size_t total = (size_t)TENSOR_ARENA_SIZE + TENSOR_ARENA_ALIGN;
    Serial.printf(
        "[INFERENCE] Free internal heap: %u bytes (PSRAM largest: %u) need arena alloc %u\n",
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
        (unsigned)total);
    // Prefer INTERNAL: esp-nn + TFLite hot path stays in fast SRAM (~23ms class). PSRAM arena was ~75ms INF.
    // dsep_cnn with first-sep stride 2 keeps arena_used ~under 110KB; if internal alloc fails, fall back to PSRAM.
    uint8_t* raw =
        (uint8_t*)heap_caps_malloc(total, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    bool in_psram = false;
    if (raw == nullptr) {
        raw = (uint8_t*)heap_caps_malloc(total, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        in_psram = (raw != nullptr);
    }
    if (raw == nullptr) {
        return false;
    }
    tensor_arena_alloc = raw;
    const uintptr_t aligned =
        ((uintptr_t)raw + TENSOR_ARENA_ALIGN - 1) & ~((uintptr_t)TENSOR_ARENA_ALIGN - 1);
    tensor_arena = (uint8_t*)aligned;
    memset(tensor_arena, 0, TENSOR_ARENA_SIZE);
    Serial.printf("[INFERENCE] Tensor arena: %s\n",
        in_psram ? "PSRAM (fallback, slower INF)" : "INTERNAL (preferred)");
    return true;
}

static tflite::MicroInterpreter* interpreter = nullptr;
static TfLiteTensor* input_tensor = nullptr;
static TfLiteTensor* output_tensor = nullptr;

// Last inference result
static int lastClass = -1;
static float lastConfidence = 0.0f;
static InferenceTimings lastTimings = {};
static uint8_t* jpegRgbBuffer = nullptr;
static size_t jpegRgbBufferSize = 0;

CnnTimingStats g_cnn_stats = {};

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

static bool validateModelData(const unsigned char* modelData) {
    if (!modelData || ((uintptr_t)modelData & 0x3) != 0) {
        Serial.println("[INFERENCE] FAILED: model data null or misaligned");
        return false;
    }
    if (getModelDataLen() < 4) {
        Serial.println("[INFERENCE] FAILED: model data too small");
        return false;
    }
    char magic[5] = {0};
    for (int i = 0; i < 4; i++) magic[i] = (char)modelData[4 + i];
    if (strcmp(magic, "TFL3") != 0) {
        Serial.printf("[INFERENCE] FAILED: bad magic '%s' (expected TFL3)\n", magic);
        return false;
    }
    return true;
}

static bool validateTensors() {
    input_tensor = interpreter->input(0);
    if (!input_tensor ||
        input_tensor->dims->size != 4 ||
        input_tensor->dims->data[1] != 96 ||
        input_tensor->dims->data[2] != 96 ||
        input_tensor->dims->data[3] != 1 ||
        input_tensor->type != kTfLiteInt8) {
        Serial.println("[INFERENCE] FAILED: input must be (1,96,96,1) int8");
        return false;
    }
    output_tensor = interpreter->output(0);
    if (!output_tensor ||
        output_tensor->dims->size != 2 ||
        output_tensor->dims->data[1] != 4 ||
        output_tensor->type != kTfLiteInt8) {
        Serial.println("[INFERENCE] FAILED: output must be (1,4) int8");
        return false;
    }
    Serial.printf("[INFERENCE] Input quant: scale=%.6f zero_point=%d\n",
        input_tensor->params.scale, input_tensor->params.zero_point);
    Serial.printf("[INFERENCE] Arena used: %u bytes\n",
        (unsigned)interpreter->arena_used_bytes());
    return true;
}

static TfLiteStatus registerLineFollowerOps(tflite::MicroMutableOpResolver<32>& resolver) {
    if (resolver.AddConv2D() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddDepthwiseConv2D() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddMaxPool2D() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddAveragePool2D() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddFullyConnected() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddAdd() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddMean() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddSoftmax() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddReshape() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddShape() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddStridedSlice() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddPack() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddUnpack() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddCast() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddSqueeze() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddRelu() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddRelu6() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddQuantize() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddDequantize() != kTfLiteOk) return kTfLiteError;
    if (resolver.AddPad() != kTfLiteOk) return kTfLiteError;
    return kTfLiteOk;
}

bool initInference() {
    Serial.println("[INFERENCE] initInference() start");
    if (!hasModelData()) {
        Serial.println("[INFERENCE] FAILED: no model data");
        return false;
    }
    if (!ensureTensorArena()) {
        Serial.println("[INFERENCE] FAILED: tensor arena allocation");
        return false;
    }
    const unsigned char* modelData = getModelData();
    if (!validateModelData(modelData)) return false;
    Serial.printf(
        "[INFERENCE] MODEL_DATA_LEN=%u (ref: dsep~20568, lenet~50680)\n",
        getModelDataLen());
    if (getModelDataLen() > 80000 && TENSOR_ARENA_SIZE < (1200 * 1024)) {
        Serial.println("[INFERENCE] FAILED: embedded model > 80 KiB but arena < 1.2 MiB.");
        Serial.println("  dsep_cnn is 26 KiB — re-run tools/convert_to_cc.sh to embed the correct model.");
        Serial.println("  For large CNNs: increase TENSOR_ARENA_BYTES in platformio.ini.");
        return false;
    }

    const tflite::Model* model = tflite::GetModel(modelData);
    if (!model || model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("[INFERENCE] FAILED: model version mismatch (%d vs %d)\n",
            model ? model->version() : -1, TFLITE_SCHEMA_VERSION);
        return false;
    }

    static tflite::MicroMutableOpResolver<32> resolver;
    static bool resolver_ready = false;

    if (!resolver_ready) {
        if (registerLineFollowerOps(resolver) != kTfLiteOk) {
            Serial.println("[INFERENCE] FAILED: op resolver registration");
            return false;
        }
        resolver_ready = true;
    }

    if (interpreter != nullptr) {
        delete interpreter;
        interpreter = nullptr;
    }

    interpreter = new tflite::MicroInterpreter(
        model, resolver, tensor_arena, TENSOR_ARENA_SIZE);

    if (interpreter->AllocateTensors() != kTfLiteOk) {
        Serial.println("[INFERENCE] FATAL: AllocateTensors failed (increase TENSOR_ARENA_BYTES or use smaller model)");
        delete interpreter;
        interpreter = nullptr;
        return false;
    }

    if (!validateTensors()) {
        delete interpreter;
        interpreter = nullptr;
        return false;
    }

    Serial.println("[INFERENCE] initInference() SUCCESS");
    return true;
}

static void giveCameraLockIfHeld(bool held) {
    if (held && g_camera_mutex != nullptr) {
        xSemaphoreGive(g_camera_mutex);
    }
}

int runInference() {
    if (!interpreter || !input_tensor || !output_tensor) {
        Serial.println("[INFERENCE] not initialized");
        return -1;
    }

    bool cam_lock_held = false;
    if (g_camera_mutex != nullptr) {
        if (xSemaphoreTake(g_camera_mutex, pdMS_TO_TICKS(30)) != pdTRUE) {
            return -1;
        }
        cam_lock_held = true;
    }

    const uint64_t t1 = esp_timer_get_time();

    camera_fb_t* fb = nullptr;
    if (!captureFrame(&fb) || !fb || fb->len == 0) {
        Serial.println("[INFERENCE] failed to get frame");
        if (fb) {
            esp_camera_fb_return(fb);
        }
        giveCameraLockIfHeld(cam_lock_held);
        return -1;
    }
#if CNN_VERBOSE_INFERENCE_LOG
    Serial.printf("[CNN] fb size=%d\n", fb->len);
#endif

    const uint64_t t2 = esp_timer_get_time();

    bool isFastPathGrayscale96 = (fb->width == 96 && fb->height == 96 && fb->format == PIXFORMAT_GRAYSCALE);
    bool needResize = (fb->width != 96 || fb->height != 96);
    bool needGrayscale = (fb->format != PIXFORMAT_GRAYSCALE);

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
                giveCameraLockIfHeld(cam_lock_held);
                return -1;
            }
            if (!fmt2rgb888(fb->buf, fb->len, fb->format, jpegRgbBuffer)) {
                Serial.println("[INFERENCE] JPEG decode failed");
                esp_camera_fb_return(fb);
                giveCameraLockIfHeld(cam_lock_held);
                return -1;
            }
            jpegRgb = jpegRgbBuffer;
        }

        for (int y = 0; y < 96; ++y) {
            for (int x = 0; x < 96; ++x) {
                int srcX = (x * fb->width) / 96;
                int srcY = (y * fb->height) / 96;
                int srcIdx = srcY * fb->width + srcX;

                uint8_t gray;
                if (needGrayscale && fb->format == PIXFORMAT_RGB565) {
                    uint16_t pixel = ((uint16_t*)src)[srcIdx];
                    uint8_t r5 = (pixel >> 11) & 0x1F;
                    uint8_t g6 = (pixel >> 5) & 0x3F;
                    uint8_t b5 = pixel & 0x1F;
                    uint8_t r8 = (r5 * 255) / 31;
                    uint8_t g8 = (g6 * 255) / 63;
                    uint8_t b8 = (b5 * 255) / 31;
                    gray = (uint8_t)(0.299f * r8 + 0.587f * g8 + 0.114f * b8);
                } else if (needGrayscale && fb->format == PIXFORMAT_YUV422) {
                    gray = src[srcIdx * 2];
                } else if (needGrayscale && fb->format == PIXFORMAT_JPEG) {
                    int rgbIdx = srcIdx * 3;
                    uint8_t r8 = jpegRgb[rgbIdx];
                    uint8_t g8 = jpegRgb[rgbIdx + 1];
                    uint8_t b8 = jpegRgb[rgbIdx + 2];
                    gray = (uint8_t)(0.299f * r8 + 0.587f * g8 + 0.114f * b8);
                } else {
                    gray = src[srcIdx];
                }
                processed[y * 96 + x] = gray;
            }
        }
        src = processed;
    }

    int8_t* input = input_tensor->data.int8;
    for (int i = 0; i < 96 * 96; ++i) {
        input[i] = (int8_t)(src[i] - 128);
    }

    esp_camera_fb_return(fb);
    giveCameraLockIfHeld(cam_lock_held);
    cam_lock_held = false;

    const uint64_t t3 = esp_timer_get_time();

    // Run inference
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        Serial.println("[INFERENCE] invoke failed");
        return -1;
    }

    const uint64_t t4 = esp_timer_get_time();

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

    if constexpr (CNN_LOG_INFERENCE_EVERY_FRAME) {
        Serial.printf("[CNN] class=%d conf=%.2f\n", maxIdx, maxVal);
    }

    lastClass = maxIdx;
    lastConfidence = maxVal;
    lastTimings.cam_us  = t2 - t1;
    lastTimings.prep_us = t3 - t2;
    lastTimings.inf_us  = t4 - t3;

    const uint64_t total = t4 - t1;
    g_cnn_stats.cam_us     = (uint32_t)(t2 - t1);
    g_cnn_stats.prep_us    = (uint32_t)(t3 - t2);
    g_cnn_stats.inf_us     = (uint32_t)(t4 - t3);
    g_cnn_stats.total_us   = (uint32_t)total;
    g_cnn_stats.fps        = (total > 0) ? (1e6f / (float)total) : 0.0f;
    g_cnn_stats.last_class = maxIdx;
    g_cnn_stats.last_conf  = maxVal;

    return maxIdx;
}

int getLastClass() {
    return lastClass;
}

float getLastConfidence() {
    return lastConfidence;
}

const InferenceTimings& getLastInferenceTimings() {
    return lastTimings;
}

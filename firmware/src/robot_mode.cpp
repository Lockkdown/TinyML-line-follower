#include "robot_mode.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "camera.h"
#include "controller.h"
#include "inference.h"
#include "motor.h"

static RobotMode g_robotMode = MODE_CONTROL;
static TaskHandle_t g_cnnTaskHandle = nullptr;
static volatile bool g_cnnLoopActive = false;
static int g_classHistory[CNN_MOMENTUM_FRAMES] = {3, 3, 3};
static int g_historyIndex = 0;

static void pushHistoryClass(int classIndex) {
    g_classHistory[g_historyIndex] = classIndex;
    g_historyIndex = (g_historyIndex + 1) % CNN_MOMENTUM_FRAMES;
}

// Returns the class with >= 2 votes out of 3; falls back to `fallback` if no majority.
static int getMajorityClass(int fallback) {
    int counts[4] = {0, 0, 0, 0};
    for (int i = 0; i < CNN_MOMENTUM_FRAMES; ++i) {
        counts[g_classHistory[i]]++;
    }
    const int requiredVotes = (CNN_MOMENTUM_FRAMES / 2) + 1;
    for (int c = 0; c < 4; ++c) {
        if (counts[c] >= requiredVotes) return c;
    }
    return fallback;
}

static void stopCnnLoop() {
    g_cnnLoopActive = false;
}

static void applyAction(int actionClass) {
    classToAction(actionClass);
}

static void cnnLoopTask(void* param) {
    int lastActionClass = 3;

    while (g_cnnLoopActive) {
        const uint64_t t_start = esp_timer_get_time();

        int cls = runInference();
        const InferenceTimings& timings = getLastInferenceTimings();
        float confidence = getLastConfidence();

        if (cls < 0 || cls > 3) cls = 3;

        // Low-confidence: hold last command rather than trusting raw prediction
        int candidate = (confidence >= CNN_CONFIDENCE_THRESHOLD) ? cls : lastActionClass;
        pushHistoryClass(candidate);

        int actionClass = getMajorityClass(lastActionClass);
        lastActionClass = actionClass;

        const uint64_t t5_start = esp_timer_get_time();
        applyAction(actionClass);
        const uint64_t act_us = esp_timer_get_time() - t5_start;
        const uint64_t total_us = esp_timer_get_time() - t_start;

        Serial.printf("[CNN] class=%d conf=%.2f | CAM:%llu PREP:%llu INF:%llu ACT:%llu TOTAL:%llu us\n",
            cls, confidence, timings.cam_us, timings.prep_us, timings.inf_us, act_us, total_us);

        taskYIELD();
    }

    g_cnnTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

static void startCnnLoop() {
    if (g_cnnTaskHandle != nullptr) {
        return;
    }

    g_cnnLoopActive = true;
    g_historyIndex = 0;
    for (int i = 0; i < CNN_MOMENTUM_FRAMES; ++i) {
        g_classHistory[i] = 3;
    }
    constexpr int kCnnTaskPriority = 6;
    xTaskCreatePinnedToCore(
        cnnLoopTask, "CNN Loop", 16384, nullptr, kCnnTaskPriority, &g_cnnTaskHandle, 1);
}

void setRobotMode(RobotMode mode) {
    if (g_robotMode == mode) {
        return;
    }

    if (mode != MODE_CNN) {
        stopCnnLoop();
    }

    if (mode == MODE_CONTROL) {
        setMotor(0, 0);
        deinitCamera();
        g_robotMode = MODE_CONTROL;
        return;
    }

    if (mode == MODE_DATASET) {
        if (!initCamera()) {
            Serial.println("[Mode] Failed to init camera for DATASET mode");
            return;
        }
        g_robotMode = MODE_DATASET;
        return;
    }

    if (mode == MODE_CNN) {
        deinitCamera();
        if (!initCameraForInference()) {
            Serial.println("[Mode] Failed to init camera for CNN mode");
            return;
        }

        initMotor();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        startCnnLoop();
        g_robotMode = MODE_CNN;
    }
}

RobotMode getRobotMode() {
    return g_robotMode;
}

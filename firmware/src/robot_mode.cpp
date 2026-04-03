#include "robot_mode.h"

#include <Arduino.h>
#include <WiFi.h>
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
static bool g_coastPendingStop = false;

static void pushHistoryClass(int classIndex) {
    g_classHistory[g_historyIndex] = classIndex;
    g_historyIndex = (g_historyIndex + 1) % CNN_MOMENTUM_FRAMES;
}

static int getMomentumTurnClass() {
    int leftCount = 0;
    int rightCount = 0;
    for (int i = 0; i < CNN_MOMENTUM_FRAMES; ++i) {
        if (g_classHistory[i] == 1) {
            ++leftCount;
        } else if (g_classHistory[i] == 2) {
            ++rightCount;
        }
    }

    int requiredVotes = (CNN_MOMENTUM_FRAMES / 2) + 1;
    if (leftCount >= requiredVotes) {
        return 1;
    }
    if (rightCount >= requiredVotes) {
        return 2;
    }
    return 3;
}

static void stopCnnLoop() {
    g_cnnLoopActive = false;
}

static void cnnLoopTask(void* param) {
    while (g_cnnLoopActive) {
        const uint32_t start_time = millis();
        int cls = runInference();
        float confidence = getLastConfidence();

        int actionClass = cls;
        if (actionClass < 0 || actionClass > 3) {
            actionClass = 3;
        }

        if (confidence < CNN_CONFIDENCE_THRESHOLD) {
            if (actionClass == 3) {
                actionClass = getMomentumTurnClass();
            } else {
                actionClass = g_classHistory[(g_historyIndex + CNN_MOMENTUM_FRAMES - 1) % CNN_MOMENTUM_FRAMES];
            }
        }

        pushHistoryClass(actionClass);

        if (actionClass == 3) {
            if (g_coastPendingStop) {
                setMotor(0, 0);
                g_coastPendingStop = false;
            } else {
                classToAction(3);
                g_coastPendingStop = true;
            }
        } else {
            g_coastPendingStop = false;
            classToAction(actionClass);
        }
        const uint32_t end_time = millis();
        Serial.printf("[CNN] Time: %lu ms | Class: %d | Action: %d | Conf: %.3f\n",
            end_time - start_time, cls, actionClass, confidence);
        vTaskDelay(pdMS_TO_TICKS(10));
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
    g_coastPendingStop = false;
    xTaskCreatePinnedToCore(cnnLoopTask, "CNN Loop", 16384, nullptr, 1, &g_cnnTaskHandle, 1);
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

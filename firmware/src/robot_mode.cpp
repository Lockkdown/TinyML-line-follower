// file: firmware/src/robot_mode.cpp
// purpose: manage robot operating modes and CNN inference loop
// dependencies: camera.h, controller.h, inference.h, motor.h, wifi_controller.h

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
#include "wifi_controller.h"

static RobotMode g_robotMode = MODE_CONTROL;
static TaskHandle_t g_cnnTaskHandle = nullptr;
static volatile bool g_cnnLoopActive = false;

static void stopCnnLoop() {
    g_cnnLoopActive = false;
}

static void applyHysteresis(int cls, float conf, int* prev_class, int* hold_count) {
    if (cls == *prev_class) {
        *hold_count = 0;
        classToAction(cls, conf);
    } else {
        (*hold_count)++;
        // Log when suppressing a turn command — critical for diagnosing 90° overshoot.
        // forward=0, left=1, right=2; prev_class=0 means was going straight.
        const bool suppressingTurn = (cls == 1 || cls == 2) && (*prev_class == 0);
        if (suppressingTurn) {
            Serial.printf("[HYST] HOLDING turn cls=%d (hold %d/%d) acting on prev=%d\n",
                          cls, *hold_count, HOLD_FRAMES, *prev_class);
        }
        if (*hold_count >= HOLD_FRAMES) {
            *prev_class = cls;
            *hold_count = 0;
        }
        classToAction(*prev_class, conf);
    }
}

static void cnnLoopTask(void* param) {
    int prev_class = 3;
    int hold_count = 0;
    int frame_count = 0;

    while (g_cnnLoopActive) {
        const uint64_t loop_t0 = esp_timer_get_time();
        Serial.println("[CNN] loop tick");
        const int raw_cls = runInference();
        const float conf = getLastConfidence();
        int cls = raw_cls;
        if (cls < 0 || cls > 3) {
            cls = 3;
        }

        applyHysteresis(cls, conf, &prev_class, &hold_count);

        frame_count++;
        digitalWrite(CNN_INDICATOR_LED, frame_count % 2);

        if (CNN_LOG_PREDICTION_EVERY_FRAMES > 0 &&
            (frame_count % CNN_LOG_PREDICTION_EVERY_FRAMES) == 0) {
            const InferenceTimings& tm = getLastInferenceTimings();
            const uint64_t loop_us = esp_timer_get_time() - loop_t0;
            const float loop_fps =
                (loop_us > 0) ? (1e6f / (float)loop_us) : 0.0f;
            Serial.printf(
                "[CNN] class=%d conf=%.2f hold_cls=%d | CAM:%llu PREP:%llu INF:%llu us | "
                "pipe_fps:%.1f loop_us:%llu loop_fps:%.1f\n",
                cls,
                conf,
                prev_class,
                (unsigned long long)tm.cam_us,
                (unsigned long long)tm.prep_us,
                (unsigned long long)tm.inf_us,
                g_cnn_stats.fps,
                (unsigned long long)loop_us,
                loop_fps);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    g_cnnTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

static bool startCnnLoop() {
    if (g_cnnTaskHandle != nullptr) {
        return true;
    }
    pinMode(CNN_INDICATOR_LED, OUTPUT);
    g_cnnLoopActive = true;
    constexpr int kCnnTaskPriority = 6;
    const BaseType_t created = xTaskCreatePinnedToCore(
        cnnLoopTask, "CNN Loop", 16384, nullptr, kCnnTaskPriority, &g_cnnTaskHandle, 1);
    if (created != pdPASS) {
        g_cnnLoopActive = false;
        g_cnnTaskHandle = nullptr;
        Serial.println("[Mode] CNN task create failed");
        return false;
    }
    return true;
}

void setRobotMode(RobotMode mode) {
    if (g_robotMode == mode) {
        return;
    }

    if (mode != MODE_CNN) {
        stopCnnLoop();
    }

    if (mode == MODE_CONTROL) {
        const bool comingFromCnn = (g_robotMode == MODE_CNN);
        setMotor(0, 0);
        if (comingFromCnn) {
            setCameraModeStream();
            reconnectWiFiAfterCnn();
        } else {
            deinitCamera();
        }
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
        if (!initCamera()) {
            Serial.println("[Mode] Failed to init camera for CNN mode");
            return;
        }
        // Tắt WiFi trước khi đổi mode camera để /snapshot không giữ mutex tranh với setCameraModeCNN.
        initMotor();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(150);

        if (!setCameraModeCNN()) {
            Serial.println("[Mode] Failed to set camera to CNN mode — restoring WiFi");
            reconnectWiFiAfterCnn();
            return;
        }
        if (!startCnnLoop()) {
            Serial.println("[Mode] CNN loop start failed — restoring WiFi");
            setCameraModeStream();
            reconnectWiFiAfterCnn();
            return;
        }
        Serial.println("[Mode] CNN running — WiFi off; USB Serial shows [CNN] predictions");
        g_robotMode = MODE_CNN;
    }
}

RobotMode getRobotMode() {
    return g_robotMode;
}

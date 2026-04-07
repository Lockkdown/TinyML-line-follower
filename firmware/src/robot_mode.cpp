// file: firmware/src/robot_mode.cpp
// purpose: manage robot operating modes and CNN inference loop
// dependencies: camera.h, cnn_recovery.h, controller.h, inference.h, motor.h, wifi_controller.h

#include "robot_mode.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "camera.h"
#include "cnn_recovery.h"
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

static void logHystHoldVerbose(int cls, int hold_count, int prev_snapshot) {
    if constexpr (!HYST_VERBOSE_LOG) {
        return;
    }
    const bool suppressingTurn = (cls == 1 || cls == 2) && (prev_snapshot == 0);
    if (!suppressingTurn) {
        return;
    }
    Serial.printf(
        "[HYST] HOLDING turn cls=%d (hold %d/%d) acting on prev=%d\n",
        cls,
        hold_count,
        HOLD_FRAMES,
        prev_snapshot);
}

static void applyTurnHysteresis(int cls, float conf, int* prev_class, int* hold_count) {
    if (cls == *prev_class) {
        *hold_count = 0;
        classToAction(cls, conf);
        return;
    }
    (*hold_count)++;
    logHystHoldVerbose(cls, *hold_count, *prev_class);
    if (*hold_count >= HOLD_FRAMES) {
        *prev_class = cls;
        *hold_count = 0;
    }
    classToAction(*prev_class, conf);
}

static void tickGraceMask(int& cls, int* prev_class, int* grace_period_counter) {
    if (*grace_period_counter <= 0) {
        return;
    }
    (*grace_period_counter)--;
    if (cls == 3) {
        cls = *prev_class;
    }
}

static bool runGraceAndRecovery(int& cls, float conf, int* prev_class, int* grace_period_counter) {
    tickGraceMask(cls, prev_class, grace_period_counter);
    if (cls != 3) {
        const bool start_grace = cnn_recovery_should_grace_before_reset();
        cnn_recovery_reset();
        if (start_grace) {
            *grace_period_counter = RECOVERY_GRACE_FRAMES;
            if constexpr (HYST_VERBOSE_LOG) {
                Serial.printf(
                    "[HYST] Line found! Starting grace period (%d frames).\n",
                    RECOVERY_GRACE_FRAMES);
            }
        }
        return false;
    }
    const CnnRecoveryResult rr = cnn_recovery_process_class3(&cls, *prev_class, conf);
    return rr == CnnRecoveryResult::HandledReturn;
}

static void applyHysteresis(int& cls, float conf, int* prev_class, int* hold_count) {
    static int grace_period_counter = 0;
    if (runGraceAndRecovery(cls, conf, prev_class, &grace_period_counter)) {
        return;
    }
    applyTurnHysteresis(cls, conf, prev_class, hold_count);
}

static void cnnLoopTask(void* param) {
    int prev_class = 3;
    int hold_count = 0;
    int frame_count = 0;

    while (g_cnnLoopActive) {
        const uint64_t loop_t0 = esp_timer_get_time();
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

        vTaskDelay(0);
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
    // 8 KiB stack: loop is thin (runInference on main arena); frees internal heap after ~160 KiB tensor arena.
    const BaseType_t created = xTaskCreatePinnedToCore(
        cnnLoopTask, "CNN Loop", 8192, nullptr, kCnnTaskPriority, &g_cnnTaskHandle, 1);
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
        stopWebServer();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(400);

        if (!deinitCamera()) {
            Serial.println("[Mode] Failed to deinit camera before CNN — restoring WiFi");
            reconnectWiFiAfterCnn();
            return;
        }

        // Let I2C/SCCB and sensor settle after deinit before re-init (avoids SCCB_Write Failed).
        delay(500);

        if (!initCameraForCnnMode()) {
            Serial.println("[Mode] Failed to init camera for CNN mode — restoring WiFi");
            reconnectWiFiAfterCnn();
            if (!initCamera()) {
                Serial.println("[Mode] Camera stream restore failed after CNN init error");
            }
            return;
        }

        // Re-init after camera LEDC/XCLK so ENA/ENB PWM stays attached (init before deinit can be overwritten).
        initMotor();
        setMotor(0, 0);

        if (!initInference()) {
            Serial.println("[Mode] Inference init failed — restoring WiFi");
            deinitCamera();
            reconnectWiFiAfterCnn();
            if (!initCamera()) {
                Serial.println("[Mode] Camera stream restore after inference error");
            }
            return;
        }

        initMotor();
        setMotor(0, 0);

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

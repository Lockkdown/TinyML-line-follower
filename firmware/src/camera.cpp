// file: firmware/src/camera.cpp
// purpose: OV2640 init, stream/CNN mode switching, frame capture
// dependencies: camera.h, esp32-camera, FreeRTOS

#include <Arduino.h>
#include <esp_camera.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "camera.h"

constexpr int CAMERA_WARMUP_FRAMES = 3;
static bool g_cameraInitialized = false;

SemaphoreHandle_t g_camera_mutex = nullptr;

static camera_config_t buildCameraConfig() {
    camera_config_t config;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;

    config.xclk_freq_hz = XCLK_FREQ_HZ;
    config.ledc_timer   = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_4;

    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = STREAM_QUALITY_DEFAULT;
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    config.grab_mode    = CAMERA_GRAB_LATEST;

    return config;
}

static void flushTwoFrames() {
    for (int i = 0; i < 2; ++i) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (fb) {
            esp_camera_fb_return(fb);
        }
    }
}

static bool initCameraWithConfig(const camera_config_t& config, const char* successLabel) {
    if (g_cameraInitialized) {
        return true;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[Camera] Init FAILED: 0x%x\n", err);
        return false;
    }

    for (int i = 0; i < CAMERA_WARMUP_FRAMES; ++i) {
        camera_fb_t* frame_buffer = esp_camera_fb_get();
        if (frame_buffer == nullptr) {
            Serial.println("[Camera] Warmup frame unavailable");
            continue;
        }
        esp_camera_fb_return(frame_buffer);
    }

    setStreamQuality(STREAM_QUALITY_DEFAULT);
    g_cameraInitialized = true;
    if (g_camera_mutex == nullptr) {
        g_camera_mutex = xSemaphoreCreateMutex();
    }
    Serial.println(successLabel);
    return true;
}

bool initCamera() {
    camera_config_t config = buildCameraConfig();
    return initCameraWithConfig(config, "[Camera] Init SUCCESS — QVGA JPEG");
}

bool deinitCamera() {
    if (!g_cameraInitialized) {
        return true;
    }

    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
        Serial.printf("[Camera] Deinit FAILED: 0x%x\n", err);
        return false;
    }

    g_cameraInitialized = false;
    Serial.println("[Camera] Deinit SUCCESS");
    return true;
}

bool captureFrame(camera_fb_t** frame_buffer) {
    if (!g_cameraInitialized || frame_buffer == nullptr) {
        return false;
    }

    *frame_buffer = esp_camera_fb_get();
    return *frame_buffer != nullptr;
}

void setStreamQuality(int quality) {
    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor) {
        sensor->set_quality(sensor, quality);
    }
}

static camera_config_t buildCnnCameraConfig() {
    camera_config_t config = buildCameraConfig();
    config.pixel_format = PIXFORMAT_GRAYSCALE;
    config.frame_size   = FRAMESIZE_96X96;
    return config;
}

static bool reinitCameraWithConfig(const camera_config_t& config, const char* label) {
    if (g_cameraInitialized) {
        esp_camera_deinit();
        g_cameraInitialized = false;
    }
    return initCameraWithConfig(config, label);
}

bool setCameraModeCNN() {
    if (!g_cameraInitialized) {
        return false;
    }
    const bool have_lock =
        (g_camera_mutex != nullptr) &&
        (xSemaphoreTake(g_camera_mutex, pdMS_TO_TICKS(2000)) == pdTRUE);
    if (g_camera_mutex != nullptr && !have_lock) {
        return false;
    }
    const bool ok = reinitCameraWithConfig(
        buildCnnCameraConfig(), "[Camera] CNN mode: 96x96 GRAYSCALE");
    if (have_lock) {
        xSemaphoreGive(g_camera_mutex);
    }
    return ok;
}

bool setCameraModeStream() {
    if (!g_cameraInitialized) {
        return false;
    }
    const bool have_lock =
        (g_camera_mutex != nullptr) &&
        (xSemaphoreTake(g_camera_mutex, pdMS_TO_TICKS(500)) == pdTRUE);
    if (g_camera_mutex != nullptr && !have_lock) {
        return false;
    }
    const bool ok = reinitCameraWithConfig(
        buildCameraConfig(), "[Camera] Stream mode: QVGA JPEG");
    if (have_lock) {
        xSemaphoreGive(g_camera_mutex);
    }
    return ok;
}

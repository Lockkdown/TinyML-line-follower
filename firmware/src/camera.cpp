// file: firmware/src/camera.cpp
// purpose: initialise OV2640 camera for JPEG snapshot streaming
// dependencies: camera.h, esp32-camera

#include <Arduino.h>
#include <esp_camera.h>

#include "camera.h"

constexpr int CAMERA_WARMUP_FRAMES = 3;

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
    config.ledc_timer   = LEDC_TIMER_1;
    // LEDC_CHANNEL_2 avoids conflict with motor PWM channels 2 & 3
    config.ledc_channel = LEDC_CHANNEL_4;

    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size   = FRAMESIZE_QVGA; // 320x240
    config.jpeg_quality = STREAM_QUALITY_DEFAULT;
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    config.grab_mode    = CAMERA_GRAB_LATEST; // Đổi lại thành LATEST để giảm độ trễ, luôn lấy frame mới nhất

    return config;
}

bool initCamera() {
    camera_config_t config = buildCameraConfig();
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
    Serial.println("[Camera] Init SUCCESS — QVGA JPEG");
    return true;
}

void setStreamQuality(int quality) {
    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor) {
        sensor->set_quality(sensor, quality);
    }
}

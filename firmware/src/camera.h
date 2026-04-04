#pragma once

#include <esp_camera.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t g_camera_mutex;

// Camera GPIO constants — GOOUUU ESP32-S3-CAM (matches codebase-context.md §8)
constexpr int PWDN_GPIO_NUM  = -1;
constexpr int RESET_GPIO_NUM = -1;
constexpr int XCLK_GPIO_NUM  = 15;
constexpr int SIOD_GPIO_NUM  = 4;
constexpr int SIOC_GPIO_NUM  = 5;

// Data pins according to GOOUUU ESP32-S3-CAM pinout schematic
constexpr int Y9_GPIO_NUM    = 16; // D7
constexpr int Y8_GPIO_NUM    = 17; // D6
constexpr int Y7_GPIO_NUM    = 18; // D5
constexpr int Y6_GPIO_NUM    = 12; // D4
constexpr int Y5_GPIO_NUM    = 10; // D3
constexpr int Y4_GPIO_NUM    = 8;  // D2 (from pinout: CAM_Y4 is IO8)
constexpr int Y3_GPIO_NUM    = 9;  // D1 (from pinout: CAM_Y3 is IO9)
constexpr int Y2_GPIO_NUM    = 11; // D0 (from pinout: CAM_Y2 is IO11)

constexpr int VSYNC_GPIO_NUM = 6;
constexpr int HREF_GPIO_NUM  = 7;
constexpr int PCLK_GPIO_NUM  = 13;

constexpr int XCLK_FREQ_HZ = 20000000;
constexpr int STREAM_QUALITY_DEFAULT = 12;

bool initCamera();
bool deinitCamera();
bool captureFrame(camera_fb_t** frame_buffer);
void setStreamQuality(int quality);
bool setCameraModeCNN();
bool setCameraModeStream();

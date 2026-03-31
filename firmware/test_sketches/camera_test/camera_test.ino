#include "esp_camera.h"

// Camera pin definitions — GOOUUU ESP32-S3-CAM (matches codebase-context.md)
#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   15
#define SIOD_GPIO_NUM    4
#define SIOC_GPIO_NUM    5
#define Y9_GPIO_NUM     16
#define Y8_GPIO_NUM     17
#define Y7_GPIO_NUM     18
#define Y6_GPIO_NUM     12
#define Y5_GPIO_NUM     10
#define Y4_GPIO_NUM      9
#define Y3_GPIO_NUM      8
#define Y2_GPIO_NUM     11
#define VSYNC_GPIO_NUM   6
#define HREF_GPIO_NUM    7
#define PCLK_GPIO_NUM   13

static bool initCamera() {
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_GRAYSCALE;
    config.frame_size   = FRAMESIZE_96X96;
    config.fb_count     = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[CAMERA] FAILED — error: 0x%x\n", err);
        return false;
    }
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("=== Camera Test: OV2640 96x96 Grayscale ===");
    Serial.println("[CAMERA] Initializing...");

    if (!initCamera()) {
        Serial.println(">> Halted. Check wiring and reset.");
        while (true) { delay(1000); }
    }

    Serial.println("[CAMERA] SUCCESS");
}

void loop() {
    static uint32_t frameCount = 0;

    camera_fb_t* fb = esp_camera_fb_get();
    frameCount++;

    if (fb) {
        Serial.printf("[FRAME %2u] OK — %u bytes (%ux%u)\n",
                      frameCount, fb->len, fb->width, fb->height);
        esp_camera_fb_return(fb);
    } else {
        Serial.printf("[FRAME %2u] FAILED — null framebuffer\n", frameCount);
    }

    delay(1000);
}

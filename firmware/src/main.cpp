#include <Arduino.h>
#include "motor.h"
#include "camera.h"
#include "wifi_config.h"
#include "wifi_controller.h"

void setup() {
    Serial.begin(115200);
    delay(1000); // Đợi Serial ổn định
    Serial.printf("\n[System] PSRAM Size: %d bytes\n", ESP.getPsramSize());
    
    if (!initCamera()) {
        Serial.println("[Camera] Skipping — web UI still available");
    }
    connectWiFi(WIFI_SSID, WIFI_PASSWORD);
    startWebServer();
    initMotor();
}

void loop() {}

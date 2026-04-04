#include <Arduino.h>
#include "motor.h"
#include "camera.h"
#include "wifi_config.h"
#include "wifi_controller.h"
#include "inference.h"
#include "robot_mode.h"

static void runMotorTest() {
    Serial.println("[TEST] Running motor test sequence...");
    setMotor(160, 160);  delay(1000);
    setMotor(0, 0);      delay(500);
    setMotor(-160, 160); delay(500);
    setMotor(0, 0);      delay(500);
    setMotor(160, -160); delay(500);
    setMotor(0, 0);
    Serial.println("[TEST] Done.");
}

void setup() {
    Serial.begin(115200);
    delay(1000); // Đợi Serial ổn định
    setCpuFrequencyMhz(240);
    Serial.printf("\n[System] CPU: %u MHz\n", getCpuFrequencyMhz());
    Serial.printf("[System] PSRAM Size: %d bytes\n", ESP.getPsramSize());

    setRobotMode(MODE_CONTROL);
    connectWiFi(WIFI_SSID, WIFI_PASSWORD);
    startWebServer();
    initMotor();
    if (initCamera()) {
        setCameraModeStream();
    }
    if (!initInference()) {
        Serial.println("[MAIN] Inference init failed — CNN mode unavailable");
    }
}

void loop() {
    if (Serial.available()) {
        const char c = Serial.read();
        if (c == 'T') runMotorTest();
    }
}

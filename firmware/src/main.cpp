#include <Arduino.h>
#include "motor.h"
#include "wifi_config.h"
#include "wifi_controller.h"

void setup() {
    Serial.begin(115200);
    connectWiFi(WIFI_SSID, WIFI_PASSWORD);
    startWebServer();
    initMotor();
}

void loop() {}

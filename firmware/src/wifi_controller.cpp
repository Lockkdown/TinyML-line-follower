#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#include "motor.h"
#include "wifi_controller.h"
#include "wifi_ui.h"

static AsyncWebServer server(80);

static void registerRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/html", WIFI_CONTROLLER_HTML);
    });

    server.on("/forward", HTTP_POST, [](AsyncWebServerRequest* request) {
        setMotor(SPEED_FULL, SPEED_FULL);
        request->send(200);
    });

    server.on("/left", HTTP_POST, [](AsyncWebServerRequest* request) {
        setMotor(SPEED_TURN, SPEED_FULL);
        request->send(200);
    });

    server.on("/right", HTTP_POST, [](AsyncWebServerRequest* request) {
        setMotor(SPEED_FULL, SPEED_TURN);
        request->send(200);
    });

    server.on("/stop", HTTP_POST, [](AsyncWebServerRequest* request) {
        setMotor(0, 0);
        request->send(200);
    });
}

void connectWiFi(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    Serial.print("[WiFi] Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("[WiFi] Connected — IP: ");
    Serial.println(WiFi.localIP());
}

void startWebServer() {
    registerRoutes();
    server.begin();
    Serial.println("[WebServer] Started on port 80");
}

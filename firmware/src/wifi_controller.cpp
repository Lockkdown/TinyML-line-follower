// file: firmware/src/wifi_controller.cpp
// purpose: setup wifi connection and handle web server endpoints
// dependencies: motor.h, camera.h, wifi_controller.h, wifi_ui.h, wifi_config.h, jpeg_response.h

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <esp_camera.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "motor.h"
#include "camera.h"
#include "wifi_controller.h"
#include "wifi_ui.h"
#include "wifi_config.h"
#include "jpeg_response.h"
#include "inference.h"
#include "controller.h"

static AsyncWebServer server(80);
uint8_t g_speed = 100;

// CNN mode state
static bool cnnModeActive = false;
static TaskHandle_t cnnTaskHandle = nullptr;

// CNN loop task runs on Core 0
void cnnLoopTask(void* param) {
    while (cnnModeActive) {
        int cls = runInference();
        if (cls >= 0 && cls <= 3) {
            classToAction(cls);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // ~10fps
    }
    // Safety stop when CNN mode exits
    setMotor(0, 0);
    vTaskDelete(nullptr);
}

static void registerRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/html", WIFI_CONTROLLER_HTML);
    });

    server.on("/speed", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (request->hasParam("value", true)) {
            const AsyncWebParameter* p = request->getParam("value", true);
            int val = p->value().toInt();
            g_speed = constrain(val, 40, 255);
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Missing value");
        }
    });

    server.on("/forward", HTTP_POST, [](AsyncWebServerRequest* request) {
        cnnModeActive = false;
        setMotor(0, 0);
        setMotor(g_speed, g_speed);
        request->send(200);
    });

    server.on("/left", HTTP_POST, [](AsyncWebServerRequest* request) {
        cnnModeActive = false;
        setMotor(0, 0);
        setMotor(g_speed / 2, g_speed);
        request->send(200);
    });

    server.on("/right", HTTP_POST, [](AsyncWebServerRequest* request) {
        cnnModeActive = false;
        setMotor(0, 0);
        setMotor(g_speed, g_speed / 2);
        request->send(200);
    });

    server.on("/backward", HTTP_POST, [](AsyncWebServerRequest* request) {
        cnnModeActive = false;
        setMotor(0, 0);
        setMotor(-g_speed, -g_speed);
        request->send(200);
    });

    server.on("/stop", HTTP_POST, [](AsyncWebServerRequest* request) {
        cnnModeActive = false;
        setMotor(0, 0);
        request->send(200);
    });

    server.on("/snapshot", HTTP_GET, [](AsyncWebServerRequest* request) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb || fb->len == 0) {
            Serial.println("[Camera] Capture failed or empty frame");
            if (fb) esp_camera_fb_return(fb);
            request->send(503, "text/plain", "frame unavailable");
            return;
        }

        if (fb->format != PIXFORMAT_JPEG) {
            Serial.printf("[Camera] Wrong format: %d\n", fb->format);
            esp_camera_fb_return(fb);
            request->send(503, "text/plain", "wrong format");
            return;
        }

        uint8_t* buf = (uint8_t*)malloc(fb->len);
        if (!buf) {
            esp_camera_fb_return(fb);
            request->send(503, "text/plain", "Out of memory");
            return;
        }

        memcpy(buf, fb->buf, fb->len);
        size_t len = fb->len;
        esp_camera_fb_return(fb); // Giải phóng PSRAM ngay lập tức

        HeapJpegResponse* response = new HeapJpegResponse(buf, len);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
        request->send(response);
    });

    // CNN mode endpoints
    server.on("/cnn/start", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        response->addHeader("Access-Control-Allow-Headers", "*");
        request->send(response);
    });

    server.on("/cnn/start", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!cnnModeActive) {
            cnnModeActive = true;
            xTaskCreatePinnedToCore(cnnLoopTask, "CNN Loop", 16384, nullptr, 1, &cnnTaskHandle, 1);
        }
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\":\"started\"}");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });

    server.on("/cnn/stop", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        response->addHeader("Access-Control-Allow-Headers", "*");
        request->send(response);
    });

    server.on("/cnn/stop", HTTP_POST, [](AsyncWebServerRequest* request) {
        cnnModeActive = false;
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", "{\"status\":\"stopped\"}");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });

    server.on("/cnn/status", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
        response->addHeader("Access-Control-Allow-Headers", "*");
        request->send(response);
    });

    server.on("/cnn/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        String json = "{\"active\":" + String(cnnModeActive ? "true" : "false");
        json += ",\"class\":" + String(getLastClass());
        json += ",\"class_name\":\"";
        int cls = getLastClass();
        switch (cls) {
            case 0: json += "forward"; break;
            case 1: json += "left"; break;
            case 2: json += "right"; break;
            case 3: json += "nothing"; break;
            default: json += "unknown"; break;
        }
        json += "\",\"confidence\":" + String(getLastConfidence(), 3);
        json += "}";
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });
}

void connectWiFi(const char* ssid, const char* password) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
    delay(100);

    // Giữ kết nối ổn định cho poll liên tục, giảm lag ping
    WiFi.setSleep(false);
    
    // Giảm TX Power để tránh sụt áp khi rút cáp Type-C
    WiFi.setTxPower(WIFI_POWER_15dBm);

    WiFi.begin(ssid, password);
    Serial.print("[WiFi] Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("[WiFi] Connected");
    Serial.print("[WiFi] SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("[WiFi] Local IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WiFi] Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("[WiFi] Subnet: ");
    Serial.println(WiFi.subnetMask());
}

void startWebServer() {
    registerRoutes();
    server.begin();
    Serial.println("[WebServer] Started on port 80");
}

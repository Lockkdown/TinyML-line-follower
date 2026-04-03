// file: firmware/src/wifi_controller.cpp
// purpose: setup wifi connection and handle web server endpoints
// dependencies: motor.h, camera.h, wifi_controller.h, wifi_ui.h, wifi_config.h, jpeg_response.h

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "motor.h"
#include "camera.h"
#include "wifi_controller.h"
#include "wifi_ui.h"
#include "wifi_config.h"
#include "jpeg_response.h"
#include "robot_mode.h"

static AsyncWebServer server(80);
uint8_t g_speed = 100;
static volatile bool g_cnnTransitionScheduled = false;
static volatile bool g_wifiReconnectTaskStarted = false;

static const char* modeToString(RobotMode mode) {
    switch (mode) {
        case MODE_CONTROL:
            return "control";
        case MODE_DATASET:
            return "dataset";
        case MODE_CNN:
            return "cnn";
        default:
            return "unknown";
    }
}

static void cnnTransitionTask(void* param) {
    vTaskDelay(pdMS_TO_TICKS(500));
    setRobotMode(MODE_CNN);
    g_cnnTransitionScheduled = false;
    vTaskDelete(nullptr);
}

static void wifiReconnectTask(void* param) {
    (void)param;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WiFi] Disconnected — reconnecting");
            WiFi.reconnect();
        }
    }
}

static void registerRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/html", WIFI_CONTROLLER_HTML);
    });

    server.on("/ping", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "pong");
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

    server.on("/motor/trim", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (request->hasParam("left", true) && request->hasParam("right", true)) {
            const AsyncWebParameter* pl = request->getParam("left", true);
            const AsyncWebParameter* pr = request->getParam("right", true);
            float left = pl->value().toFloat();
            float right = pr->value().toFloat();
            setMotorTrim(left, right);
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Missing left/right");
        }
    });

    server.on("/motor/trim", HTTP_GET, [](AsyncWebServerRequest* request) {
        String json = "{\"left\":" + String(getLeftTrim(), 3) + ",\"right\":" + String(getRightTrim(), 3) + "}";
        request->send(200, "application/json", json);
    });

    server.on("/forward", HTTP_POST, [](AsyncWebServerRequest* request) {
        setMotor(g_speed, g_speed);
        request->send(200);
    });

    server.on("/left", HTTP_POST, [](AsyncWebServerRequest* request) {
        setMotor(g_speed / 2, g_speed);
        request->send(200);
    });

    server.on("/right", HTTP_POST, [](AsyncWebServerRequest* request) {
        setMotor(g_speed, g_speed / 2);
        request->send(200);
    });

    server.on("/backward", HTTP_POST, [](AsyncWebServerRequest* request) {
        setMotor(-g_speed, -g_speed);
        request->send(200);
    });

    server.on("/stop", HTTP_POST, [](AsyncWebServerRequest* request) {
        setMotor(0, 0);
        request->send(200);
    });

    server.on("/mode", HTTP_GET, [](AsyncWebServerRequest* request) {
        String json = "{\"mode\":\"";
        json += modeToString(getRobotMode());
        json += "\"}";
        request->send(200, "application/json", json);
    });

    server.on("/camera/on", HTTP_POST, [](AsyncWebServerRequest* request) {
        setRobotMode(MODE_DATASET);
        String json = "{\"mode\":\"";
        json += modeToString(getRobotMode());
        json += "\"}";
        request->send(200, "application/json", json);
    });

    server.on("/camera/off", HTTP_POST, [](AsyncWebServerRequest* request) {
        setRobotMode(MODE_CONTROL);
        String json = "{\"mode\":\"";
        json += modeToString(getRobotMode());
        json += "\"}";
        request->send(200, "application/json", json);
    });

    server.on("/snapshot", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (getRobotMode() != MODE_DATASET) {
            request->send(503, "text/plain", "Camera OFF - switch to dataset mode");
            return;
        }

        camera_fb_t* fb = nullptr;
        if (!captureFrame(&fb) || !fb || fb->len == 0) {
            Serial.println("[Camera] Capture failed or empty frame");
            if (fb) {
                esp_camera_fb_return(fb);
            }
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

    server.on("/cnn/start", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        response->addHeader("Access-Control-Allow-Headers", "*");
        request->send(response);
    });

    server.on("/cnn/start", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!g_cnnTransitionScheduled) {
            g_cnnTransitionScheduled = true;
            xTaskCreatePinnedToCore(cnnTransitionTask, "CNN Transition", 4096, nullptr, 1, nullptr, 1);
        }
        AsyncWebServerResponse* response = request->beginResponse(
            200,
            "application/json",
            "{\"status\":\"entering_cnn\",\"message\":\"WiFi will disconnect in 500ms\"}"
        );
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Connection", "close");
        request->send(response);
    });

    server.on("/cnn/stop", HTTP_OPTIONS, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse(200);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        response->addHeader("Access-Control-Allow-Headers", "*");
        request->send(response);
    });

    server.on("/cnn/stop", HTTP_POST, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse(
            409,
            "application/json",
            "{\"status\":\"not_supported\",\"message\":\"Reset device to exit CNN mode\"}"
        );
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

    if (!g_wifiReconnectTaskStarted) {
        g_wifiReconnectTaskStarted = true;
        xTaskCreatePinnedToCore(
            wifiReconnectTask, "WiFi Reconnect", 4096, nullptr, 1, nullptr, 1);
    }
}

void startWebServer() {
    registerRoutes();
    server.begin();
    Serial.println("[WebServer] Started on port 80");
}

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <esp_camera.h>

#include "motor.h"
#include "camera.h"
#include "wifi_controller.h"
#include "wifi_ui.h"
#include "wifi_config.h"

static AsyncWebServer server(80);

class AsyncJpegCopyResponse : public AsyncAbstractResponse {
private:
    uint8_t* _buf;
    size_t _len;
    size_t _index;
public:
    AsyncJpegCopyResponse(const camera_fb_t* fb) : _buf(nullptr), _len(0), _index(0) {
        _code = 200;
        _contentType = "image/jpeg";
        if (fb && fb->len > 0) {
            // Cố gắng dùng RAM nội (DRAM) trước vì QVGA JPEG rất nhỏ (~15KB) và nhanh hơn
            _buf = static_cast<uint8_t*>(malloc(fb->len));
            if (!_buf) {
                // Fallback qua PSRAM nếu RAM nội đầy
                _buf = static_cast<uint8_t*>(ps_malloc(fb->len));
            }
            
            if (_buf) {
                memcpy(_buf, fb->buf, fb->len);
                _len = fb->len;
                _contentLength = _len;
            } else {
                _code = 503;
            }
        } else {
            _code = 503;
        }
    }
    ~AsyncJpegCopyResponse() {
        if (_buf) {
            free(_buf); // free works for both malloc and ps_malloc
        }
    }
    bool _sourceValid() const override {
        return _buf != nullptr;
    }
    size_t _fillBuffer(uint8_t* buf, size_t maxLen) override {
        if (!_buf || _index >= _len) return 0;
        size_t ret = _len - _index;
        if (ret > maxLen) ret = maxLen;
        memcpy(buf, _buf + _index, ret);
        _index += ret;
        return ret;
    }
};

static bool isValidJpegFrame(const camera_fb_t* frame_buffer) {
    if (frame_buffer == nullptr || frame_buffer->buf == nullptr || frame_buffer->len < 4) {
        return false;
    }

    const uint8_t jpeg_start_0 = 0xFF;
    const uint8_t jpeg_start_1 = 0xD8;
    const uint8_t jpeg_end_0 = 0xFF;
    const uint8_t jpeg_end_1 = 0xD9;

    return frame_buffer->buf[0] == jpeg_start_0
        && frame_buffer->buf[1] == jpeg_start_1
        && frame_buffer->buf[frame_buffer->len - 2] == jpeg_end_0
        && frame_buffer->buf[frame_buffer->len - 1] == jpeg_end_1;
}

static void registerRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/html", WIFI_CONTROLLER_HTML);
    });

    server.on("/forward", HTTP_POST, [](AsyncWebServerRequest* request) {
        // Both motors at full speed
        setMotor(SPEED_FULL, SPEED_FULL);
        request->send(200);
    });

    server.on("/left", HTTP_POST, [](AsyncWebServerRequest* request) {
        setMotor(0, SPEED_FULL); // Use 0 to make it pivot on left wheel
        request->send(200);
    });

    server.on("/right", HTTP_POST, [](AsyncWebServerRequest* request) {
        setMotor(SPEED_FULL, 0); // Use 0 to make it pivot on right wheel
        request->send(200);
    });

    server.on("/backward", HTTP_POST, [](AsyncWebServerRequest* request) {
        // Both motors at full speed (reverse)
        setMotor(-SPEED_FULL, -SPEED_FULL);
        request->send(200);
    });

    server.on("/stop", HTTP_POST, [](AsyncWebServerRequest* request) {
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

        // Tạo một buffer trên heap (RAM nội thay vì PSRAM) để chứa ảnh, 
        // vì AsyncWebServer làm việc với RAM nội ổn định hơn.
        uint8_t* buf = (uint8_t*)malloc(fb->len);
        if (!buf) {
            esp_camera_fb_return(fb);
            request->send(503, "text/plain", "Out of memory");
            return;
        }

        memcpy(buf, fb->buf, fb->len);
        size_t len = fb->len;
        esp_camera_fb_return(fb); // Giải phóng PSRAM ngay lập tức

        // Dùng AsyncAbstractResponse chuẩn của thư viện
        class HeapJpegResponse : public AsyncAbstractResponse {
        private:
            uint8_t* _buf;
            size_t _len;
            size_t _index;
        public:
            HeapJpegResponse(uint8_t* buf, size_t len) : _buf(buf), _len(len), _index(0) {
                _code = 200;
                _contentType = "image/jpeg";
                _contentLength = len;
            }
            ~HeapJpegResponse() {
                if (_buf) free(_buf); // Tự động dọn RAM khi gửi xong
            }
            bool _sourceValid() const override {
                return _buf != nullptr;
            }
            size_t _fillBuffer(uint8_t* data, size_t maxLen) override {
                if (!_buf || _index >= _len) return 0;
                size_t ret = _len - _index;
                if (ret > maxLen) ret = maxLen;
                memcpy(data, _buf + _index, ret);
                _index += ret;
                return ret;
            }
        };

        HeapJpegResponse* response = new HeapJpegResponse(buf, len);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
        request->send(response);
    });
}

void connectWiFi(const char* ssid, const char* password) {
    IPAddress local_ip, gateway_ip, subnet_mask;
    local_ip.fromString(STATIC_IP);
    gateway_ip.fromString(GATEWAY);
    subnet_mask.fromString(SUBNET);
    
    if (!WiFi.config(local_ip, gateway_ip, subnet_mask)) {
        Serial.println("[WiFi] Static IP config failed, using DHCP");
    }
    
    WiFi.begin(ssid, password);
    Serial.print("[WiFi] Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("[WiFi] Static IP: ");
    Serial.println(WiFi.localIP());
}

void startWebServer() {
    registerRoutes();
    server.begin();
    Serial.println("[WebServer] Started on port 80");
}

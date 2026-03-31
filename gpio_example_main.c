#include "driver/gpio.h"
#include "esp_camera.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "Robot_Car";

// --- 1. CẤU HÌNH WIFI ---
#define WIFI_SSID "Lockdown"
#define WIFI_PASS "phuc12345"

// --- 2. CẤU HÌNH CHÂN L298N (Đã sửa để né chân Boot) ---
#define GPIO_IN1 1
#define GPIO_IN2 2
#define GPIO_IN3 42
#define GPIO_IN4 41
#define GPIO_ENA 14 // Đã đổi sang 14 (An toàn)
#define GPIO_ENB 21 // Đã đổi sang 21 (An toàn)

// --- 3. CẤU HÌNH CHÂN CAMERA (ESP32-S3 CAM) ---
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5
#define Y9_GPIO_NUM 16
#define Y8_GPIO_NUM 17
#define Y7_GPIO_NUM 18
#define Y6_GPIO_NUM 12
#define Y5_GPIO_NUM 10
#define Y4_GPIO_NUM 11
#define Y3_GPIO_NUM 9
#define Y2_GPIO_NUM 8
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13

// --- HÀM ĐIỀU KHIỂN ĐỘNG CƠ ---
void control_car(int in1, int in2, int in3, int in4) {
  gpio_set_level(GPIO_IN1, in1);
  gpio_set_level(GPIO_IN2, in2);
  gpio_set_level(GPIO_IN3, in3);
  gpio_set_level(GPIO_IN4, in4);
}

// --- GIAO DIỆN WEB CHUYÊN NGHIỆP ---
static const char *index_html =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' "
    "content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable="
    "no'>"
    "<title>ESP32-S3 Robot Car Controller</title>"
    "<style>"
    "*{margin:0;padding:0;box-sizing:border-box}"
    "body{font-family:'Segoe "
    "UI',system-ui,-apple-system,sans-serif;background:linear-gradient(135deg,#"
    "667eea 0%,#764ba2 100%);"
    "min-height:100vh;display:flex;align-items:center;justify-content:center;"
    "padding:20px}"
    ".container{background:rgba(255,255,255,0.98);border-radius:24px;padding:"
    "30px;max-width:600px;width:100%;"
    "box-shadow:0 20px 60px rgba(0,0,0,0.3);animation:slideIn 0.5s ease}"
    "@keyframes "
    "slideIn{from{opacity:0;transform:translateY(30px)}to{opacity:1;transform:"
    "translateY(0)}}"
    ".header{text-align:center;margin-bottom:25px}"
    ".header "
    "h1{color:#333;font-size:28px;font-weight:700;margin-bottom:8px;display:"
    "flex;align-items:center;justify-content:center;gap:10px}"
    ".header p{color:#666;font-size:14px}"
    ".status-bar{display:flex;gap:10px;margin-bottom:20px}"
    ".status{flex:1;padding:12px;border-radius:12px;font-size:13px;font-weight:"
    "600;text-align:center;transition:all 0.3s}"
    ".status-cmd{background:linear-gradient(135deg,#11998e 0%,#38ef7d "
    "100%);color:white}"
    ".status-cam{background:linear-gradient(135deg,#4facfe 0%,#00f2fe "
    "100%);color:white}"
    ".video-wrapper{position:relative;margin-bottom:25px}"
    ".video-container{width:100%;aspect-ratio:4/"
    "3;background:#000;border-radius:16px;overflow:hidden;"
    "box-shadow:0 10px 30px rgba(0,0,0,0.2);position:relative}"
    ".video-container img{width:100%;height:100%;object-fit:cover}"
    ".cam-overlay{position:absolute;top:10px;right:10px;background:rgba(0,0,0,"
    "0.7);color:white;padding:6px 12px;"
    "border-radius:20px;font-size:12px;backdrop-filter:blur(10px)}"
    ".controls{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;max-"
    "width:350px;margin:0 auto}"
    ".btn{background:linear-gradient(135deg,#667eea 0%,#764ba2 "
    "100%);color:white;border:none;padding:22px;"
    "font-size:20px;font-weight:600;border-radius:16px;cursor:pointer;"
    "transition:all 0.2s ease;"
    "box-shadow:0 4px 15px "
    "rgba(102,126,234,0.4);user-select:none;touch-action:manipulation;position:"
    "relative;overflow:hidden}"
    ".btn::before{content:'';position:absolute;top:50%;left:50%;width:0;height:"
    "0;border-radius:50%;"
    "background:rgba(255,255,255,0.3);transform:translate(-50%,-50%);"
    "transition:width 0.6s,height 0.6s}"
    ".btn:active::before{width:300px;height:300px}"
    ".btn:hover{transform:translateY(-3px);box-shadow:0 6px 20px "
    "rgba(102,126,234,0.6)}"
    ".btn:active{transform:translateY(1px);box-shadow:0 2px 10px "
    "rgba(102,126,234,0.4)}"
    ".btn-stop{background:linear-gradient(135deg,#f093fb 0%,#f5576c "
    "100%);box-shadow:0 4px 15px rgba(245,87,108,0.4)}"
    ".btn-stop:hover{box-shadow:0 6px 20px rgba(245,87,108,0.6)}"
    ".btn-up{grid-column:2}"
    ".btn-down{grid-column:2}"
    ".footer{text-align:center;margin-top:20px;color:#999;font-size:12px}"
    ".footer a{color:#667eea;text-decoration:none}"
    "@media(max-width:480px){.container{padding:20px}.header "
    "h1{font-size:24px}.btn{padding:18px;font-size:18px}}"
    "</style></head><body>"
    "<div class='container'>"
    "<div class='header'><h1><span>🤖</span>ESP32-S3 Robot "
    "Car<span>🚗</span></h1>"
    "<p>Advanced Control System v2.0</p></div>"
    "<div class='status-bar'>"
    "<div class='status status-cmd' id='status'>✅ Ready</div>"
    "<div class='status status-cam' id='camStatus'>📹 Live</div>"
    "</div>"
    "<div class='video-wrapper'>"
    "<div class='video-container'><img src='/stream' id='stream' "
    "onerror='camErr()'>"
    "<div class='cam-overlay' id='fps'>-- FPS</div></div>"
    "</div>"
    "<div class='controls'>"
    "<button class='btn btn-up' onmousedown='cmd(\"forward\")' "
    "onmouseup='cmd(\"stop\")' "
    "ontouchstart='cmd(\"forward\")' ontouchend='cmd(\"stop\")'>⬆️</button>"
    "<button class='btn' onmousedown='cmd(\"left\")' onmouseup='cmd(\"stop\")' "
    "ontouchstart='cmd(\"left\")' ontouchend='cmd(\"stop\")'>⬅️</button>"
    "<button class='btn btn-stop' onclick='cmd(\"stop\")'>🛑</button>"
    "<button class='btn' onmousedown='cmd(\"right\")' "
    "onmouseup='cmd(\"stop\")' "
    "ontouchstart='cmd(\"right\")' ontouchend='cmd(\"stop\")'>➡️</button>"
    "<button class='btn btn-down' onmousedown='cmd(\"backward\")' "
    "onmouseup='cmd(\"stop\")' "
    "ontouchstart='cmd(\"backward\")' ontouchend='cmd(\"stop\")'>⬇️</button>"
    "</div>"
    "<div class='footer'>Powered by <a href='#'>ESP32-S3</a> | FreeRTOS</div>"
    "</div>"
    "<script>"
    "let fc=0,lt=Date.now();"
    "function "
    "cmd(c){fetch('/"
    "'+c).then(r=>{if(r.ok){document.getElementById('status').innerHTML='🎮 "
    "'+c.toUpperCase();"
    "setTimeout(()=>{if(c!='stop')document.getElementById('status').innerHTML='"
    "✅ Ready'},2000)}}).catch(()=>{"
    "document.getElementById('status').innerHTML='❌ Error'})}"
    "function camErr(){document.getElementById('camStatus').innerHTML='⚠️ "
    "Offline'}"
    "setInterval(()=>{fc++;let "
    "now=Date.now();if(now-lt>1000){document.getElementById('fps').innerHTML="
    "fc+' FPS';"
    "document.getElementById('camStatus').innerHTML='📹 "
    "Live';fc=0;lt=now}},100);"
    "document.addEventListener('keydown',e=>{if(e.repeat)return;switch(e.key){"
    "case'ArrowUp':case'w':cmd('forward');break;"
    "case'ArrowDown':case's':cmd('backward');break;case'ArrowLeft':case'a':cmd("
    "'left');break;"
    "case'ArrowRight':case'd':cmd('right');break;case' ':cmd('stop');break}});"
    "document.addEventListener('keyup',e=>{if(e.key.startsWith('Arrow')||'wasd'"
    ".includes(e.key))cmd('stop')});"
    "</script></body></html>";

// --- HTTP HANDLERS ---
esp_err_t index_handler(httpd_req_t *req) {
  return httpd_resp_send(req, index_html, strlen(index_html));
}

esp_err_t stream_handler(httpd_req_t *req) {
  // Camera disabled - return placeholder
  const char *placeholder =
      "<html><body "
      "style='background:#000;color:#fff;text-align:center;padding:50px;'>"
      "<h2>📹 Camera Disabled</h2>"
      "<p>Camera is temporarily disabled to ensure motor control works "
      "properly.</p>"
      "<p>Motor control is now active. Test the buttons below!</p>"
      "<a href='/' style='color:#4CAF50;'>← Back to Control Panel</a>"
      "</body></html>";
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, placeholder, strlen(placeholder));
}

esp_err_t forward_handler(httpd_req_t *req) {
  control_car(1, 0, 1, 0);
  ESP_LOGI(TAG, "FORWARD command executed");
  return httpd_resp_send(req, "OK", 2);
}
esp_err_t backward_handler(httpd_req_t *req) {
  control_car(0, 1, 0, 1);
  ESP_LOGI(TAG, "BACKWARD command executed");
  return httpd_resp_send(req, "OK", 2);
}
esp_err_t left_handler(httpd_req_t *req) {
  control_car(0, 1, 1, 0);
  ESP_LOGI(TAG, "LEFT command executed");
  return httpd_resp_send(req, "OK", 2);
}
esp_err_t right_handler(httpd_req_t *req) {
  control_car(1, 0, 0, 1);
  ESP_LOGI(TAG, "RIGHT command executed");
  return httpd_resp_send(req, "OK", 2);
}
esp_err_t stop_handler(httpd_req_t *req) {
  control_car(0, 0, 0, 0);
  ESP_LOGI(TAG, "STOP command executed");
  return httpd_resp_send(req, "OK", 2);
}

void start_web_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.max_uri_handlers = 8;
  httpd_handle_t server = NULL;
  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_uri_t uri_get = {
        .uri = "/", .method = HTTP_GET, .handler = index_handler};
    httpd_register_uri_handler(server, &uri_get);
    httpd_uri_t uri_stream = {
        .uri = "/stream", .method = HTTP_GET, .handler = stream_handler};
    httpd_register_uri_handler(server, &uri_stream);
    httpd_uri_t uri_fwd = {
        .uri = "/forward", .method = HTTP_GET, .handler = forward_handler};
    httpd_register_uri_handler(server, &uri_fwd);
    httpd_uri_t uri_bwd = {
        .uri = "/backward", .method = HTTP_GET, .handler = backward_handler};
    httpd_register_uri_handler(server, &uri_bwd);
    httpd_uri_t uri_left = {
        .uri = "/left", .method = HTTP_GET, .handler = left_handler};
    httpd_register_uri_handler(server, &uri_left);
    httpd_uri_t uri_right = {
        .uri = "/right", .method = HTTP_GET, .handler = right_handler};
    httpd_register_uri_handler(server, &uri_right);
    httpd_uri_t uri_stop = {
        .uri = "/stop", .method = HTTP_GET, .handler = stop_handler};
    httpd_register_uri_handler(server, &uri_stop);
  }
}

// --- WIFI EVENT HANDLER ---
static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id,
                               void *data) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
    esp_wifi_connect();
  else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
    esp_wifi_connect();
  else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
    ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&event->ip_info.ip));
    start_web_server();
  }
}

void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // 1. Khởi tạo Motor GPIO
  int pins[] = {GPIO_IN1, GPIO_IN2, GPIO_IN3, GPIO_IN4, GPIO_ENA, GPIO_ENB};
  for (int i = 0; i < 6; i++) {
    gpio_reset_pin(pins[i]);
    gpio_set_direction(pins[i], GPIO_MODE_OUTPUT);
  }
  gpio_set_level(GPIO_ENA, 1); // Bật Motor A
  gpio_set_level(GPIO_ENB, 1); // Bật Motor B
  control_car(0, 0, 0, 0);

  // 2. Camera - TẠM THỜI DISABLE ĐỂ XE CHẠY ĐƯỢC
  ESP_LOGI(TAG, "Camera disabled - focusing on motor control first");

  // TODO: Sẽ enable camera sau khi xe chạy ổn định
  /*
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.frame_size = FRAMESIZE_QQVGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 5;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
    return;
  }

  // Wait for camera to stabilize
  vTaskDelay(pdMS_TO_TICKS(500));

  // Configure OV2640 sensor
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_framesize(s, FRAMESIZE_QQVGA);
    s->set_quality(s, 5);
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_whitebal(s, 1);
    s->set_gain_ctrl(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_awb_gain(s, 1);
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
    ESP_LOGI(TAG, "Camera sensor configured");

    // Flush first few bad frames
    for (int i = 0; i < 5; i++) {
      camera_fb_t *fb = esp_camera_fb_get();
      if (fb) {
        esp_camera_fb_return(fb);
      }
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "Camera warmup complete");
  }
  */

  // 3. Khởi tạo WiFi
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                      &wifi_event_handler, NULL, NULL);
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                      &wifi_event_handler, NULL, NULL);
  wifi_config_t wifi_config = {
      .sta = {.ssid = WIFI_SSID, .password = WIFI_PASS}};
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  esp_wifi_start();
}
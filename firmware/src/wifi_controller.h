#pragma once

constexpr int SPEED_FULL = 255;
// DEPRECATED: Motor compensation workaround - motors now run at equal speed
// constexpr int SPEED_RIGHT_COMPENSATED = 185; // Tăng từ 150 lên 190 để cân bằng với bên trái
constexpr int SPEED_TURN = 150;

void connectWiFi(const char* ssid, const char* password);
void startWebServer();

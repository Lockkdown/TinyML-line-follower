#pragma once

constexpr int SPEED_FULL = 200;
constexpr int SPEED_TURN = 80;

void connectWiFi(const char* ssid, const char* password);
void startWebServer();

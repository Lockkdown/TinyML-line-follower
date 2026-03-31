#include "../../src/motor.h"

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("=== Motor Test: L298N Manual Control ===");
    initMotor();
    Serial.println("[MOTOR] Initialized — starting test sequence");
}

void loop() {
    Serial.println("[FORWARD] left=200, right=200");
    setMotor(200, 200);
    delay(2000);

    Serial.println("[LEFT]    left=80,  right=200");
    setMotor(80, 200);
    delay(2000);

    Serial.println("[RIGHT]   left=200, right=80");
    setMotor(200, 80);
    delay(2000);

    Serial.println("[STOP]    left=0,   right=0");
    setMotor(0, 0);
    delay(2000);
}

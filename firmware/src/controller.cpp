// file: firmware/src/controller.cpp
// purpose: map CNN class index to motor actions
// dependencies: controller.h, motor.h

#include <Arduino.h>
#include "controller.h"
#include "motor.h"

// Reduced from 180 → 120: lower speed gives more physical time to
// complete turns before the robot overshoots a 90° junction.
static int g_cnn_base_speed = 120;

void setCnnBaseSpeed(int speed) {
    if (speed < CNN_SPEED_FORWARD) speed = CNN_SPEED_FORWARD;
    if (speed > 255) speed = 255;
    g_cnn_base_speed = speed;
}

int getCnnBaseSpeed() {
    return g_cnn_base_speed;
}

// Maps confidence → inner wheel PWM (inverse: high conf = lower inner speed)
static int calcInnerSpeed(float conf) {
    float t = (conf - CONF_MIN) / (CONF_MAX - CONF_MIN);
    t = constrain(t, 0.0f, 1.0f);
    return (int)(INNER_MAX - t * (INNER_MAX - INNER_MIN));
}

void classToAction(int cls, float conf) {
    static bool trimInitialized = false;
    if (!trimInitialized) {
        setMotorTrim(CNN_LEFT_TRIM, CNN_RIGHT_TRIM);
        trimInitialized = true;
    }
    
    // Lưu lại hướng rẽ cuối cùng có line
    static int last_seen_line_class = 0;
    if (cls != 3) {
        last_seen_line_class = cls;
    }

    int left_speed = 0;
    int right_speed = 0;
    switch (cls) {
        case 0: // forward
            left_speed = BASE_SPEED;
            right_speed = BASE_SPEED;
            break;
        case 1: // left
            left_speed = calcInnerSpeed(conf);
            right_speed = BASE_SPEED;
            break;
        case 2: // right
            left_speed = BASE_SPEED;
            right_speed = calcInnerSpeed(conf);
            break;
        case 3: // nothing -> RECOVERY
            if (last_seen_line_class == 1) {
                // Lost line while turning left -> Spin left in place
                left_speed = -RECOVERY_SPEED_TURN;
                right_speed = RECOVERY_SPEED_TURN;
            } else if (last_seen_line_class == 2) {
                // Lost line while turning right -> Spin right in place
                left_speed = RECOVERY_SPEED_TURN;
                right_speed = -RECOVERY_SPEED_TURN;
            } else {
                // Lost line while going straight -> Back up slowly
                left_speed = RECOVERY_SPEED_FWD;
                right_speed = RECOVERY_SPEED_FWD;
            }
            break;
        default:
            break;
    }
    Serial.printf("[CNN] motor left=%d right=%d\n", left_speed, right_speed);
    setMotor(left_speed, right_speed);
}

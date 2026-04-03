#pragma once

enum RobotMode {
    MODE_CONTROL = 0,
    MODE_DATASET = 1,
    MODE_CNN = 2
};

void setRobotMode(RobotMode mode);
RobotMode getRobotMode();

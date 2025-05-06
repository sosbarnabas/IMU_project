#include "RedisSingleMotorController.h"
#include <chrono>
#include <thread>
#include <iostream>

void RedisSingleMotorController::loop() {
    while (true) {
        auto command = redis_.rpop("command:" + std::to_string(address_));
        if (command) {
            processCommand(*command);
        } else {
            measureAndStore();
        }
    }
}

void RedisSingleMotorController::processCommand(const std::string& raw_command) {
    try {
        // parse command string pl. "enable", "disable", stb.
        if (raw_command == "enable") {
            auto data = motor_.enable(0);
            exoskeleton::redis_tools::send_ok(redis_, "commandrespart:enable:" + std::to_string(address_), data);
        }
        else if (raw_command == "disable") {
            auto data = motor_.disable(0);
            exoskeleton::redis_tools::send_ok(redis_, "commandrespart:disable:" + std::to_string(address_), data);
        }
        // … további parancsok
    } catch (const std::exception& e) {
        exoskeleton::redis_tools::send_error(redis_, "commandrespart:" + raw_command + ":" + std::to_string(address_), e.what());
    }
}

void RedisSingleMotorController::measureAndStore() {
    auto data_list = motor_.read();
    if (!data_list.empty()) {
        exoskeleton::redis_tools::xadd_motor_data(redis_, address_, data_list[0]);
    }
}


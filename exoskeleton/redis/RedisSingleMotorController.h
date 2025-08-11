#pragma once

#include <sw/redis++/redis++.h>
#include "../motor/MultiPortExoMotors.h"
#include "RedisTools.h"

namespace exoskeleton::core {

class RedisSingleMotorController {
public:
    RedisSingleMotorController(int address, const std::string& port, int n_motors)
        : address_(address), redis_("tcp://127.0.0.1:6379"),
          motor_({port}), n_motors_(n_motors) {}

    void loop();

private:
    int address_;
    sw::redis::Redis redis_;
    MultiPortExoMotors motor_;
    int n_motors_;

    void processCommand(const std::string& raw_command);
    auto measureAndStore() -> SingleMotorData;
};

} // exoskeleton::core

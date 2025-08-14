#pragma once

#include <string>
#include <sw/redis++/redis++.h>
#include "../core/SinglePortExoMotor.h"

namespace exoskeleton::core {

class RedisSinglePortController {
public:
    RedisSinglePortController(int address, const std::string& serial_num, int n_motors);
    void loop();
    void _loop();

private:
    int address_;
    sw::redis::Redis redis_;
    SinglePortExoMotor motor_;
    int n_motors_;

    void processCommand(const std::string& raw_command);
    auto measureAndStore() -> SingleMotorData;
};

} // exoskeleton::core

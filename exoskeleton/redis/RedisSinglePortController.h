#pragma once

#include <sw/redis++/redis++.h>
#include "../motor/SinglePortExoMotor.h"
#include "RedisTools.h"


class RedisSinglePortController {
public:
    RedisSinglePortController(int address, const std::string& port, int n_motors)
        : address_(address), redis_("tcp://127.0.0.1:6379"),
          motor_({port}), n_motors_(n_motors) {}
    void loop();
    void _loop();

private:
    int address_;
    sw::redis::Redis redis_;
    SinglePortExoMotor motor_;
    int n_motors_;

    void processCommand(const std::string& raw_command);
    void measureAndStore();
};

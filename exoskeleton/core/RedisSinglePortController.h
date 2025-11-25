#pragma once

#include <string>
#include <map>
#include <memory>
#include <sw/redis++/redis++.h>
#include "../core/SinglePortExoMotor.h"
#include "../core/DataLogger.h"
#include "../settings/Settings.h"

namespace exoskeleton::core
{

    class RedisSinglePortController
    {
    public:
        RedisSinglePortController(const settings::MotorProps &motor_props, size_t n_motors);
        void loop();
        void _loop();

    private:
        settings::MotorProps motor_props_;
        sw::redis::Redis redis_;
        SinglePortExoMotor motor_;
        size_t n_motors_;
        std::unique_ptr<DataLogger> data_logger_;

        void processCommand(const std::string &raw_command);
        auto measureAndStore() -> SingleMotorData;
    };

} // exoskeleton::core

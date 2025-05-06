#pragma once

#include <sw/redis++/redis++.h>
#include <optional>
#include <string>
#include <unordered_map>
#include "../motor/motor.h"

namespace exoskeleton::redis_tools {
    std::unordered_map<std::string, std::string>
    hgetall_map(sw::redis::Redis &redis, const std::string &key);

    std::optional<std::string>
    get_opt(sw::redis::Redis &redis, const std::string &key);

    bool get_flag(sw::redis::Redis &redis,
                  const std::string &key,
                  bool clear = false);

    sw::redis::Subscriber make_keyspace_subscriber(
        sw::redis::Redis &redis,
        const std::string &key);

    void xadd_motor_data(sw::redis::Redis &redis, int address, const exoskeleton::motor::SingleMotorData &data);

    void send_ok(sw::redis::Redis &redis, const std::string &key, const std::string &record);
    void send_error(sw::redis::Redis &redis, const std::string &key, const std::string &error);

}

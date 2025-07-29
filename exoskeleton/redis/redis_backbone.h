#pragma once

#include <sw/redis++/redis++.h>
#include <chrono>
#include <string>

class RedisBackbone {
public:
    RedisBackbone(
        std::chrono::seconds const& dt,
        std::string const& redis_url = "tcp://127.0.0.1:6379"
        );

    // Logically not const, as it modifies the underlying Redis
    auto operator() () -> void;

private:
    sw::redis::Redis redis_;
    std::chrono::seconds dt_;
};

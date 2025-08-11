#include "redis_backbone.h"
#include <iostream>

namespace exoskeleton::core {

RedisBackbone::RedisBackbone(
    std::chrono::seconds const& dt,
    std::string const& redis_url
    ) : redis_{redis_url}, dt_{dt} {}

auto RedisBackbone::operator() () -> void {
    while (true) {
        auto const t0 = std::chrono::steady_clock::now();

        // First notify motor controllers ...
        this->redis_.set("sync:loop:next", std::to_string(t0.time_since_epoch().count()));

        // ... then check for exit condition.
        // This way they also can notice exit condition.
        if (auto const& exit_flag = redis_.get("exit"); exit_flag && *exit_flag == "1") {
            std::cout << "Exited by \"exit\" command" << std::endl;
            break;
        }

        std::this_thread::sleep_until(t0 + this->dt_);
    }
}

} // exoskeleton::core
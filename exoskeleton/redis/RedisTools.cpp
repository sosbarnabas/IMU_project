#include "RedisTools.h"
#include <unordered_map>
#include <string>
#include <optional>
#include <iterator>  // for std::inserter

namespace exoskeleton::redis_tools {

std::unordered_map<std::string, std::string>
hgetall_map(sw::redis::Redis &redis, const std::string &key) {
    std::unordered_map<std::string, std::string> result;
    redis.hgetall(key, std::inserter(result, result.begin()));
    return result;
}

std::optional<std::string>
get_opt(sw::redis::Redis &redis, const std::string &key) {
    auto v = redis.get(key);
    if (v) {
        return *v;
    }
    return std::nullopt;
}

bool get_flag(sw::redis::Redis &redis,
              const std::string &key,
              bool clear) {
    if (clear) {
        auto val = redis.getset(key, "0");
        if (val) {
            try {
                return std::stoi(*val) != 0;
            } catch (...) {
                return false;
            }
        }
        return false;
    } else {
        auto v = redis.get(key);
        if (!v) return false;
        try {
            return std::stoi(*v) != 0;
        } catch (...) {
            return false;
        }
    }
}

sw::redis::Subscriber make_keyspace_subscriber(
    sw::redis::Redis &redis,
    const std::string &key) {
    // Enable keyspace notifications
    redis.command<std::string>("CONFIG", "SET", "notify-keyspace-events", "KEA");

    // Create subscriber
    auto sub = redis.subscriber();
    std::string channel = "__keyspace@0__:" + key;
    sub.subscribe(channel);
    return sub;
}

} 

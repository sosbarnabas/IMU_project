#include "RedisTools.h"
#include <chrono>
#include <unordered_map>
#include <string>
#include <optional>
#include <iterator>
#include <nlohmann/json.hpp>

namespace exoskeleton::redis_tools {
    namespace {
        auto map_log_level(LogLevel level) -> std::string {
            switch (level) {
                using enum LogLevel;
                case debug:
                    return "DEBUG";
                case info:
                    return "INFO";
                case warning:
                    return "WARNING";
                case error:
                    return "ERROR";
                default:
                    return "UNKNOWN_LEVEL";
            }
        }
    }
    
    std::vector<int> parseJsonArray(const std::string& json_str) {
        auto j = nlohmann::json::parse(json_str);
        std::vector<int> result;

        for (const auto& el : j) {
            result.push_back(el.get<int>());
        }
        return result;
    }

    auto jsonArray(const std::vector<int>& array) -> std::string {
        nlohmann::json out = array;
        return out.dump();
    }

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

    void log(sw::redis::Redis& redis, const std::string& source, const std::string& message, LogLevel level) {
        std::unordered_map<std::string, std::string> fields =  {
            {"t", std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())},
            {"src", source},
            {"level", map_log_level(level)},
            {"msg", message},
        };
        redis.xadd(log_key, "*", fields.begin(), fields.end());
    }
}

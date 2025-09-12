#include "RedisTools.h"
#include <chrono>
#include <unordered_map>
#include <string>
#include <optional>
#include <iterator>
#include <nlohmann/json.hpp>
#include "../motor/ExoMotorsInterface.h"

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

    void xadd_motor_data(sw::redis::Redis &redis, int address, const exoskeleton::core::SingleMotorData &data) {
        std::unordered_map<std::string, std::string> fields = {
            {"t", std::to_string(data.t)},
            {"n_tries", std::to_string(data.n_tries)},
            {"enabled", std::to_string(data.enabled)},
            {"slot_idx", std::to_string(data.slot_idx)},
            {"cmd_cntr", std::to_string(data.cmd_cntr)},
            {"position", std::to_string(data.position)},
            {"torque", std::to_string(data.torque)}
        };

        std::string stream_key = "xdata:" + std::to_string(address);
        redis.xadd(stream_key, "*", fields.begin(), fields.end());
    }



    void send_ok(sw::redis::Redis &redis, const std::string &key, const std::string &record) {
        //std::cerr << key << " " << record << std::endl;
        redis.lpush(key, "OK:" + record);
    }

    void send_error(sw::redis::Redis &redis, const std::string &key, const std::string &error) {
        redis.lpush(key, "ER:" + error);
    }
    std::optional<int> signal_data_ready(sw::redis::Redis &redis, int n_motors) {
        auto new_cnt = redis.incr("sync:data:cnt");
        if (new_cnt == n_motors) {
            auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            auto t = sw::redis::StringView{std::to_string(time)};
            auto p = redis.pipeline();
            p.set("sync:data:cnt","0");
            p.set("sync:data:ready",t);
            p.exec();
            return time;
        }
        else {
            return std::nullopt;
        }

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


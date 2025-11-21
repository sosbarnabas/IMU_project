#pragma once

#include <sw/redis++/redis++.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace exoskeleton::redis_tools {

    // Redis key constants
    static const std::string STARTED_KEY = "started";
    static const std::string SYNC_KEY = "sync:loop:next";
    static const std::string STOP_KEY = "stop";
    static const std::string COMMAND_KEY = "command";
    static const std::string COMMAND_RESULT_KEY = "commandres";
    static const std::string COMMAND_PARTIAL_RESULT_KEY = "commandrespart";
    static const std::string EXIT_KEY = "exit";
    static const std::string log_key = "log";

    [[nodiscard]] auto parseJsonArray(const std::string& json_str) -> std::vector<int>;
    [[nodiscard]] auto jsonArray(const std::vector<int>& array) -> std::string;

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

    enum class LogLevel {
        debug,
        info,
        warning,
        error,
    };
    
    void log(
        sw::redis::Redis& redis,
        const std::string& source,
        const std::string& message,
        LogLevel level = LogLevel::info);

}

#include "RedisFacade.h"
#include "EnvLoader.h"
#include <stdexcept>
#include <optional>

namespace exoskeleton::redis {

Facade::Facade(const std::string &uri)
    : _redis(uri) {}

exoskeleton::settings::Settings Facade::load_env(bool required) {
    auto m = redis_tools::hgetall_map(_redis, "conf:env");
    if (required && m.empty()) {
        throw std::runtime_error("Missing required conf:env in Redis");
    }
    return exoskeleton::settings::Settings::from_map(m);
}

exoskeleton::settings::UserParams Facade::load_user_params() {
    auto m = redis_tools::hgetall_map(_redis, "conf:user");
    if (m.empty()) {
        throw std::runtime_error("Missing conf:user in Redis");
    }
    return exoskeleton::settings::UserParams::from_map(m);
}

void Facade::set_user_params(const exoskeleton::settings::UserParams& params) {
    auto m = params.to_map();
    
    // Save all fields to Redis hash
    for (const auto& [key, value] : m) {
        _redis.hset("conf:user", key, value);
    }
    
    flag_user_params_changed();
}

void Facade::set_user_param(const std::string& key, const std::string& value) {
    _redis.hset("conf:user", key, value);
}

bool Facade::user_params_changed(bool clear) {
    return redis_tools::get_flag(_redis, "conf:user:set", clear);
}

bool Facade::db_params_changed(bool clear) {
    return redis_tools::get_flag(_redis, "dbchanged", clear);
}

std::optional<int> Facade::current_user_id() {
    auto s = redis_tools::get_opt(_redis, "currentuserid");
    if (!s) return std::nullopt;
    try {
        return std::stoi(*s);
    } catch (...) {
        return std::nullopt;
    }
}

void Facade::set_value(const std::string& key, const std::string& value) {
    _redis.set(key, value);
}

void Facade::flag_user_params_changed() {
    _redis.set("conf:user:set", "1");
}

void Facade::flag_db_params_changed() {
    _redis.set("dbchanged", "1");
}

bool Facade::initialize_env_from_file(const std::string& env_path, bool overwrite) {
    try {
        int uploaded = EnvLoader::upload_to_redis(_redis, env_path, overwrite);
        return uploaded > 0;
    } catch (const std::exception& e) {
        return false;
    }
}

void Facade::initialize_run_addrs_from_env() {
    try {
        auto settings = load_env(false);
        
        std::vector<std::pair<std::string, std::string>> motor_mappings;
        int address = 0;
        
        auto add_motor = [&](const std::string& motor_name, const std::string& serial) {
            if (!serial.empty()) {
                std::string value = std::to_string(address) + "|" + serial;
                motor_mappings.emplace_back(motor_name, value);
                address++;
            }
        };
        
        add_motor("e_flex", settings.motor_e_flex);
        add_motor("e_ext", settings.motor_e_ext);
        add_motor("s_flex", settings.motor_s_flex);
        add_motor("s_ext", settings.motor_s_ext);
        add_motor("s_add_pron", settings.motor_s_add_pron);
        add_motor("s_abd", settings.motor_s_abd);
        add_motor("s_add_sup", settings.motor_s_add_sup);
        
        if (!motor_mappings.empty()) {
            _redis.del("run:addrs");
            _redis.hset("run:addrs", motor_mappings.begin(), motor_mappings.end());
        }
    } catch (const std::exception&) {
        // Silently fail - caller will handle missing run:addrs
    }
}

} // namespace exoskeleton::redis

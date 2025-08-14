#include "RedisFacade.h"
#include <stdexcept>
#include <optional>

namespace exoskeleton::redis {

Facade::Facade(const std::string &uri)
    : _redis(uri) {}

settings::Settings Facade::load_env(bool required) {
    auto m = redis_tools::hgetall_map(_redis, "conf:env");
    if (required && m.empty()) {
        throw std::runtime_error("Missing required conf:env in Redis");
    }
    return settings::Settings::from_map(m);
}

settings::UserParams Facade::load_user_params() {
    auto m = redis_tools::hgetall_map(_redis, "conf:user");
    if (m.empty()) {
        throw std::runtime_error("Missing conf:user in Redis");
    }
    return settings::UserParams::from_map(m);
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


} // namespace redis

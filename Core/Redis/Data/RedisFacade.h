#pragma once

#include "RedisTools.h"
#include <sw/redis++/redis++.h>
#include <optional>
#include <string>
#include <filesystem>

#include "Settings.h"
#include "UserParams.h"

namespace exoskeleton::redis {

class Facade {
public:
    explicit Facade(const std::string &uri = "tcp://127.0.0.1:6379");

    // Load conf:env → Settings (throws if required and empty)
    [[nodiscard]] exoskeleton::settings::Settings load_env(bool required = true);

    // Load conf:user → UserParams (throws if missing)
    exoskeleton::settings::UserParams load_user_params();
    
    // Save UserParams → conf:user and set conf:user:set flag
    void set_user_params(const exoskeleton::settings::UserParams& params);
    
    // Set individual user param field
    void set_user_param(const std::string& key, const std::string& value);

    // Get/reset conf:user:set flag
    bool user_params_changed(bool clear = false);

    // Get/reset dbchanged flag
    bool db_params_changed(bool clear = false);

    // GET currentuserid
    std::optional<int> current_user_id();

    void set_value(const std::string& key, const std::string& value);

    void flag_user_params_changed();

    void flag_db_params_changed();

    // Initialize conf:env from .env file if not exists or if overwrite=true
    bool initialize_env_from_file(const std::string& env_path = "../Core/Redis/.env", bool overwrite = false);

    // Initialize run:addrs from conf:env motor mappings
    void initialize_run_addrs_from_env();

    sw::redis::Redis _redis;
};

} // namespace exoskeleton::redis

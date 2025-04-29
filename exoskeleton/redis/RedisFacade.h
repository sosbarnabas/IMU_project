#pragma once

#include "RedisTools.h"
#include <sw/redis++/redis++.h>
#include <optional>
#include <string>

#include "../settings/Settings.h"
#include "../settings/UserParams.h"

namespace exoskeleton::redis {

class Facade {
public:
    explicit Facade(const std::string &uri = "tcp://127.0.0.1:6379");

    // Load conf:env → Settings (throws if required and empty)
    exoskeleton::settings::Settings load_env(bool required = true);

    // Load conf:user → UserParams (throws if missing)
    exoskeleton::settings::UserParams load_user_params();

    // Get/reset conf:user:set flag
    bool user_params_changed(bool clear = false);

    // Get/reset dbchanged flag
    bool db_params_changed(bool clear = false);

    // GET currentuserid
    std::optional<int> current_user_id();

private:
    sw::redis::Redis _redis;
};

} // namespace redis

#include "UserParams.h"
#include <stdexcept>

namespace exoskeleton::settings {

UserParams UserParams::from_map(const std::unordered_map<std::string,std::string> &m) {
    UserParams p;
    auto get_int = [&](const std::string &key){
        auto it = m.find(key);
        if (it == m.end()) throw std::runtime_error("Missing key in conf:user: " + key);
        return std::stoi(it->second);
    };
    auto get_str = [&](const std::string &key){
        auto it = m.find(key);
        if (it == m.end()) throw std::runtime_error("Missing key in conf:user: " + key);
        return it->second;
    };
    p.motorforce_min   = get_int("motorforce_min");
    p.motorforce_max   = get_int("motorforce_max");
    p.bodyweight       = get_int("bodyweight");
    p.upper_arm        = get_int("upper_arm");
    p.upper_arm_cuff   = get_int("upper_arm_cuff");
    p.forearm          = get_int("forearm");
    p.forearm_cuff     = get_int("forearm_cuff");
    p.cuff             = get_int("cuff");
    p.motorforce       = get_int("motorforce");
    p.assist           = get_int("assist");
    p.selected_task    = get_str("selected_task");
    p.user_id          = get_int("user_id");
    return p;
}

std::unordered_map<std::string,std::string> UserParams::to_map() const {
    return {
        {"motorforce_min", std::to_string(motorforce_min)},
        {"motorforce_max", std::to_string(motorforce_max)},
        {"bodyweight", std::to_string(bodyweight)},
        {"upper_arm", std::to_string(upper_arm)},
        {"upper_arm_cuff", std::to_string(upper_arm_cuff)},
        {"forearm", std::to_string(forearm)},
        {"forearm_cuff", std::to_string(forearm_cuff)},
        {"cuff", std::to_string(cuff)},
        {"motorforce", std::to_string(motorforce)},
        {"assist", std::to_string(assist)},
        {"selected_task", selected_task},
        {"user_id", std::to_string(user_id)}
    };
}

}

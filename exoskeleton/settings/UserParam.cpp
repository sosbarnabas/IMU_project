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

}

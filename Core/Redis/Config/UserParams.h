#pragma once

#include <string>
#include <unordered_map>

namespace exoskeleton::settings {

class UserParams {
public:
    int motorforce_min;
    int motorforce_max;
    int bodyweight;
    int upper_arm;
    int upper_arm_cuff;
    int forearm;
    int forearm_cuff;
    int cuff;
    int motorforce;
    int assist;
    std::string selected_task;
    int user_id;

    static UserParams from_map(const std::unordered_map<std::string,std::string> &m);
    std::unordered_map<std::string,std::string> to_map() const;
};

}

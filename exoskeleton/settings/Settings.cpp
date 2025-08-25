#include "Settings.h"
#include <stdexcept>

namespace exoskeleton::settings {

MotorProps::MotorProps(const std::string& name, const std::string& serial_number, int address)
    : name{name}, serial_number{serial_number}, address{address} {}


Settings Settings::from_map(const std::unordered_map<std::string,std::string> &m) {
    Settings s;
    auto get = [&](const std::string &key) -> std::string {
        auto it = m.find(key);
        if (it == m.end()) throw std::runtime_error("Missing key in conf:env: " + key);
        return it->second;
    };
    s.motor_e_flex    = get("motor_e_flex");
    s.motor_e_ext     = get("motor_e_ext");
    s.motor_s_flex    = get("motor_s_flex");
    s.motor_s_ext     = get("motor_s_ext");
    s.motor_s_add_pron= get("motor_s_add_pron");
    s.motor_s_abd     = get("motor_s_abd");
    s.motor_s_add_sup = get("motor_s_add_sup");
    s.multiport_motors= std::stoi(get("multiport_motors")) != 0;
    s.mock_motors     = std::stoi(get("mock_motors")) != 0;
    s.restapi_port    = std::stoi(get("restapi_port"));
    s.db_admin_pw     = get("db_admin_pw");
    return s;
}

std::vector<std::string> Settings::motor_ids() const {
    std::vector<std::string> ids;
    if (!motor_e_flex.empty())    ids.push_back(motor_e_flex);
    if (!motor_e_ext.empty())     ids.push_back(motor_e_ext);
    if (!motor_s_flex.empty())    ids.push_back(motor_s_flex);
    if (!motor_s_ext.empty())     ids.push_back(motor_s_ext);
    if (!motor_s_add_pron.empty())ids.push_back(motor_s_add_pron);
    if (!motor_s_abd.empty())     ids.push_back(motor_s_abd);
    if (!motor_s_add_sup.empty()) ids.push_back(motor_s_add_sup);
    return ids;
}

std::vector<MotorProps> Settings::motor_props() const {
    std::vector<MotorProps> props;
    size_t address = 0;
    if (!motor_e_flex.empty())    props.emplace_back("e_flex", motor_e_flex, address++);
    if (!motor_e_ext.empty())     props.emplace_back("e_ext", motor_e_ext, address++);
    if (!motor_s_flex.empty())    props.emplace_back("s_flex", motor_s_flex, address++);
    if (!motor_s_ext.empty())     props.emplace_back("s_ext", motor_s_ext, address++);
    if (!motor_s_add_pron.empty())props.emplace_back("s_add_pron", motor_s_add_pron, address++);
    if (!motor_s_abd.empty())     props.emplace_back("s_abd", motor_s_abd, address++);
    if (!motor_s_add_sup.empty()) props.emplace_back("s_add_sup", motor_s_add_sup, address++);
    return props;
}

}

// File: settings/UserParams.h
#pragma once

#include <string>
#include <unordered_map>

namespace settings {

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
};

}
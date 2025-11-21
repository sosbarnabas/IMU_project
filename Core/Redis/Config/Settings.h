#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace exoskeleton::settings {

struct MotorProps {
    std::string name;
    std::string serial_number;
    int address;

    MotorProps (const std::string&, const std::string&, int);
};

class Settings {
public:
    std::string motor_e_flex;
    std::string motor_e_ext;
    std::string motor_s_flex;
    std::string motor_s_ext;
    std::string motor_s_add_pron;
    std::string motor_s_abd;
    std::string motor_s_add_sup;
    bool multiport_motors;
    bool mock_motors;
    int restapi_port;
    std::string db_admin_pw;

    // Load from a map<string,string> (from Redis hgetall)
    static Settings from_map(const std::unordered_map<std::string,std::string> &m);

    // Helper to get motor IDs
    [[nodiscard]] auto motor_ids() const -> std::vector<std::string>;

    [[nodiscard]] auto motor_props() const -> std::vector<MotorProps>;
};

}

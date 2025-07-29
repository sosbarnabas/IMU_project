#include "ExoMotorsInterface.h"
#include <sstream>

namespace exoskeleton::core{

    SingleMotorData SingleMotorData::empty() {
        SingleMotorData data;
        data.enabled = false;
        data.slot_idx = 0;
        data.cmd_cntr = 0;
        data.position = 0;
        data.torque = 0;
        data.t = 0;
        data.n_tries = 0;
        return data;
    }

    std::string SingleMotorData::to_string() const {
        return std::to_string(enabled) + "," +
               std::to_string(slot_idx) + "," +
               std::to_string(cmd_cntr) + "," +
               std::to_string(position) + "," +
               std::to_string(torque) + "," +
               std::to_string(t) + "," +
               std::to_string(n_tries);
    }

    bool SingleMotorData::is_valid() {
            return !(slot_idx == 0 && cmd_cntr == 0 && position == 0 && torque == 0 && !enabled);

    }

    SingleMotorData SingleMotorData::from_string(const std::string& record) {
        SingleMotorData data;
        std::stringstream ss(record);
        std::string item;
        std::getline(ss, item, ','); data.enabled = std::stoi(item);
        std::getline(ss, item, ','); data.slot_idx = std::stoi(item);
        std::getline(ss, item, ','); data.cmd_cntr = std::stoi(item);
        std::getline(ss, item, ','); data.position = std::stoi(item);
        std::getline(ss, item, ','); data.torque = std::stoi(item);
        std::getline(ss, item, ','); data.t = std::stoull(item);
        std::getline(ss, item, ','); data.n_tries = std::stoi(item);
        return data;
    }
}
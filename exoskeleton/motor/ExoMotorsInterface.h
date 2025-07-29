#pragma once

#include <vector>
#include <string>
#include "motor.h"

namespace exoskeleton::core {

    struct SingleMotorData : public exoskeleton::motor::SingleMotorData {
        uint64_t t = 0;
        int n_tries = 0;
		 SingleMotorData()
        : exoskeleton::motor::SingleMotorData{false, 0, 0, 0, 0} {}
        bool is_valid();
        static SingleMotorData empty();
        std::string to_string() const;
        static SingleMotorData from_string(const std::string& record);
    };

    struct SerialStatus {
        bool connected;
        std::string name;
    };

    class ExoMotorsInterface {
    public:
        virtual ~ExoMotorsInterface() = default;

        virtual SerialStatus connect() = 0;
        virtual SerialStatus disconnect() = 0;
        virtual SerialStatus status() const = 0;

        virtual SingleMotorData enable(int address) = 0;
        virtual SingleMotorData raw_enable(int address) = 0;
        virtual SingleMotorData disable(int address) = 0;
        virtual SingleMotorData set_zero(int address) = 0;
        virtual SingleMotorData set_offset(int address, int position) = 0;

        virtual SingleMotorData upload_function(int address, int slot, const std::vector<int>& function) = 0;
        virtual SingleMotorData select_function(int address, int slot) = 0;
        virtual SingleMotorData set_function(int address, const std::vector<int>& function, int slot = 7) = 0;

        virtual std::vector<SingleMotorData> read() = 0;
        virtual std::vector<SingleMotorData> read_last() = 0;

        virtual int n_motors() const = 0;
    };
}
#pragma once

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include "motor.h"
#include <QSerialPort>

class MultiPortExoMotors {
public:
    MultiPortExoMotors(const std::vector<std::string>& ports, double timeout_sec = 3.0)
        : ports_(ports),
          timeout_ns_(static_cast<long long>(timeout_sec * 1e9)),
          last_connect_result_("NO_CONNECT") {}

    void connect();
    void disconnect();

    exoskeleton::motor::SingleMotorData enable(int address);
    exoskeleton::motor::SingleMotorData raw_enable(int address);
    exoskeleton::motor::SingleMotorData disable(int address);
    exoskeleton::motor::SingleMotorData set_zero(int address);
    exoskeleton::motor::SingleMotorData set_offset(int address, int position);
    exoskeleton::motor::SingleMotorData upload_function(int address, int slot, const std::vector<int>& function);
    exoskeleton::motor::SingleMotorData select_function(int address, int slot);

    std::vector<exoskeleton::motor::SingleMotorData> read();
    std::vector<exoskeleton::motor::SingleMotorData> read_last();

    int n_motors() const { return serials_.size(); }

private:
    std::vector<std::shared_ptr<QSerialPort>> serials_;
    std::vector<std::string> ports_;
    std::vector<exoskeleton::motor::SingleMotorData> prev_read_;
    std::vector<exoskeleton::motor::SingleMotorData> latest_full_read_;
    long long timeout_ns_;
    std::string last_connect_result_;

    void require_serial() const;
    exoskeleton::motor::SingleMotorData with_cntr_check(int address, std::function<void(QSerialPort*,int)> func);
    exoskeleton::motor::SingleMotorData with_cntr_check(int address, std::function<void(QSerialPort*,int,int)> func,int position);
    std::vector<exoskeleton::motor::SingleMotorData> internal_read(int max_tries = 1);
};

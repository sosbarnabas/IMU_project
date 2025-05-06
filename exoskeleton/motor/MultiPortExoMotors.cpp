#include "MultiPortExoMotors.h"
#include <stdexcept>
#include <chrono>
#include <thread>
#include <iostream>

void MultiPortExoMotors::connect() {
    serials_.clear();
    for (const auto& port : ports_) {
        auto serial = std::make_shared<QSerialPort>(QString::fromStdString(port));
        if (!serial->open(QIODevice::ReadWrite)) {
            last_connect_result_ = "CONNECT_OPEN_FAILED";
            throw std::runtime_error("Failed to open port: " + port);
        }
        serials_.push_back(serial);
    }
    last_connect_result_ = "CONNECT_SUCCESS";
}

void MultiPortExoMotors::disconnect() {
    for (auto& serial : serials_) {
        serial->close();
    }
    serials_.clear();
    latest_full_read_.clear();
    last_connect_result_ = "DISCONNECTED";
}

exoskeleton::motor::SingleMotorData MultiPortExoMotors::enable(int address) {
    require_serial();
    return with_cntr_check(address, exoskeleton::motor::motor_enable);
}

exoskeleton::motor::SingleMotorData MultiPortExoMotors::disable(int address) {
    require_serial();
    return with_cntr_check(address, exoskeleton::motor::motor_disable);
}

exoskeleton::motor::SingleMotorData MultiPortExoMotors::set_zero(int address) {
    require_serial();
    return with_cntr_check(address, exoskeleton::motor::motor_set_zero);
}

exoskeleton::motor::SingleMotorData MultiPortExoMotors::set_offset(int address, int position) {
    require_serial();
    return with_cntr_check(address, exoskeleton::motor::motor_set_offset, position);
}

std::vector<exoskeleton::motor::SingleMotorData> MultiPortExoMotors::read() {
    require_serial();
    latest_full_read_ = internal_read();
    return latest_full_read_;
}

std::vector<exoskeleton::motor::SingleMotorData> MultiPortExoMotors::read_last() {
    return latest_full_read_.empty() ? read() : latest_full_read_;
}

void MultiPortExoMotors::require_serial() const {
    if (serials_.empty()) {
        throw std::runtime_error("Serial connection not established: " + last_connect_result_);
    }
}

exoskeleton::motor::SingleMotorData MultiPortExoMotors::with_cntr_check(int address, std::function<void(QSerialPort*,int)> func) {
    auto serial = serials_.at(address).get();
    auto t0 = std::chrono::steady_clock::now();
    int n_tries = 1;

    while (true) {
        func(serial,address);  // hívás motor.cpp-ből

        auto data = exoskeleton::motor::read_data(serial, 3);
        auto t1 = std::chrono::steady_clock::now();
        auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

        if (data.has_value()) {
            auto prev_cntr = prev_read_.at(address).cmd_cntr;
            prev_read_.at(address) = data.value();
            if (prev_cntr != data.cmd_cntr) {
                return data.value();
            }
            if (dt > timeout_ns_) {
                throw std::runtime_error("Timeout in with_cntr_check");
            }
        }
        ++n_tries;
    }
}


exoskeleton::motor::SingleMotorData MultiPortExoMotors::with_cntr_check(int address, std::function<void(QSerialPort*,int,int)> func,int position) {
    auto serial = serials_.at(address).get();
    auto t0 = std::chrono::steady_clock::now();
    int n_tries = 1;
    int value = position;

    while (true) {
        func(serial,value,address);  // hívás motor.cpp-ből

        auto data = exoskeleton::motor::read_data(serial, 3);
        auto t1 = std::chrono::steady_clock::now();
        auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

        if (data.has_value()) {
            auto prev_cntr = prev_read_.at(address).cmd_cntr;
            prev_read_.at(address) = data.value();
            if (prev_cntr != data.cmd_cntr) {
                return data.value();
            }
            if (dt > timeout_ns_) {
                throw std::runtime_error("Timeout in with_cntr_check");
            }
        }
        ++n_tries;
    }
}


std::vector<exoskeleton::motor::SingleMotorData> MultiPortExoMotors::internal_read(int max_tries) {
    std::vector<exoskeleton::motor::SingleMotorData> result;
    for (size_t i = 0; i < serials_.size(); ++i) {
        auto data = exoskeleton::motor::read_data(serials_[i].get(), max_tries);
        if (data.has_value()) {
            result.push_back(data.value());
        } else {
            result.push_back(exoskeleton::motor::SingleMotorData::empty());
        }
    }
    return result;
}

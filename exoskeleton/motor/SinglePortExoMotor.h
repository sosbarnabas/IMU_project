// SinglePortExoMotor.h

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <QSerialPort>

#include "motor.h"
#include "ExoMotorsInterface.h"

using namespace exoskeleton::core;

SingleMotorData from_base(const exoskeleton::motor::SingleMotorData& base, uint64_t t, int tries);

class SinglePortExoMotor {
public:
    explicit SinglePortExoMotor(const std::string& port, double timeout_sec = 3.0);

    // Soros kapcsolat vezérlés
    SerialStatus connect();
    SerialStatus disconnect();
    SerialStatus status() const;

    // Alapműveletek
    SingleMotorData enable();
    SingleMotorData raw_enable();
    SingleMotorData disable();
    SingleMotorData set_zero();
    SingleMotorData set_offset(int position);

    // Funkciók
    SingleMotorData upload_function(int slot , const std::vector<int>& function);
    SingleMotorData select_function(int slot);
    SingleMotorData set_function(const std::vector<int>& function, int slot = 7);

    // Olvasás
    SingleMotorData read(int max_tries = 3);
    SingleMotorData read_last() const;

private:
    std::string port_;
    QSerialPort* serial_ = nullptr;
    SingleMotorData prev_read_;
    int64_t timeout_ns_;
    std::string last_connect_result_ = "NOT_CONNECTED";

    void require_serial() const;

    // Belső újrapróbálás vezérléssel
    SingleMotorData with_cntr_check(std::function<void(QSerialPort*, int)> func);
    SingleMotorData with_cntr_check(std::function<void(QSerialPort*, int, int)> func, int value);
};
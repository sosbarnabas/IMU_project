// SinglePortExoMotor.h

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <QSerialPort>

#include "../motor/motor.h"
#include "../motor/ExoMotorsInterface.h"

namespace exoskeleton::core {

class SinglePortExoMotor {
public:
    explicit SinglePortExoMotor(const std::string& serial_number, double timeout_sec = 3.0);

    // Soros kapcsolat vezérlés
    SerialStatus connect();
    SerialStatus disconnect();
    [[nodiscard]] SerialStatus status() const;

    // Alapműveletek
    SingleMotorData enable();
    SingleMotorData raw_enable();
    SingleMotorData disable();
    SingleMotorData set_zero();
    SingleMotorData set_offset(int position);

    // Funkciók
    SingleMotorData upload_function(int slot, const std::vector<int>& function);
    SingleMotorData select_function(int slot);
    SingleMotorData set_function(const std::vector<int>& function, int slot = 7);

    // Olvasás
    [[nodiscard]] SingleMotorData read(int max_tries = 3);
    [[nodiscard]] SingleMotorData read_last();

private:
    std::string port_;
    std::string serial_number_;
    std::unique_ptr<QSerialPort> serial_;
    SingleMotorData prev_read_;
    int64_t timeout_ns_;
    std::string last_connect_result_ = "NOT_CONNECTED";

    void require_serial() const;

    // Belső újrapróbálás vezérléssel
    SingleMotorData with_cntr_check(const std::function<void(QSerialPort&, int)>& func);
    SingleMotorData with_cntr_check(const std::function<void(QSerialPort&, int, int)>& func, int value);
};

} // exoskeleton::core
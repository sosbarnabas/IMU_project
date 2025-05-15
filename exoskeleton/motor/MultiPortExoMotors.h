// MultiPortExoMotors.h

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <functional>
#include <QSerialPort>

#include "motor.h"
#include "ExoMotorsInterface.h"



SingleMotorData from_base(const exoskeleton::motor::SingleMotorData& base, uint64_t t, int tries);

class MultiPortExoMotors : public ExoMotorsInterface {
public:
    MultiPortExoMotors(const std::vector<std::string>& ports, double timeout_sec = 3.0);

    SerialStatus connect() override;
    SerialStatus disconnect() override;
    SerialStatus status() const override;

    SingleMotorData enable(int address) override;
    SingleMotorData raw_enable(int address) override;
    SingleMotorData disable(int address) override;
    SingleMotorData set_zero(int address) override;
    SingleMotorData set_offset(int address, int position) override;

    SingleMotorData upload_function(int address, int slot, const std::vector<int>& function) override;
    SingleMotorData select_function(int address, int slot) override;
    SingleMotorData set_function(int address, const std::vector<int>& function, int slot = 7) override;

    std::vector<SingleMotorData> read() override;
    std::vector<SingleMotorData> read_last() override;

    int n_motors() const override;

private:
    std::vector<std::string> ports_;
    std::vector<std::shared_ptr<QSerialPort>> serials_;
    std::vector<SingleMotorData> prev_read_;
    std::vector<SingleMotorData> latest_full_read_;
    int64_t timeout_ns_;
    std::string last_connect_result_;

    void require_serial() const;
    std::string join_ports() const;
    std::vector<SingleMotorData> internal_read(int max_tries = 1);

    SingleMotorData with_cntr_check(int address, std::function<void(QSerialPort*, int)> func);
    SingleMotorData with_cntr_check(int address, std::function<void(QSerialPort*, int, int)> func, int value);
};


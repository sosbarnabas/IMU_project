// SinglePortExoMotor.cpp
#include "SinglePortExoMotor.h"
#include <sstream>
#include <stdexcept>

using namespace exoskeleton::core;

SingleMotorData from_base(const exoskeleton::motor::SingleMotorData& base, uint64_t t, int tries) {
    SingleMotorData d;
    d.enabled = base.enabled;
    d.slot_idx = base.slot_idx;
    d.cmd_cntr = base.cmd_cntr;
    d.position = base.position;
    d.torque = base.torque;
    d.t = t;
    d.n_tries = tries;
    return d;
}

SinglePortExoMotor::SinglePortExoMotor(const std::string& port, double timeout_sec)
    : port_(port), timeout_ns_(static_cast<int64_t>(timeout_sec * 1e9)) {}

SerialStatus SinglePortExoMotor::connect() {
    serial_ = new QSerialPort(QString::fromStdString(port_));
    serial_->setBaudRate(1000000);
    serial_->setDataBits(QSerialPort::Data8);
    serial_->setParity(QSerialPort::NoParity);
    serial_->setStopBits(QSerialPort::OneStop);

    if (!serial_->open(QIODevice::ReadWrite)) {
        last_connect_result_ = "CONNECT_OPEN_FAILED";
        throw std::runtime_error("Failed to open port: " + port_);
    }

    auto data = exoskeleton::motor::read_data(serial_, 10);
    if (!data.has_value()) {
        last_connect_result_ = "CONNECT_READ_FAILED";
        throw std::runtime_error("Initial read failed");
    }

    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    prev_read_ = from_base(data, now, 0);
    last_connect_result_ = "CONNECT_SUCCESS";
    return status();
}

SerialStatus SinglePortExoMotor::disconnect() {
    if (serial_) serial_->close();
    delete serial_;
    serial_ = nullptr;
    last_connect_result_ = "DISCONNECTED";
    return status();
}

SerialStatus SinglePortExoMotor::status() const {
    bool connected = serial_ && serial_->isOpen();
    return SerialStatus{connected, port_};
}

void SinglePortExoMotor::require_serial() const {
    if (!serial_ || !serial_->isOpen()) {
        throw std::runtime_error("Serial connection is off: " + last_connect_result_);
    }
}

SingleMotorData SinglePortExoMotor::with_cntr_check(std::function<void(QSerialPort*, int)> func) {
    require_serial();
    auto t0 = std::chrono::steady_clock::now();
    int n_tries = 1;

    while (true) {
        func(serial_, 0);
        auto data = exoskeleton::motor::read_data(serial_, 3);
        if (data.has_value()) {
            auto prev_cntr = prev_read_.cmd_cntr;
            auto now = std::chrono::steady_clock::now().time_since_epoch().count();
            prev_read_ = from_base(data, now, n_tries);
            if (prev_cntr != data.cmd_cntr) return prev_read_;
        }
        if (std::chrono::steady_clock::now() - t0 > std::chrono::nanoseconds(timeout_ns_)) {
            throw std::runtime_error("Timeout in with_cntr_check");
        }
        ++n_tries;
    }
}

SingleMotorData SinglePortExoMotor::with_cntr_check(std::function<void(QSerialPort*, int, int)> func, int value) {
    require_serial();
    auto t0 = std::chrono::steady_clock::now();
    int n_tries = 1;

    while (true) {
        func(serial_, value, 0);
        auto data = exoskeleton::motor::read_data(serial_, 3);
        if (data.has_value()) {
            auto prev_cntr = prev_read_.cmd_cntr;
            auto now = std::chrono::steady_clock::now().time_since_epoch().count();
            prev_read_ = from_base(data, now, n_tries);
            if (prev_cntr != data.cmd_cntr) return prev_read_;
        }
        if (std::chrono::steady_clock::now() - t0 > std::chrono::nanoseconds(timeout_ns_)) {
            throw std::runtime_error("Timeout in with_cntr_check");
        }
        ++n_tries;
    }
}

SingleMotorData SinglePortExoMotor::enable() {
    require_serial();
    std::vector<int> weak_func(360);
    for (int i = 0; i < 360; ++i) weak_func[i] = 10 - (20 * i) / 359;
    set_function(weak_func, 7);
    set_zero();
    raw_enable();
    return set_offset(-prev_read_.position);
}

SingleMotorData SinglePortExoMotor::raw_enable() {
    return with_cntr_check(exoskeleton::motor::motor_enable);
}

SingleMotorData SinglePortExoMotor::disable() {
    return with_cntr_check(exoskeleton::motor::motor_disable);
}

SingleMotorData SinglePortExoMotor::set_zero() {
    return with_cntr_check(exoskeleton::motor::motor_set_zero);
}

SingleMotorData SinglePortExoMotor::set_offset(int position) {
    return with_cntr_check(exoskeleton::motor::motor_set_offset, position);
}

SingleMotorData SinglePortExoMotor::upload_function(int slot, const std::vector<int>& function) {
    require_serial();
    auto t0 = std::chrono::steady_clock::now();
    int n_tries = 1;

    while (true) {
        exoskeleton::motor::motor_set_slot_function(serial_, slot, function);
        auto data = exoskeleton::motor::read_data(serial_, 3);
        if (data.has_value()) {
            auto prev_cntr = prev_read_.cmd_cntr;
            prev_read_ = from_base(data, std::chrono::steady_clock::now().time_since_epoch().count(), n_tries);
            if (prev_cntr != data.cmd_cntr) return prev_read_;
        }
        if (std::chrono::steady_clock::now() - t0 > std::chrono::nanoseconds(timeout_ns_)) {
            throw std::runtime_error("Timeout in upload_function");
        }
        ++n_tries;
    }
}

SingleMotorData SinglePortExoMotor::select_function(int slot) {
    require_serial();
    auto t0 = std::chrono::steady_clock::now();
    int n_tries = 1;

    while (true) {
        exoskeleton::motor::motor_select_slot(serial_, slot);
        auto data = exoskeleton::motor::read_data(serial_, 3);
        if (data.has_value()) {
            auto prev_cntr = prev_read_.cmd_cntr;
            prev_read_ = from_base(data, std::chrono::steady_clock::now().time_since_epoch().count(), n_tries);
            if (prev_cntr != data.cmd_cntr) return prev_read_;
        }
        if (std::chrono::steady_clock::now() - t0 > std::chrono::nanoseconds(timeout_ns_)) {
            throw std::runtime_error("Timeout in select_function");
        }
        ++n_tries;
    }
}

SingleMotorData SinglePortExoMotor::set_function(const std::vector<int>& function, int slot) {
    upload_function(slot, function);
    return select_function(slot);
}

SingleMotorData SinglePortExoMotor::read(int max_tries) {
    require_serial();
    auto data = exoskeleton::motor::read_data(serial_, max_tries);
    if (data.has_value()) {
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        prev_read_ = from_base(data, now, 1);
        return prev_read_;
    }
    return SingleMotorData::empty();
}

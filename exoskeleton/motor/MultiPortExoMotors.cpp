// MultiPortExoMotors.cpp
#include "MultiPortExoMotors.h"
#include <sstream>
#include <stdexcept>

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
SingleMotorData MultiPortExoMotors::raw_enable(int address) {
    require_serial();
    return with_cntr_check(address, exoskeleton::motor::motor_enable);
}

SingleMotorData MultiPortExoMotors::enable(int address) {
    require_serial();
    std::vector<int> weak_func(360);
    for (int i = 0; i < 360; ++i) weak_func[i] = 10 - (20 * i) / 359;
    set_function(address, weak_func);
    set_zero(address);
    raw_enable(address);
    auto data = read();
    int pos = data[address].position;
    return set_offset(address, -pos);
}

SingleMotorData MultiPortExoMotors::disable(int address) {
    require_serial();
    return with_cntr_check(address, exoskeleton::motor::motor_disable);
}

SingleMotorData MultiPortExoMotors::set_zero(int address) {
    require_serial();
    return with_cntr_check(address, exoskeleton::motor::motor_set_zero);
}

SingleMotorData MultiPortExoMotors::set_offset(int address, int position) {
    require_serial();
    return with_cntr_check(address, exoskeleton::motor::motor_set_offset, position);
}

SingleMotorData MultiPortExoMotors::upload_function(int address, int slot, const std::vector<int>& function) {
    require_serial();
    auto serial = serials_.at(address);
    auto t0 = std::chrono::steady_clock::now();
    int n_tries = 1;
    while (true) {
        exoskeleton::motor::motor_set_slot_function(serial, slot, function);
        auto data = exoskeleton::motor::read_data(serial, 3);
        if (data.has_value()) {
            if (prev_read_.size() <= static_cast<size_t>(address)) prev_read_.resize(address + 1);
            auto prev_cntr = prev_read_[address].cmd_cntr;
            prev_read_[address] = from_base(data, std::chrono::steady_clock::now().time_since_epoch().count(), n_tries);
            if (prev_cntr != data.cmd_cntr) {
                return prev_read_[address];
            }
        }
        if (std::chrono::steady_clock::now() - t0 > std::chrono::nanoseconds(timeout_ns_)) {
            throw std::runtime_error("Timeout in upload_function");
        }
        ++n_tries;
    }
}
SingleMotorData MultiPortExoMotors::select_function(int address, int slot) {
    require_serial();
    auto serial = serials_.at(address);
    auto t0 = std::chrono::steady_clock::now();
    int n_tries = 1;
    while (true) {
        exoskeleton::motor::motor_select_slot(serial, slot);
        auto data = exoskeleton::motor::read_data(serial, 3);
        if (data.has_value()) {
            if (prev_read_.size() <= static_cast<size_t>(address)) prev_read_.resize(address + 1);
            auto prev_cntr = prev_read_[address].cmd_cntr;
            prev_read_[address] = from_base(data, std::chrono::steady_clock::now().time_since_epoch().count(), n_tries);
            if (prev_cntr != data.cmd_cntr) {
                return prev_read_[address];
            }
        }
        if (std::chrono::steady_clock::now() - t0 > std::chrono::nanoseconds(timeout_ns_)) {
            throw std::runtime_error("Timeout in select_function");
        }
        ++n_tries;
    }
}


SingleMotorData MultiPortExoMotors::set_function(int address, const std::vector<int>& function, int slot) {
    upload_function(address, slot, function);
    return select_function(address, slot);
}

std::vector<SingleMotorData> MultiPortExoMotors::read() {
    require_serial();
    latest_full_read_ = internal_read();
    return latest_full_read_;
}

std::vector<SingleMotorData> MultiPortExoMotors::read_last() {
    return latest_full_read_.empty() ? read() : latest_full_read_;
}

int MultiPortExoMotors::n_motors() const {
    return static_cast<int>(serials_.size());
}

MultiPortExoMotors::MultiPortExoMotors(const std::vector<std::string>& ports, double timeout_sec)
    : ports_(ports), timeout_ns_(static_cast<int64_t>(timeout_sec * 1e9)) {}


SerialStatus MultiPortExoMotors::connect() {
    serials_.clear();
    for (const auto& port : ports_) {
        QSerialPort* serial = new QSerialPort("COM3");
        serial->setBaudRate(1000000);
        serial->setDataBits(QSerialPort::Data8);
        serial->setParity(QSerialPort::NoParity);
        serial->setStopBits(QSerialPort::OneStop);

        if (!serial->open(QIODevice::ReadWrite)) {
            last_connect_result_ = "CONNECT_OPEN_FAILED";
            throw std::runtime_error("Failed to open port: " + port);
        }
        serials_.push_back(serial);
    }
    for (auto& s : serials_) {
        auto data = exoskeleton::motor::read_data(s, 10);
        if (!data.has_value()) {
            last_connect_result_ = "CONNECT_READ_FAILED";
            throw std::runtime_error("Initial read failed");
        }
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        prev_read_.push_back(from_base(data, now, 0));
    }
    last_connect_result_ = "CONNECT_SUCCESS";
    return status();
}

SerialStatus MultiPortExoMotors::disconnect() {
    for (auto& s : serials_) s->close();
    serials_.clear();
    latest_full_read_.clear();
    last_connect_result_ = "DISCONNECTED";
    return status();
}

SerialStatus MultiPortExoMotors::status() const {
    bool connected = !serials_.empty();
    for (const auto& s : serials_) {
        if (!s->isOpen()) connected = false;
    }
    return SerialStatus{connected, join_ports()};
}

std::string MultiPortExoMotors::join_ports() const {
    std::string result;
    for (size_t i = 0; i < ports_.size(); ++i) {
        result += ports_[i];
        if (i + 1 < ports_.size()) result += ",";
    }
    return result;
}

void MultiPortExoMotors::require_serial() const {
    if (serials_.empty()) {
        throw std::runtime_error("Serial connection is off: " + last_connect_result_);
    }
}

SingleMotorData MultiPortExoMotors::with_cntr_check(int address, std::function<void(QSerialPort*, int)> func) {
    auto serial = serials_.at(address);
    auto t0 = std::chrono::steady_clock::now();
    int n_tries = 1;
    while (true) {
        func(serial, address);
        auto data = exoskeleton::motor::read_data(serial, 3);
        if (data.has_value()) {
            auto prev_cntr = prev_read_[address].cmd_cntr;
            auto now = std::chrono::steady_clock::now().time_since_epoch().count();
            prev_read_[address] = from_base(data, now, n_tries);

            if (prev_cntr != data.cmd_cntr) {
                auto now = std::chrono::steady_clock::now().time_since_epoch().count();
                return from_base(data, now, n_tries);
            }
            if (std::chrono::steady_clock::now() - t0 > std::chrono::nanoseconds(timeout_ns_)) {
                throw std::runtime_error("Timeout in with_cntr_check");
            }
        }
        ++n_tries;
    }
}

SingleMotorData MultiPortExoMotors::with_cntr_check(int address, std::function<void(QSerialPort*, int, int)> func, int value) {
    auto serial = serials_.at(address);
    auto t0 = std::chrono::steady_clock::now();
    int n_tries = 1;
    while (true) {
        func(serial, value, address);
        auto data = exoskeleton::motor::read_data(serial, 3);
        if (data.has_value()) {
            auto prev_cntr = prev_read_[address].cmd_cntr;
            auto now = std::chrono::steady_clock::now().time_since_epoch().count();
            prev_read_[address] = from_base(data, now, n_tries);

            if (prev_cntr != data.cmd_cntr) {
                auto now = std::chrono::steady_clock::now().time_since_epoch().count();
                return from_base(data, now, n_tries);
            }
            if (std::chrono::steady_clock::now() - t0 > std::chrono::nanoseconds(timeout_ns_)) {
                throw std::runtime_error("Timeout in with_cntr_check");
            }
        }
        ++n_tries;
    }
}

std::vector<SingleMotorData> MultiPortExoMotors::internal_read(int max_tries) {
    std::vector<SingleMotorData> result;
    for (size_t i = 0; i < serials_.size(); ++i) {
        auto data = exoskeleton::motor::read_data(serials_[i], max_tries);
        if (data.has_value()) {
            auto now = std::chrono::steady_clock::now().time_since_epoch().count();
            prev_read_.push_back(from_base(data, now, 1));
            result.push_back(from_base(data, now, 1));
        } else {
            result.push_back(SingleMotorData::empty());
        }
    }
    return result;
}





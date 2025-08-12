#include "SinglePortExoMotor.h"
#include "MultiPortExoMotors.h"
#include <stdexcept>

namespace exoskeleton::core {

SinglePortExoMotor::SinglePortExoMotor(const std::string& serial_number, double timeout_sec)
    : serial_number_{serial_number}
    , serial_{std::make_unique<QSerialPort>()}
    , timeout_ns_{static_cast<int64_t>(timeout_sec * 1e9)} {}

SerialStatus SinglePortExoMotor::connect() {
    disconnect();

    port_ = exoskeleton::motor::find_port_name_by_serial_num(serial_number_);
    try {
        serial_ = exoskeleton::motor::open_serial_port(port_);
    } catch (exoskeleton::motor::CannotOpenSerialPort const&) {
        last_connect_result_ = "CONNECT_OPEN_FAILED";
        throw;
    }

    const auto data = exoskeleton::motor::read_data(*serial_, 10);
    if (!data.has_value()) {
        last_connect_result_ = "CONNECT_READ_FAILED";
        throw std::runtime_error("Initial read failed");
    }

    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    prev_read_ = from_base(data, now, 0);
    last_connect_result_ = "CONNECT_SUCCESS";
    return status();
}

SerialStatus SinglePortExoMotor::disconnect() {
    if (serial_) {
        serial_->close();
    }
    serial_.reset();
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

SingleMotorData SinglePortExoMotor::with_cntr_check(const std::function<void(QSerialPort&, int)>& func) {
    require_serial();
    auto t0 = std::chrono::steady_clock::now();
    int n_tries = 1;

    while (true) {
        func(*serial_, 0);
        auto data = exoskeleton::motor::read_data(*serial_, 3);
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

SingleMotorData SinglePortExoMotor::with_cntr_check(const std::function<void(QSerialPort&, int, int)>& func, int value) {
    require_serial();
    auto t0 = std::chrono::steady_clock::now();
    int n_tries = 1;

    while (true) {
        func(*serial_, value, 0);
        auto data = exoskeleton::motor::read_data(*serial_, 3);
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
    const auto data = read();
    return set_offset(-data.position);
}

SingleMotorData SinglePortExoMotor::raw_enable() {
    require_serial();
    return with_cntr_check(exoskeleton::motor::motor_enable);
}

SingleMotorData SinglePortExoMotor::disable() {
    require_serial();
    return with_cntr_check(exoskeleton::motor::motor_disable);
}

SingleMotorData SinglePortExoMotor::set_zero() {
    require_serial();
    return with_cntr_check(exoskeleton::motor::motor_set_zero);
}

SingleMotorData SinglePortExoMotor::set_offset(int position) {
    require_serial();
    return with_cntr_check(exoskeleton::motor::motor_set_offset, position);
}

SingleMotorData SinglePortExoMotor::upload_function(int slot, const std::vector<int>& function) {
    require_serial();
    auto t0 = std::chrono::steady_clock::now();
    int n_tries = 1;

    while (true) {
        exoskeleton::motor::motor_set_slot_function(*serial_, slot, function);
        auto data = exoskeleton::motor::read_data(*serial_, 3);
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
    return with_cntr_check([](QSerialPort& s, int value, int) { return exoskeleton::motor::motor_select_slot(s, value); }, slot);
}

SingleMotorData SinglePortExoMotor::set_function(const std::vector<int>& function, int slot) {
    upload_function(slot, function);
    return select_function(slot);
}

SingleMotorData SinglePortExoMotor::read(int max_tries) {
    require_serial();
    auto data = exoskeleton::motor::read_data(*serial_, max_tries);
    if (data.has_value()) {
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        prev_read_ = from_base(data, now, 1);
        return prev_read_;
    }
    return SingleMotorData::empty();
}

SingleMotorData SinglePortExoMotor::read_last() {
    return prev_read_.t == 0 ? read() : prev_read_;
}

} // exoskeleton::core
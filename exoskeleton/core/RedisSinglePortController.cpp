#include "../core/RedisSinglePortController.h"
#include "RedisTools.h"
#include <sw/redis++/redis++.h>
#include <chrono>
#include <sstream>
#include <vector>
#include <iostream>

namespace exoskeleton::core {

RedisSinglePortController::RedisSinglePortController(const settings::MotorProps& motor_props, size_t n_motors)
    : motor_props_{motor_props}
    , redis_{"tcp://127.0.0.1:6379"}
    , motor_{motor_props.serial_number}
    , n_motors_{n_motors}
    {}

void RedisSinglePortController::loop() {
    motor_.connect();

    auto subscriber = exoskeleton::redis_tools::make_keyspace_subscriber(redis_,"sync:loop:next");

    subscriber.on_message([this](const std::string& channel, const std::string& msg) {
       // std::cout << "[DEBUG] sync:loop:next triggerelt: " << msg << std::endl;
        if (msg != "set") return;

        auto raw_command = redis_.rpop("command:" + std::to_string(motor_props_.address));

        if (raw_command) {
            processCommand(*raw_command);
        } else {
            measureAndStore();
            exoskeleton::redis_tools::signal_data_ready(redis_,n_motors_);
            //std::cout << "[DEBUG] Nincs parancs " << data.size() << std::endl;
        }
    });

    while (true) {
        try {
            if (auto const exit_flag = redis_.get("exit");
                exit_flag && *exit_flag == "1") {
                motor_.disable();  // TODO emergency_stop
                std::cout << "Exited by \"exit\" command" << std::endl;
                break;
                }

            if (auto const stop_flag = redis_.getset("stop:" + std::to_string(motor_props_.address), "0");
                stop_flag && *stop_flag == "1") {
                std::cout << "stop" << std::endl;
                auto const data = motor_.disable();  // TODO emergency_stop
                exoskeleton::redis_tools::send_ok(
                    redis_,
                    redis_tools::COMMAND_RESULT_KEY
                    + ":stop:" + std::to_string(motor_props_.address),
                    data.to_string()
                );
                continue;
                }
            subscriber.consume();  // ez figyeli az üzeneteket
        } catch (std::exception const& e) {
            std::cerr << e.what() << '\n';
        } catch (...) {
            std::cerr << "Unknown exception at " << __func__ << "\n";
        }
    }
}

void RedisSinglePortController::processCommand(const std::string &raw_command) {
    std::vector<std::string> parts;
    std::stringstream ss(raw_command);
    std::string tok;
    while (std::getline(ss, tok, '|')) parts.push_back(tok);
    if (parts.size() < 3) {
        std::cerr << "Invalid command: " << raw_command << std::endl;
        return;
    }
    const auto &t = parts[0];
    const auto &c = parts[1];
    int idx = std::stoi(parts[2]);
    bool partial = (idx == -1);

    std::cerr << t << " " << c << " " << idx << std::endl;

    std::string key{partial ? redis_tools::COMMAND_PARTIAL_RESULT_KEY : redis_tools::COMMAND_RESULT_KEY};
    key += ":";
    key += c;
    key += ":";
    key += std::to_string(motor_props_.address);
    key += ":";
    key += t;
    try {
        std::string response;
        if (c == "connect") {
            auto const record = motor_.connect();
            response = record.to_string();
        }
        else if (c == "disconnect") {
            auto const record = motor_.disconnect();
            response = record.to_string();
        }
        else if (c == "status") {
            auto const record = motor_.status();
            response = record.to_string();
        }
        else if (c == "enable") {
            auto const record = motor_.enable();
            response = record.to_string();
        }
        else if (c == "disable") {
            auto const record = motor_.disable();
            response = record.to_string();
        }
        else if (c == "read") {
            auto const data = measureAndStore();
            response = data.to_string();
        }
        else if (c == "fn_upload") {
            if (parts.size() < 4) {
                throw std::runtime_error("Missing value for fn_upload");
            }
            std::string const& json_str = parts[3];
            if (auto values = redis_tools::parseJsonArray(json_str); !values.empty()) {
                int slot = values.front();
                values.erase(values.begin());

                auto record = motor_.upload_function(slot, values);
                response = record.to_string();
            }
        }
        else if (c=="fn_select") {
            int slot = std::stoi(parts[3]);
            auto const record = motor_.select_function(slot);
            response = record.to_string();
        }
        else if (c == "zero") {
            auto const record = motor_.set_zero();
            response = record.to_string();
        }
        else if (c == "function") {
            std::string const& json_str = parts[3];
            if (auto values = redis_tools::parseJsonArray(json_str); !values.empty()) {
                auto const record = motor_.set_function(values, 7);
                response = record.to_string();
            }
        }
        // TODO offset
        // TODO fn_get
        else {
            std::cerr << "Unknown command: " << c << std::endl;
        }

        exoskeleton::redis_tools::send_ok(redis_, key, response);
    }
    catch (const std::exception &e) {
        std::cerr << "[DEBUG] Exception at: " << idx << std::endl;
        exoskeleton::redis_tools::send_error(redis_, key, e.what());
    }
}

auto RedisSinglePortController::measureAndStore() -> SingleMotorData {
    auto const data = motor_.read();
    if (data.is_valid()) {
        exoskeleton::redis_tools::xadd_motor_data(redis_, motor_props_.address, data);
    }
    return data;
}

} // exoskeleton::core

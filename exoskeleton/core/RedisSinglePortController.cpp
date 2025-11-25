#include "../core/RedisSinglePortController.h"
#include "RedisTools.h"
#include <sw/redis++/redis++.h>
#include <chrono>
#include <sstream>
#include <vector>
#include <iostream>

namespace exoskeleton::core {
    RedisSinglePortController::RedisSinglePortController(const settings::MotorProps &motor_props, size_t n_motors)
        : motor_props_{motor_props}, redis_{"tcp://127.0.0.1:6379"}, motor_{motor_props.serial_number},
          n_motors_{n_motors}, data_logger_{std::make_unique<DataLogger>(redis_, "../exoskeleton_data_log.csv")} {
    }

    void RedisSinglePortController::loop() {
        motor_.connect();
        const auto address_str = std::to_string(motor_props_.address);

        auto subscriber = exoskeleton::redis_tools::make_keyspace_subscriber(redis_, "sync:loop:next");

        subscriber.on_message([this, &address_str](const std::string &channel, const std::string &msg) {
            if (msg != "set") return;

            auto raw_command = redis_.rpop("command:" + address_str);

            if (raw_command) {
                processCommand(*raw_command);
            } else {
                measureAndStore();
                exoskeleton::redis_tools::signal_data_ready(redis_, n_motors_);
                //std::cout << "[DEBUG] Nincs parancs " << data.size() << std::endl;
            }
        });

        while (true) {
            try {
                if (auto const exit_flag = redis_.get("exit");
                    exit_flag && *exit_flag == "1") {
                    motor_.disable(); // TODO emergency_stop
                    redis_tools::log(redis_, motor_props_.name, "Exited by \"exit\" command");
                    break;
                }

                if (auto const stop_flag = redis_.getset("stop:" + address_str, "0");
                    stop_flag && *stop_flag == "1") {
                    redis_tools::log(redis_, motor_props_.name, "stop");
                    auto const data = motor_.disable(); // TODO emergency_stop
                    exoskeleton::redis_tools::send_ok(
                        redis_,
                        redis_tools::COMMAND_RESULT_KEY + ":stop:" + address_str,
                        data.to_string());
                    continue;
                }
                subscriber.consume(); // ez figyeli az üzeneteket
            } catch (std::exception const &e) {
                redis_tools::log(redis_, motor_props_.name, e.what(), redis_tools::LogLevel::error);
            }
            catch (...) {
                redis_tools::log(redis_, motor_props_.name, "Unknown exception at " + std::string{__func__},
                                 redis_tools::LogLevel::error);
            }
        }
    }

    void RedisSinglePortController::processCommand(const std::string &raw_command) {
        const auto address_str = std::to_string(motor_props_.address);

        std::vector<std::string> parts;
        std::stringstream ss(raw_command);
        std::string tok;
        while (std::getline(ss, tok, '|'))
            parts.push_back(tok);
        if (parts.size() < 3) {
            redis_tools::log(redis_, motor_props_.name, "Invalid command: " + raw_command,
                             redis_tools::LogLevel::error);
            return;
        }
        const auto &t = parts[0];
        const auto &c = parts[1];
        int idx = std::stoi(parts[2]);
        bool partial = (idx == -1);

        redis_tools::log(redis_, motor_props_.name, raw_command);

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
            } else if (c == "disconnect") {
                auto const record = motor_.disconnect();
                response = record.to_string();
            } else if (c == "status") {
                auto const record = motor_.status();
                response = record.to_string();
            } else if (c == "enable") {
                auto const record = motor_.enable();
                response = record.to_string();
            } else if (c == "disable") {
                auto const record = motor_.disable();
                response = record.to_string();
            } else if (c == "read") {
                auto const data = measureAndStore();
                response = data.to_string();
            } else if (c == "fn_upload") {
                if (parts.size() < 4) {
                    throw std::runtime_error("Missing value for fn_upload");
                }
                std::string const &json_str = parts[3];
                if (auto values = redis_tools::parseJsonArray(json_str); !values.empty()) {
                    int slot = values.front();
                    values.erase(values.begin());

                    auto record = motor_.upload_function(slot, values);
                    response = record.to_string();
                }
            } else if (c == "fn_select") {
                int slot = std::stoi(parts[3]);
                auto const record = motor_.select_function(slot);
                response = record.to_string();
            } else if (c == "zero") {
                auto const record = motor_.set_zero();
                response = record.to_string();
            } else if (c == "offset") {
                int value = std::stoi(parts[3]);
                auto const record = motor_.set_offset(value);
                response = record.to_string();
            } else if (c == "function") {
                std::string const &json_str = parts[3];
                if (auto values = redis_tools::parseJsonArray(json_str); !values.empty()) {
                    auto const record = motor_.set_function(values, 7);
                    response = record.to_string();
                }
            } else if (c == "fn_get") {
                response = "";
                bool first = true;
                for (const auto &[slot, fn]: motor_.get_functions()) {
                    if (first) {
                        first = false;
                    } else {
                        response += "|";
                    }
                    auto values = decltype(fn){{slot}};
                    values.insert(values.end(), fn.begin(), fn.end());
                    response += redis_tools::jsonArray(values);
                }
            } else if (c == "startlogging") {
                data_logger_->startLogging();
                response = "OK:logging_started";
            } else if (c == "stoplogging") {
                size_t records = data_logger_->stopLogging();
                response = "OK:logging_stopped_" + std::to_string(records) + "_records";
            } else {
                redis_tools::log(redis_, motor_props_.name, "Unknown command: " + c, redis_tools::LogLevel::error);
            }

            redis_tools::send_ok(redis_, key, response);
        } catch (const std::exception &e) {
            redis_tools::send_error(redis_, key, e.what());
            redis_tools::log(redis_, motor_props_.name, e.what(), redis_tools::LogLevel::error);
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

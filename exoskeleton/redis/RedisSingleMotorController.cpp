#include "RedisSingleMotorController.h"
#include "RedisTools.h"
#include <sw/redis++/redis++.h>
#include <chrono>
#include <sstream>
#include <vector>
#include <iostream>
#include <nlohmann/json.hpp>
// Redis key constants
static const std::string STARTED_KEY = "started";
static const std::string SYNC_KEY = "sync:loop:next";
static const std::string STOP_KEY = "stop";
static const std::string COMMAND_KEY = "command";
static const std::string COMMAND_RESULT_KEY = "commandres";
static const std::string COMMAND_PARTIAL_RESULT_KEY = "commandrespart";
static const std::string EXIT_KEY = "exit";



std::vector<int> parseJsonArray(const std::string& json_str) {
    auto j = nlohmann::json::parse(json_str);
    std::vector<int> result;

    for (const auto& el : j) {
        result.push_back(el.get<int>());
    }
    return result;
}

void RedisSingleMotorController::loop() {
    motor_.connect();

    auto subscriber = exoskeleton::redis_tools::make_keyspace_subscriber(redis_,"sync:loop:next");

    subscriber.on_message([this](std::string channel, std::string msg) {
       // std::cout << "[DEBUG] sync:loop:next triggerelt: " << msg << std::endl;
        if (msg != "set") return;

        auto raw_command = redis_.rpop("command:" + std::to_string(address_));

        if (raw_command) {
            processCommand(*raw_command);
        } else {
            measureAndStore();
            exoskeleton::redis_tools::signal_data_ready(redis_,n_motors_);
            //std::cout << "[DEBUG] Nincs parancs " << data.size() << std::endl;
        }
    });

    while (true) {
        if (auto const exit_flag = redis_.get("exit");
            exit_flag && *exit_flag == "1") {
            motor_.disable(0);  // TODO emergency_stop
            std::cout << "Exited by \"exit\" command" << std::endl;
            break;
        }

        if (auto const stop_flag = redis_.getset("stop:" + std::to_string(address_), "0");
            stop_flag && *stop_flag == "1") {
            std::cout << "stop" << std::endl;
            auto const data = motor_.disable(0);  // TODO emergency_stop
            exoskeleton::redis_tools::send_ok(
                redis_,
                COMMAND_RESULT_KEY
                + ":stop:" + std::to_string(address_),
                data.to_string()
            );
            continue;
        }
        subscriber.consume();  // ez figyeli az üzeneteket
    }
}

void RedisSingleMotorController::processCommand(const std::string &raw_command) {
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

    std::string key{partial ? COMMAND_PARTIAL_RESULT_KEY : COMMAND_RESULT_KEY};
    key += ":";
    key += c;
    key += ":";
    key += std::to_string(address_);
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
            auto const record = motor_.enable(0);
            response = record.to_string();
        }
        else if (c == "disable") {
            auto const record = motor_.disable(0);
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
            if (auto values = parseJsonArray(json_str); !values.empty()) {
                int slot = values.front();
                values.erase(values.begin());

                auto record = motor_.upload_function(0,slot,values);
                response = record.to_string();
            }
        }
        else if (c=="fn_select") {
            int slot = std::stoi(parts[3]);
            auto const record = motor_.select_function(0,slot);
            response = record.to_string();
        }
        else if (c == "zero") {
            auto const record = motor_.set_zero(0);
            response = record.to_string();
        }
        else if (c == "function") {
            std::string const& json_str = parts[3];
            if (auto values = parseJsonArray(json_str); !values.empty()) {
                auto const record = motor_.set_function(0,values,7);
                response = record.to_string();
            }
        }
        // TODO offset
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

auto RedisSingleMotorController::measureAndStore() -> SingleMotorData {
    auto const data = motor_.read();
    if (!data.empty()) {
        exoskeleton::redis_tools::xadd_motor_data(redis_, address_, data[0]);
    }
    return data[0];
}

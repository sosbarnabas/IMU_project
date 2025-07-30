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
            std::cout << "Exited by \"exit\" command" << std::endl;
            break;
        }

        if (auto const stop_flag = redis_.getset("stop:" + std::to_string(address_), "0");
            stop_flag && *stop_flag == "1") {
            std::cout << "stop" << std::endl;
            auto const data = motor_.disable(0);
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

    try {
        if (c == "enable") {

             auto record = motor_.enable(0);

            exoskeleton::redis_tools::send_ok(
                redis_,
                (partial ? COMMAND_PARTIAL_RESULT_KEY : COMMAND_RESULT_KEY)
                + ":enable:" + std::to_string(address_) + ":" + t,
                record.to_string()
            );
        }
        else if (c == "disable") {
           // auto t0 = std::chrono::steady_clock::now();
             auto record = motor_.disable(0);
            //auto t1 = std::chrono::steady_clock::now();
            //auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0);
            //std::cout << dur.count() << "ms" << std::endl;
            exoskeleton::redis_tools::send_ok(
                redis_,
                (partial ? COMMAND_PARTIAL_RESULT_KEY : COMMAND_RESULT_KEY)
                + ":disable:" + std::to_string(address_) + ":" + t,
                record.to_string()
            );
        }
        else if (c == "read") {
            auto const data = measureAndStore();
            exoskeleton::redis_tools::send_ok(
                redis_,
                (partial ? COMMAND_PARTIAL_RESULT_KEY : COMMAND_RESULT_KEY)
                + ":read:" + std::to_string(address_) + ":" + t,
                data.to_string()
            );
        }
        else if (c == "fn_upload") {
            if (parts.size() < 4) {
                throw std::runtime_error("Missing value for fn_upload");
            }
            std::string json_str = parts[3];
            auto values = parseJsonArray(json_str);

            if (!values.empty()) {
                int slot = values.front();
                values.erase(values.begin());

                auto record = motor_.upload_function(0,slot,values);

                exoskeleton::redis_tools::send_ok(
                    redis_,
                    (partial ? COMMAND_PARTIAL_RESULT_KEY : COMMAND_RESULT_KEY)
                    + ":fn_upload:" + std::to_string(address_) + ":" + t,
                    record.to_string()
                );
            }
        }
        else if (c=="fn_select") {
            int slot = std::stoi(parts[3]);
            auto record = motor_.select_function(0,slot);
            exoskeleton::redis_tools::send_ok(
                redis_,
                (partial ? COMMAND_PARTIAL_RESULT_KEY : COMMAND_RESULT_KEY)
                + ":fn_select:" + std::to_string(address_) + ":" + t,
                    record.to_string()
                    );
        }
        else if (c == "zero") {
            auto record = motor_.set_zero(0);
            exoskeleton::redis_tools::send_ok(
                redis_,
                (partial ? COMMAND_PARTIAL_RESULT_KEY : COMMAND_RESULT_KEY)
                    + ":zero:" + std::to_string(address_) + ":" + t,
            record.to_string());
        }
        else if (c == "function") {

            std::string json_str = parts[3];
            auto values = parseJsonArray(json_str);

            if (!values.empty()) {

                auto record = motor_.set_function(0,values,7);

                exoskeleton::redis_tools::send_ok(
                    redis_,
                    (partial ? COMMAND_PARTIAL_RESULT_KEY : COMMAND_RESULT_KEY)
                    + ":function:" + std::to_string(address_) + ":" + t,
                    record.to_string()
                );
            }
        }
        // TODO offset
        // TODO connect
        // TODO disconnect
        // TODO status
        else {
            std::cerr << "Unknown command: " << c << std::endl;
        }
    }
    catch (const std::exception &e) {
        std::cerr << "[DEBUG] Ixepswn " << idx << std::endl;
        exoskeleton::redis_tools::send_error(
            redis_,
            (partial ? COMMAND_PARTIAL_RESULT_KEY : COMMAND_RESULT_KEY)
            + ":" + c + ":" + std::to_string(address_) + ":" + t,
            e.what()
        );
    }
}

auto RedisSingleMotorController::measureAndStore() -> SingleMotorData {
    auto data = motor_.read();
    if (!data.empty()) {
        exoskeleton::redis_tools::xadd_motor_data(redis_, address_, data[0]);
    }
    return data[0];
}

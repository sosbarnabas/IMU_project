#include "RedisSingleMotorController.h"
#include "RedisTools.h"
#include <sw/redis++/redis++.h>
#include <chrono>
#include <sstream>
#include <vector>
#include <iostream>

// Redis key constants
static const std::string STARTED_KEY = "started";
static const std::string SYNC_KEY = "sync:loop:next";
static const std::string STOP_KEY = "stop";
static const std::string COMMAND_KEY = "command";
static const std::string COMMAND_RESULT_KEY = "commandres";
static const std::string COMMAND_PARTIAL_RESULT_KEY = "commandrespart";
static const std::string EXIT_KEY = "exit";

void RedisSingleMotorController::loop() {
    redis_.brpop("started:" + std::to_string(address_), 0);
    std::cout << "[DEBUG] Start jelzés megérkezett!" << std::endl;
    motor_.connect();
    auto subscriber = exoskeleton::redis_tools::make_keyspace_subscriber(redis_,"sync:loop:next");
    subscriber.subscribe("sync:loop:next");

    subscriber.on_message([this](std::string channel, std::string msg) {
        std::cout << "[DEBUG] sync:loop:next triggerelt: " << msg << std::endl;
        if (msg != "set") return;

        auto raw_command = redis_.rpop("command:" + std::to_string(address_));
        if (raw_command) {
            std::cout << "[DEBUG] Parancs: " << *raw_command << std::endl;
            processCommand(*raw_command);
        } else {
            measureAndStore();
            //std::cout << "[DEBUG] Nincs parancs " << data.size() << std::endl;
        }
    });

    while (true) {
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
            std::cerr << "[DEBUG] Enabl motor " << idx << std::endl;
            motor_.raw_enable(idx);
            std::cerr << "[DEBUG] Enabled motor " << idx << std::endl;
            exoskeleton::redis_tools::send_ok(
                redis_,
                (partial ? COMMAND_PARTIAL_RESULT_KEY : COMMAND_RESULT_KEY)
                + ":enable:" + std::to_string(address_) + ":" + t,
                "1,0,0,0,0,0,0"
            );
        }
        else if (c == "disable") {
            motor_.disable(idx);
            exoskeleton::redis_tools::send_ok(
                redis_,
                (partial ? COMMAND_PARTIAL_RESULT_KEY : COMMAND_RESULT_KEY)
                + ":disable:" + std::to_string(address_) + ":" + t,
                "ok"
            );
        }
        else if (c == "read") {
            measureAndStore();
        }
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

void RedisSingleMotorController::measureAndStore() {
    auto data = motor_.read();
    std::cout << data[0] << std::endl;
    if (!data.empty()) {
        exoskeleton::redis_tools::xadd_motor_data(redis_, address_, data[0]);
    }
}

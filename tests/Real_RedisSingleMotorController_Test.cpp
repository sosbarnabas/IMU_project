// test_RedisSingleMotorController.cpp
#include "../exoskeleton/redis/RedisSingleMotorController.h"
#include <sw/redis++/redis++.h>
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    try {
        std::cout << "[DEBUG] Setting up real controller on actual motor port...\n";

        int address = 1;
        std::string port = "COM3";

        RedisSingleMotorController controller(address, port);

        std::cout << "[DEBUG] Starting controller loop thread...\n";
        std::thread controller_thread([&controller]() {
            controller.loop();
        });

        std::this_thread::sleep_for(std::chrono::seconds(1));
        sw::redis::Redis redis("tcp://127.0.0.1:6379");

        std::cout << "[DEBUG] Signaling start...\n";
        redis.lpush("started:" + std::to_string(address), "start");

        std::string command_list = "command:" + std::to_string(address);
        std::string t = "123456";

        std::cout << "[DEBUG] Sending test commands...\n";
        redis.lpush(command_list, t + "|enable|-1|0");
        redis.lpush(command_list, t + "|read|-1|0");
        redis.lpush(command_list, t + "|disable|-1|0");

        std::cout << "[DEBUG] Triggering sync events...\n";
        redis.set("sync:loop:next", "set");
        std::this_thread::sleep_for(std::chrono::seconds(1));
        redis.set("sync:loop:next", "set");
        std::this_thread::sleep_for(std::chrono::seconds(1));
        redis.set("sync:loop:next", "set");
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::cout << "[DEBUG] Sending exit signal...\n";
        redis.set("exit", "1");
        redis.set("sync:loop:next", "set");

        controller_thread.join();
        std::cout << "[DEBUG] Test completed successfully.\n";
    }
    catch (const std::exception &e) {
        std::cerr << "[ERROR] Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
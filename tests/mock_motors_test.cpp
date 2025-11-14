#include "../exoskeleton/mock/MockRedisSinglePortController.h"
#include "../exoskeleton/core/RedisSingleIMUController.h"
#include "../exoskeleton/core/RedisTools.h"
#include <sw/redis++/redis++.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

using json = nlohmann::json;
using namespace sw::redis;

/**
 * Simple test executable for mock motors with Redis backend.
 *
 * Usage:
 * 1. Ensure Redis is running on localhost:6379
 * 2. Run this executable in one terminal

 */
int main()
{
    try
    {
        std::cout << "[MOCK_TEST] Initializing mock motor test...\n";

        // Connect to Redis
        Redis redis("tcp://127.0.0.1:6379");
        redis.del("exit");
        redis.set("mock_mode", "1");

        std::cout << "[MOCK_TEST] Connected to Redis.\n";

        // Create mock controller
        auto mock_controller =
            std::make_unique<exoskeleton::core::MockRedisSinglePortController>(7);

        std::cout << "[MOCK_TEST] Mock controller created. Starting loops in background threads...\n";

        // Initialize MCP2221 for IMU
        std::cout << "[MOCK_TEST] Initializing MCP2221 for IMU...\n";
        MCP2221 mcp2221;

        auto imu_controller = std::make_unique<exoskeleton::core::RedisSingleIMUController>(
            mcp2221,
            "tcp://127.0.0.1:6379",
            0 // imu_id = 0
        );
        std::cout << "[MOCK_TEST] IMU controller created.\n";

        // Run motor controller in a separate thread
        std::thread controller_thread([&mock_controller]()
                                      {
            try {
                mock_controller->loop();
            } catch (const std::exception& e) {
                std::cerr << "[MOCK_TEST] Motor controller error: " << e.what() << "\n";
            } });

        // Run IMU controller in a separate thread
        std::thread imu_thread([&imu_controller]()
                               {
            try {
                imu_controller->loop();
            } catch (const std::exception& e) {
                std::cerr << "[MOCK_TEST] IMU controller error: " << e.what() << "\n";
            } });

        std::cout << "[MOCK_TEST] Controller threads started (motor + IMU).\n";
        ;
        std::cout << "[MOCK_TEST] You can now send commands via Redis:\n";
        std::cout << "  Motor commands:\n";
        std::cout << "    redis-cli RPUSH command:0 '0|enable|0|'\n";
        std::cout << "    redis-cli XREAD STREAMS xdata:0 0\n";
        std::cout << "  IMU commands:\n";
        std::cout << "    redis-cli RPUSH command:imu:0 '0|connect|0|'\n";
        std::cout << "    redis-cli RPUSH command:imu:0 '0|icminit|0|'\n";
        std::cout << "    redis-cli RPUSH command:imu:0 '0|start|0|'\n";
        std::cout << "    redis-cli XREAD STREAMS xdata:imu:0 0\n";
        std::cout << "    redis-cli SET exit 1  (to stop all)\n";
        std::cout << "[MOCK_TEST] Waiting for controllers to finish...\n";

        // Wait for both threads
        controller_thread.join();
        imu_thread.join();

        std::cout << "[MOCK_TEST] Mock motor test finished.\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "[MOCK_TEST] Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

#include <chrono>
#include <iostream>
#include <thread>
#include <windows.h>
#include "mcp2221.h"
#include "icm20948.h"
#include "threadsafe_queue.h"
#include "ImuSample.h"
#include "exoskeleton/core/RedisSingleIMUController.h"
#include "exoskeleton/core/RedisTools.h"

#define ICM1_ADDR 0x69
#define ICM2_ADDR 0x68

/**
 * @file main_redis.cpp
 * @brief Integration example: IMU with Redis-based RedisSingleIMUController
 *
 * This example demonstrates:
 * 1. Initialize MCP2221 (USB-I2C bridge)
 * 2. Initialize ICM20948 IMU
 * 3. Create RedisSingleIMUController
 * 4. Launch IMU controller loop in a separate thread
 * 5. Send commands via Redis to control the IMU (connect, start, stop, zero, read)
 * 6. Read IMU telemetry from Redis stream (xdata:imu:1)
 *
 * Command Format (via Redis LPUSH command:imu:1):
 *   "type|command|idx|value"
 *   Example: "t|start|-1|1" (start producer)
 *   Example: "t|zero|-1|1"  (toggle zeroing)
 *   Example: "t|read|-1|1"  (measure now)
 *
 * Redis Streams:
 *   - Input:  command:imu:1 (LPUSH commands here)
 *   - Output: xdata:imu:1 (IMU telemetry, XADD'd by controller)
 *   - Responses: commandres:<cmd>:imu:1:t (command results)
 *
 * Architecture:
 *   - Async independent loop running at ~120Hz
 *   - No blocking with motor loops (suitable for multi-motor systems)
 *   - Thread-safe via std::mutex on Redis and MCP2221 operations
 */

namespace
{
    // Command-line helper for interactive testing
    void print_usage()
    {
        std::cout << "\n=== Redis IMU Controller Commands ===\n"
                  << "Redis CLI examples:\n"
                  << "  LPUSH command:imu:1 \"t|start|-1|1\"     # Start producer\n"
                  << "  LPUSH command:imu:1 \"t|stop|-1|1\"      # Stop producer\n"
                  << "  LPUSH command:imu:1 \"t|zero|-1|1\"      # Toggle zeroing\n"
                  << "  LPUSH command:imu:1 \"t|read|-1|1\"      # Measure now\n"
                  << "  LPUSH command:imu:1 \"t|connect|-1|1\"   # Re-connect IMU\n"
                  << "  LPUSH command:imu:1 \"t|disconnect|-1|1\" # Disconnect IMU\n"
                  << "\nReading responses:\n"
                  << "  LRANGE commandres:start:imu:1:t 0 -1\n"
                  << "  XRANGE xdata:imu:1 - +           # Read all IMU samples\n"
                  << "\nExit: Set \"exit\" key to \"1\"\n"
                  << "  SET exit 1\n"
                  << "=================================\n\n";
    }
}

int main()
{
    try
    {
        std::cout << "[MAIN] Starting Redis IMU Controller" << std::endl;
        print_usage();

        // ============ Initialize Hardware ============
        std::cout << "[MAIN] Initializing MCP2221 (USB-I2C bridge)..." << std::endl;
        MCP2221 mcp2221;
        if (!mcp2221.open())
        {
            std::cerr << "[MAIN] ERROR: Failed to open MCP2221" << std::endl;
            return -1;
        }
        std::cout << "[MAIN] MCP2221 opened successfully" << std::endl;

        // ============ Initialize IMU ============
        std::cout << "[MAIN] Initializing ICM20948 IMU at 0x" << std::hex << ICM1_ADDR << std::dec << std::endl;
        auto begin = std::chrono::steady_clock::now();

        ICM20948 imu1(mcp2221, ICM1_ADDR, 1, def_imu_cfg);
        if (!imu1.Initialize())
        {
            std::cerr << "[MAIN] ERROR: Failed to initialize IMU" << std::endl;
            mcp2221.close();
            return -1;
        }

        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
        std::cout << "[MAIN] IMU initialized successfully in " << duration.count() << " ms" << std::endl;

        // ============ Configure FIFO ============
        std::cout << "[MAIN] Configuring IMU FIFO..." << std::endl;
        if (!imu1.FIFOConfig())
        {
            std::cerr << "[MAIN] ERROR: FIFO configuration failed" << std::endl;
            mcp2221.close();
            return -1;
        }
        std::cout << "[MAIN] FIFO configured" << std::endl;

        // ============ Calibrate ============
        std::cout << "[MAIN] Calibrating accelerometer and gyroscope..." << std::endl;
        if (!imu1.CalibrateAccelGyro(1000))
        {
            std::cerr << "[MAIN] WARNING: Calibration returned false, continuing anyway" << std::endl;
        }
        std::cout << "[MAIN] Calibration complete" << std::endl;

        // ============ Flush FIFO ============
        std::cout << "[MAIN] Flushing initial FIFO data..." << std::endl;
        if (!imu1.ReadFIFO())
        {
            std::cerr << "[MAIN] WARNING: Initial FIFO read failed" << std::endl;
        }
        std::cout << "[MAIN] FIFO flushed" << std::endl;

        // ============ Start Producer Thread ============
        std::cout << "[MAIN] Starting IMU producer thread with TSQueue..." << std::endl;
        TSQueue<ImuSample> imu_queue;
        imu1.start(imu_queue);
        std::cout << "[MAIN] Producer thread started" << std::endl;

        // ============ Create and Launch Redis Controller ============
        std::cout << "[MAIN] Creating RedisSingleIMUController..." << std::endl;
        exoskeleton::core::RedisSingleIMUController imu_controller(
            imu1,
            "imu:1",               // device_key for Redis commands
            1,                     // imu_id for stream naming
            "tcp://127.0.0.1:6379" // Redis URI
        );
        std::cout << "[MAIN] RedisSingleIMUController created" << std::endl;

        std::cout << "[MAIN] Launching IMU controller loop in separate thread..." << std::endl;
        auto imu_thread = std::jthread([&imu_controller](std::stop_token st)
                                       {
            try {
                std::cout << "[IMU_THREAD] Starting event loop" << std::endl;
                imu_controller.loop();
                std::cout << "[IMU_THREAD] Event loop exited" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[IMU_THREAD] Exception: " << e.what() << std::endl;
            } });
        std::cout << "[MAIN] IMU controller thread launched" << std::endl;

        // ============ Wait for Shutdown ============
        std::cout << "[MAIN] IMU system running. Send commands via Redis." << std::endl;
        std::cout << "[MAIN] Press Ctrl+C or set \"exit\" key to 1 in Redis to shutdown." << std::endl;

        // Keep main thread alive while controller runs
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // ============ Cleanup (if reached) ============
        std::cout << "[MAIN] Shutting down..." << std::endl;
        imu1.stop();
        mcp2221.close();
        std::cout << "[MAIN] Cleanup complete" << std::endl;

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[MAIN] Exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "[MAIN] Unknown exception" << std::endl;
        return 1;
    }
}

#pragma once

#include <string>
#include <mutex>
#include <sw/redis++/redis++.h>
#include "../../ICM/icm20948.h"
#include "../settings/Settings.h"

namespace exoskeleton::core
{

    /**
     * @class RedisSingleIMUController
     * @brief Manages a single IMU device through Redis commands.
     *
     * Features:
     * - Subscribes to sync:loop:next keyspace notifications for independent async loop (~120Hz)
     * - Processes commands from Redis queue: command:<device_key> (RPOP)
     * - Writes telemetry to separate stream: xdata:imu:<imu_id>
     * - Supports: start/stop producer thread, zero (Euler offset), read (measure now)
     * - Thread-safe: each controller runs independently in its own thread
     * - No synchronization with motor loops (async independent execution)
     */
    class RedisSingleIMUController
    {
    public:
        /**
         * @brief Constructor
         * @param imu Reference to ICM20948 IMU device (shared ownership via reference)
         * @param device_key Redis key identifier (e.g., "imu:1")
         * @param imu_id Numeric IMU identifier for stream and data
         * @param redis_uri Redis connection string (default: localhost:6379)
         */
        RedisSingleIMUController(
            ICM20948 &imu,
            const std::string &device_key,
            int imu_id,
            const std::string &redis_uri = "tcp://127.0.0.1:6379");

        /**
         * @brief Main event loop for IMU controller.
         *
         * Runs asynchronously at ~120Hz:
         * - Subscribes to sync:loop:next keyspace notifications
         * - Processes commands from command:<device_key> queue (RPOP)
         * - Measures and stores IMU samples on each notification
         * - Calls signal_data_ready(redis, 1) independently (no sync with motors)
         * - Monitors exit flag for graceful shutdown
         *
         * Should be called from its own thread or loop thread pool.
         */
        void loop();

    private:
        ICM20948 &imu_;
        std::string device_key_;
        int imu_id_;
        sw::redis::Redis redis_;
        std::mutex redis_mutex_; // Protects redis_ operations (thread-safe Redis client)

        /**
         * @brief Process a single command from Redis queue.
         *
         * Command format: "type|command|idx|value"
         * - type: transaction/request type identifier
         * - command: "start", "stop", "zero", "read", "connect", "disconnect"
         * - idx: IMU instance index (-1 for self)
         * - value: command-specific parameter
         *
         * Stores response in commandres:<cmd>:<device_key>:<type> list
         */
        void processCommand(const std::string &raw_command);

        /**
         * @brief Read FIFO and write sample to Redis stream.
         *
         * - Reads current FIFO data from IMU
         * - Applies zeroing offset if enabled
         * - XADD to xdata:imu:<imu_id> stream
         * - Calls signal_data_ready(redis_, 1) independently
         *
         * @return ImuSample if successful, empty/default if error
         */
        auto measureAndStore() -> ImuSample;
    };

} // namespace exoskeleton::core

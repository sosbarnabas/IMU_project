#pragma once
#include <string>
#include <memory>
#include <mutex>
#include <sw/redis++/redis++.h>
#include "../IMU/ICM/icm20948.h"
#include "../IMU/MCP/mcp2221.h"
#include "../IMU/include/ImuSample.h"
#include "../IMU/include/threadsafe_queue.h"

namespace exoskeleton::core
{

    /**
     * Redis-based IMU controller for ICM-20948 9-DOF sensor.
     * Runs independently at 75Hz in a separate thread.
     *
     * Architecture:
     * - Async independent loop (~75 Hz) with no blocking on external devices
     * - Command-based control via Redis queue (command:imu:0)
     * - Separate Redis stream for telemetry (xdata:imu:0)
     * - Thread-safe design (mutex-protected Redis and MCP2221 I2C operations)
     * - Graceful shutdown via `exit` flag
     *
     * Commands (via Redis LPUSH command:imu:0):
     *   - connect      : Initialize MCP2221, open USB-I2C bridge
     *   - icminit      : Initialize ICM-20948 (configure sensors, FIFO)
     *   - calibrate    : Perform or load calibration (params: save/load)
     *   - start        : Begin continuous IMU sampling at 75Hz
     *   - stop         : Pause IMU sampling
     *   - zero         : Enable/disable Euler angle zeroing (params: 1/0)
     *   - disconnect   : Close I2C connection, cleanup
     */
    class RedisSingleIMUController
    {
    public:
        /**
         * Create an IMU controller for ICM-20948 at address 0x69.
         * @param mcp2221_ref Reference to initialized/unopened MCP2221 (will be opened by controller)
         * @param redis_uri Redis connection URI (default: tcp://127.0.0.1:6379)
         * @param imu_id Device ID for Redis keys (default: 0)
         */
        RedisSingleIMUController(MCP2221 &mcp2221_ref,
                                 const std::string &redis_uri = "tcp://127.0.0.1:6379",
                                 int imu_id = 0);

        ~RedisSingleIMUController();

        /**
         * Run the IMU control loop (blocking, meant for std::jthread).
         * - Continuously checks for commands in command:imu:0 queue
         * - Reads FIFO from IMU and publishes to xdata:imu:0
         * - Target frequency: 75 Hz
         * - Exits when Redis key "exit" is set to "1"
         */
        void loop();

    private:
        int imu_id_;
        std::string device_key_; // "imu:0" for 0x69 address
        MCP2221 &mcp_;
        std::unique_ptr<ICM20948> imu_;
        sw::redis::Redis redis_;
        std::mutex redis_mutex_;

        // State
        bool connected_ = false;
        bool sampling_active_ = false;
        bool zeroing_enabled_ = false;
        uint64_t sample_sequence_ = 0;

        // Producer/consumer queue for IMU samples (preferred over ReadFIFO)
        TSQueue<ImuSample> sample_queue_;
        std::thread consumer_thread_;
        std::atomic<bool> consumer_running_{false};
        std::atomic<bool> stop_consumer_{false};

        // Calibration data
        bool calibration_loaded_ = false;
        std::array<float, 3> euler_zero_ref_{0, 0, 0}; // Reference Euler angles for zeroing

        /**
         * Process a single command from Redis.
         * Commands: connect, icminit, calibrate, start, stop, zero, disconnect
         */
        void processCommand(const std::string &raw_command);

        /**
         * Read samples from IMU FIFO and publish to Redis stream.
         * Also applies zeroing if enabled.
         */
        void measureAndStore();

        // Start/stop the consumer thread that drains `sample_queue_` and
        // publishes samples to Redis. The producer is started by calling
        // `imu_->start(sample_queue_)` (done during `icminit`).
        void startConsumer();
        void stopConsumer();

        /**
         * Publish response to command response queue.
         * Key: commandres:<command>:imu:<id>:t
         */
        void publishResponse(const std::string &command, const std::string &response);

        /**
         * Log message to Redis log stream with IMU tag.
         */
        void log(const std::string &level, const std::string &message);
    };

} // namespace exoskeleton::core

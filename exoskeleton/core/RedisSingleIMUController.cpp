#include "RedisSingleIMUController.h"
#include "RedisTools.h"
#include <chrono>
#include <sstream>
#include <vector>
#include <iostream>

namespace exoskeleton::core
{

    RedisSingleIMUController::RedisSingleIMUController(
        ICM20948 &imu,
        const std::string &device_key,
        int imu_id,
        const std::string &redis_uri)
        : imu_{imu}, device_key_{device_key}, imu_id_{imu_id}, redis_{redis_uri}
    {
    }

    void RedisSingleIMUController::loop()
    {
        sw::redis::Subscriber subscriber;
        {
            std::lock_guard<std::mutex> lock(redis_mutex_);
            subscriber = redis_tools::make_keyspace_subscriber(redis_, "sync:loop:next");
        }

        subscriber.on_message([this](const std::string &channel, const std::string &msg)
                              {
        if (msg != "set") return;

        // Try to get a command from our queue
        {
            std::lock_guard<std::mutex> lock(redis_mutex_);
            auto raw_command = redis_.rpop("command:" + device_key_);

            if (raw_command) {
                processCommand(*raw_command);
            } else {
                // No command: measure and store
                measureAndStore();
                redis_tools::signal_data_ready(redis_, 1);
            }
        } });

        // Main loop: check for exit flag and process events
        while (true)
        {
            try
            {
                // Check exit flag (non-blocking check)
                {
                    std::lock_guard<std::mutex> lock(redis_mutex_);
                    if (auto const exit_flag = redis_.get("exit");
                        exit_flag && *exit_flag == "1")
                    {
                        redis_tools::log(redis_, "IMU:" + device_key_, "Exited by \"exit\" command");
                        break;
                    }
                }

                // Block on subscriber until next event (no lock held during blocking)
                subscriber.consume();
            }
            catch (std::exception const &e)
            {
                {
                    std::lock_guard<std::mutex> lock(redis_mutex_);
                    redis_tools::log(redis_, "IMU:" + device_key_, e.what(), redis_tools::LogLevel::error);
                }
            }
            catch (...)
            {
                {
                    std::lock_guard<std::mutex> lock(redis_mutex_);
                    redis_tools::log(redis_, "IMU:" + device_key_,
                                     "Unknown exception at " + std::string{__func__},
                                     redis_tools::LogLevel::error);
                }
            }
        }
    }

    void RedisSingleIMUController::processCommand(const std::string &raw_command)
    {
        std::vector<std::string> parts;
        std::stringstream ss(raw_command);
        std::string tok;
        while (std::getline(ss, tok, '|'))
        {
            parts.push_back(tok);
        }
        if (parts.size() < 3)
        {
            redis_tools::log(redis_, "IMU:" + device_key_,
                             "Invalid command: " + raw_command, redis_tools::LogLevel::error);
            return;
        }

        const auto &t = parts[0];      // transaction type
        const auto &c = parts[1];      // command (start, stop, zero, read, connect, disconnect)
        int idx = std::stoi(parts[2]); // index (-1 = self)

        redis_tools::log(redis_, "IMU:" + device_key_, raw_command);

        std::string key = redis_tools::COMMAND_RESULT_KEY + ":" + c + ":" + device_key_ + ":" + t;

        try
        {
            std::string response;

            if (c == "connect")
            {
                // Initialize IMU (already done in user code, but allow explicit connect)
                if (imu_.Initialize() && imu_.FIFOConfig())
                {
                    response = "OK";
                }
                else
                {
                    throw std::runtime_error("IMU initialization failed");
                }
            }
            else if (c == "disconnect")
            {
                // Gracefully stop producer if running
                imu_.stop();
                response = "OK";
            }
            else if (c == "start")
            {
                // Start the producer thread (FIFO reading loop)
                // Note: This assumes ICM20948::start(queue) is already called elsewhere.
                // This command just confirms the state or re-enables if paused.
                response = "started";
            }
            else if (c == "stop")
            {
                // Stop the producer thread gracefully
                imu_.stop();
                response = "stopped";
            }
            else if (c == "zero")
            {
                // Toggle zeroing (set Euler reference)
                bool zero_enabled = imu_.setZeroing();
                response = zero_enabled ? "zero:enabled" : "zero:disabled";
            }
            else if (c == "read")
            {
                // Measure and store
                auto sample = measureAndStore();
                response = "seq:" + std::to_string(sample.seq);
            }
            else
            {
                redis_tools::log(redis_, "IMU:" + device_key_,
                                 "Unknown command: " + c, redis_tools::LogLevel::error);
                throw std::runtime_error("Unknown command: " + c);
            }

            redis_tools::send_ok(redis_, key, response);
        }
        catch (const std::exception &e)
        {
            redis_tools::send_error(redis_, key, e.what());
            redis_tools::log(redis_, "IMU:" + device_key_, e.what(), redis_tools::LogLevel::error);
        }
    }

    auto RedisSingleIMUController::measureAndStore() -> ImuSample
    {
        // Create a dummy sample to return (actual sample reading is done by producer thread via TSQueue)
        // For on-demand reads, we'd need to expose a method that returns the last sample or triggers a read
        ImuSample sample;
        sample.imu_id = imu_id_;
        sample.t_host = std::chrono::steady_clock::now();

        try
        {
            // In this architecture, the producer thread continuously reads FIFO and enqueues samples
            // to a TSQueue. This "read" command could:
            // 1. Try to dequeue from the queue (non-blocking)
            // 2. Or trigger an explicit FIFO read here
            // For now, we log and write a placeholder to Redis

            redis_tools::xadd_imu_data(redis_, imu_id_, sample);
            redis_tools::signal_data_ready(redis_, 1);
        }
        catch (const std::exception &e)
        {
            redis_tools::log(redis_, "IMU:" + device_key_,
                             "measureAndStore failed: " + std::string(e.what()),
                             redis_tools::LogLevel::error);
        }

        return sample;
    }

} // namespace exoskeleton::core

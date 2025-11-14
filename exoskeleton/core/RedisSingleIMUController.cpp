#include "RedisSingleIMUController.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <optional>
#include <iomanip>
#include <atomic>

namespace exoskeleton::core {
    struct CommandRecord {
        long long timestamp = 0;
        std::string command;
        int address = 0;
        std::string parameters;
    };

    std::optional<CommandRecord> parseImuCommandRecord(const std::string &record) {
        const auto first = record.find('|');
        if (first == std::string::npos)
            return std::nullopt;

        const auto second = record.find('|', first + 1);
        if (second == std::string::npos)
            return std::nullopt;

        const auto third = record.find('|', second + 1);
        if (third == std::string::npos)
            return std::nullopt;

        CommandRecord result;
        try {
            result.timestamp = std::stoll(record.substr(0, first));
        } catch (...) {
            result.timestamp = 0;
        }

        result.command = record.substr(first + 1, second - first - 1);

        try {
            result.address = std::stoi(record.substr(second + 1, third - second - 1));
        } catch (...) {
            result.address = 0;
        }

        result.parameters = record.substr(third + 1);
        return result;
    }

    long long nowNs() {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    }

    RedisSingleIMUController::RedisSingleIMUController(MCP2221 &mcp2221_ref,
                                                       const std::string &redis_uri,
                                                       int imu_id)
        : imu_id_(imu_id),
          device_key_("imu:" + std::to_string(imu_id)),
          mcp_(mcp2221_ref),
          imu_(nullptr),
          redis_(redis_uri) {
        std::cout << "[IMUController] Initialized for " << device_key_ << " at address 0x69\n";
    }

    RedisSingleIMUController::~RedisSingleIMUController() {
        if (imu_) {
            imu_.reset();
        }
        std::cout << "[IMUController] Destroyed\n";
    }

    void RedisSingleIMUController::loop() {
        std::cout << "[IMUController] Starting main loop (75Hz)...\n";
        std::cout << "[IMUController] Commands: connect, icminit, calibrate, start, stop, zero, disconnect\n";
        std::cout << "[IMUController] Command queue: command:" << device_key_ << "\n";
        std::cout << "[IMUController] Data stream: xdata:" << device_key_ << "\n";

        auto last_data_push = std::chrono::steady_clock::now();
        const auto data_push_interval = std::chrono::milliseconds(13); // ~75Hz (13.3ms)

        while (true) {
            try {
                // Check exit flag
                if (auto exit_flag = redis_.get("exit"); exit_flag && *exit_flag == "1") {
                    std::cout << "[IMUController] Exit signal received.\n";
                    break;
                }

                // Process commands
                auto cmd_key = std::string("command:") + device_key_;
                auto raw_command = redis_.lpop(cmd_key);
                if (raw_command) {
                    try {
                        processCommand(*raw_command);
                    } catch (const std::exception &e) {
                        std::cerr << "[IMUController] Command error: " << e.what() << "\n";
                        publishResponse("unknown", std::string("ER:") + e.what());
                    }
                }

                // IMU samples are produced into `sample_queue_` by the IMU
                // producer thread. A dedicated consumer thread drains that
                // queue and publishes to Redis. The old ReadFIFO path is
                // obsolete and therefore not used here.

                // Small sleep to prevent busy-waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            } catch (const std::exception &e) {
                std::cerr << "[IMUController] Loop error: " << e.what() << "\n";
                log("ERROR", std::string("Main loop error: ") + e.what());
                break;
            }
        }

        std::cout << "[IMUController] Stopping...\n";
        // Stop consumer thread and request IMU producer stop
        stopConsumer();
        if (imu_) {
            try {
                imu_->stop();
            } catch (...) {
            }
        }
        // Close MCP if we own/managed it; leave to caller if not
        try {
            mcp_.close();
        } catch (...) {
        }
        connected_ = false;
    }

    void RedisSingleIMUController::processCommand(const std::string &raw_command) {
        const auto parsed = parseImuCommandRecord(raw_command);
        if (!parsed) {
            std::cerr << "[IMUController] Unparseable command: '" << raw_command << "'\n";
            return;
        }

        std::string cmd = parsed->command;
        std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        const std::string &params = parsed->parameters;
        const std::string prefix = "[IMU] ";

        try {
            if (cmd == "connect") {
                if (mcp_.open()) {
                    connected_ = true;
                    publishResponse("connect", "OK");
                    log("INFO", "Connected via MCP2221");

                    //icm init is mvoed here
                    auto start = std::chrono::steady_clock::now();
                    // Create ICM instance
                    if (!imu_) {
                        imu_ = std::make_unique<ICM20948>(mcp_, 0x69, imu_id_, def_imu_cfg);
                    }
                    if (imu_->Initialize()) {
                        if (imu_->FIFOConfig()) {
                            // Start the IMU producer that pushes ImuSample into sample_queue_
                            // imu_->start(sample_queue_);
                            // startConsumer();

                            auto end = std::chrono::steady_clock::now();
                            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

                            publishResponse("icminit", "OK:init_time_" + std::to_string(duration.count()) + "ms");
                            log("INFO",
                                "ICM-20948 initialized and producer started in " + std::to_string(duration.count()) +
                                "ms");
                            calibration_loaded_ = false;
                        } else {
                            publishResponse("icminit", "ER:FIFO configuration failed");
                            log("ERROR", "FIFO configuration failed");
                        }
                    } else {
                        publishResponse("icminit", "ER:ICM-20948 initialization failed (not responding at 0x69?)");
                        log("ERROR", "ICM-20948 initialization failed at address 0x69");
                    }
                } else {
                    publishResponse("connect", "ER:Failed to open MCP2221");
                    log("ERROR", "Failed to open MCP2221");
                }
            } else if (cmd == "icminit") {
                if (!connected_) {
                    publishResponse("icminit", "ER:Not connected (run connect first)");
                    return;
                }

                auto start = std::chrono::steady_clock::now();

                // Create ICM instance
                if (!imu_) {
                    imu_ = std::make_unique<ICM20948>(mcp_, 0x69, imu_id_, def_imu_cfg);
                }

                if (imu_->Initialize()) {
                    if (imu_->FIFOConfig()) {
                        // Start the IMU producer that pushes ImuSample into sample_queue_
                        // imu_->start(sample_queue_);
                        // startConsumer();

                        auto end = std::chrono::steady_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

                        publishResponse("icminit", "OK:init_time_" + std::to_string(duration.count()) + "ms");
                        log("INFO",
                            "ICM-20948 initialized and producer started in " + std::to_string(duration.count()) + "ms");
                        calibration_loaded_ = false;
                    } else {
                        publishResponse("icminit", "ER:FIFO configuration failed");
                        log("ERROR", "FIFO configuration failed");
                    }
                } else {
                    publishResponse("icminit", "ER:ICM-20948 initialization failed (not responding at 0x69?)");
                    log("ERROR", "ICM-20948 initialization failed at address 0x69");
                }
            } else if (cmd == "calibrate") {
                if (!imu_) {
                    publishResponse("calibrate", "ER:IMU not initialized (run icminit first)");
                    return;
                }

                std::string cal_mode = params;
                std::transform(cal_mode.begin(), cal_mode.end(), cal_mode.begin(),
                               [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

                if (cal_mode == "save") {
                    std::cout << "[IMUController] Performing calibration (this may take up to 1 second)...\n";
                    auto start = std::chrono::steady_clock::now();

                    imu_->CalibrateAccelGyro(1000); // 1 second calibration

                    auto end = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

                    calibration_loaded_ = true;
                    publishResponse("calibrate", "OK:calibration_complete_" + std::to_string(duration.count()) + "ms");
                    log("INFO", "Calibration performed and saved");
                } else if (cal_mode == "load") {
                    imu_->loadCalibrationfromTxt(calibPathTXT());
                    publishResponse("calibrate", "OK:calibration_loaded");
                    calibration_loaded_ = true;
                    log("INFO", "Calibration loaded from file");
                } else {
                    publishResponse("calibrate", "ER:Invalid calibration mode (use 'save' or 'load')");
                }
            } else if (cmd == "start") {
                if (!imu_) {
                    publishResponse("start", "ER:IMU not initialized");
                    return;
                }
                imu_->start(sample_queue_);
                startConsumer();
                sampling_active_ = true;
                sample_sequence_ = 0;
                publishResponse("start", "OK:sampling_started");
                log("INFO", "IMU sampling started at 75Hz");
                std::cout << prefix << "Sampling started\n";
            } else if (cmd == "stop") {
                sampling_active_ = false;
                imu_->stop();
                stopConsumer(); // Stop the consumer thread
                publishResponse("stop", "OK:sampling_stopped");
                log("INFO", "IMU sampling stopped");
                std::cout << prefix << "Sampling stopped\n";
            } else if (cmd == "zero") {
                if (!sampling_active_) {
                    publishResponse("zero", "ER:Sampling not active");
                    return;
                }

                bool enable_zero = imu_->setZeroing();
                // zeroing_enabled_ = enable_zero;

                if (enable_zero) {
                    publishResponse("zero", "OK:euler_zeroing_enabled");
                    log("INFO", "Euler angle zeroing enabled (reference set to current)");
                } else {
                    publishResponse("zero", "OK:euler_zeroing_disabled");
                    log("INFO", "Euler angle zeroing disabled");
                }
            } else if (cmd == "disconnect") {
                if (connected_) {
                    mcp_.close();
                    connected_ = false;
                    sampling_active_ = false;
                    imu_.reset();

                    publishResponse("disconnect", "OK");
                    log("INFO", "Disconnected from IMU");
                } else {
                    publishResponse("disconnect", "ER:Not connected");
                }
            } else {
                publishResponse(cmd, "ER:Unknown command");
                std::cout << prefix << "Unknown command: '" << cmd << "'\n";
            }
        } catch (const std::exception &e) {
            std::cerr << prefix << "Error processing command: " << e.what() << "\n";
            publishResponse(cmd, std::string("ER:") + e.what());
        }
    }

    void RedisSingleIMUController::measureAndStore() {
        // Obsolete: This ReadFIFO-based path has been replaced by the
        // producer/consumer model where the IMU pushes `ImuSample` into
        // `sample_queue_` and the consumer thread publishes them to Redis.
        (void) imu_;
    }

    void RedisSingleIMUController::startConsumer() {
        if (consumer_running_)
            return;
        consumer_running_ = true;
        consumer_thread_ = std::jthread([this](std::stop_token st) {
            try {
                while (!st.stop_requested()) {
                    ImuSample sample = sample_queue_.wait_dequeue();
                    if (st.stop_requested()) break;

                    // // Apply zeroing if enabled
                    // float euler_x = sample.euler[0];
                    // float euler_y = sample.euler[1];
                    // float euler_z = sample.euler[2];
                    // if (zeroing_enabled_) {
                    //     euler_x -= euler_zero_ref_[0];
                    //     euler_y -= euler_zero_ref_[1];
                    //     euler_z -= euler_zero_ref_[2];
                    // }

                    // Publish to Redis stream
                    auto key = std::string("xdata:") + device_key_;
                    uint64_t t_ns = nowNs();
                    std::vector<std::pair<std::string, std::string> > fields;
                    fields.emplace_back("t_ns", std::to_string(t_ns));
                    //fields.emplace_back("t_ns", std::to_string(sample.t_host);
                    fields.emplace_back("seq", std::to_string(sample.seq));
                    fields.emplace_back("imu_id", std::to_string(sample.imu_id));
                    // fields.emplace_back("accel_x", std::to_string(sample.accel[0]));
                    // fields.emplace_back("accel_y", std::to_string(sample.accel[1]));
                    // fields.emplace_back("accel_z", std::to_string(sample.accel[2]));
                    // fields.emplace_back("gyro_x", std::to_string(sample.gyro[0]));
                    // fields.emplace_back("gyro_y", std::to_string(sample.gyro[1]));
                    // fields.emplace_back("gyro_z", std::to_string(sample.gyro[2]));
                    // fields.emplace_back("mag_x", std::to_string(sample.mag[0]));
                    // fields.emplace_back("mag_y", std::to_string(sample.mag[1]));
                    // fields.emplace_back("mag_z", std::to_string(sample.mag[2]));
                    fields.emplace_back("euler_roll", std::to_string(sample.euler.at(0)));
                    fields.emplace_back("euler_pitch", std::to_string(sample.euler.at(1)));
                    fields.emplace_back("euler_yaw", std::to_string(sample.euler.at(2)));
                    fields.emplace_back("fifo_size", std::to_string(sample.fifosize));
                    fields.emplace_back("fifo_mult", std::to_string(sample.fifomult));
                    uint8_t flags = 0;
                    if (sample.fifo_overflow) flags |= 0x01;
                    if (sample.fifo_underflow) flags |= 0x02;
                    if (sample.mag_ok) flags |= 0x04;
                    fields.emplace_back("flags", std::to_string(flags));

                    try {
                        std::lock_guard<std::mutex> lock(redis_mutex_);
                        redis_.xadd(key, "*", fields.begin(), fields.end());
                    } catch (const std::exception &e) {
                        std::cerr << "[IMUController] Error publishing sample: " << e.what() << "\n";
                    }
                }
            } catch (const std::exception &e) {
                std::cerr << "[IMUController] Consumer thread exception: " << e.what() << "\n";
            }
        });
    }

    void RedisSingleIMUController::stopConsumer() {
        if (!consumer_running_)
            return;

        // Request stop via stop_token
        consumer_thread_.request_stop();

        // Wake up the consumer if it's blocked on wait_dequeue
        try {
            sample_queue_.enqueue(ImuSample());
        } catch (...) {
        }

        // jthread auto-joins in destructor, but we explicitly wait if needed
        if (consumer_thread_.joinable())
            consumer_thread_.join();
        consumer_running_ = false;
    }

    void RedisSingleIMUController::publishResponse(const std::string &command, const std::string &response) {
        try {
            std::lock_guard<std::mutex> lock(redis_mutex_);
            // Single unified response key for all IMU command responses
            auto res_key = "commandres:" + device_key_;
            // Format: timestamp|command|response
            auto t_ns = nowNs();
            std::string formatted = std::to_string(t_ns) + "|" + command + "|" + response;
            redis_.lpush(res_key, formatted);
        } catch (const std::exception &e) {
            std::cerr << "[IMUController] Error publishing response: " << e.what() << "\n";
        }
    }

    void RedisSingleIMUController::log(const std::string &level, const std::string &message) {
        try {
            std::lock_guard<std::mutex> lock(redis_mutex_);
            auto t_ns = nowNs();
            std::vector<std::pair<std::string, std::string> > fields;
            fields.emplace_back("t", std::to_string(t_ns));
            fields.emplace_back("src", "IMU:" + device_key_);
            fields.emplace_back("level", level);
            fields.emplace_back("msg", message);
            redis_.xadd("log", "*", fields.begin(), fields.end());
        } catch (const std::exception &e) {
            std::cerr << "[IMUController] Error logging to Redis: " << e.what() << "\n";
        }
    }
} // namespace exoskeleton::core

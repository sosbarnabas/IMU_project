#include "RedisSingleIMUController.h"
#include "RedisTools.h"
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
          icm20948(nullptr),
          redis_(redis_uri),
          control_(4),
          exercise_controller_(redis_, control_, redis_mutex_) // construct ExerciseController with references
    {
        std::cout << "[IMUController] Initialized for " << device_key_ << " at address 0x69\n";
    }

    RedisSingleIMUController::~RedisSingleIMUController() {
        if (icm20948) {
            icm20948.reset();
        }
        std::cout << "[IMUController] Destroyed\n";
    }

    void RedisSingleIMUController::loop() {
        std::cout << "[IMUController] Starting main loop (75Hz producer, Redis-driven commands)...\n";
        std::cout << "[IMUController] Commands: connect, icminit, calibrate, start, stop, zero, disconnect\n";
        std::cout << "[IMUController] Command queue: command:" << device_key_ << "\n";
        std::cout << "[IMUController] Data stream: xdata:" << device_key_ << "\n";

        // Optional: a flag to break out of the while loop from inside the callback
        std::atomic<bool> stop_requested{false};

        // Subscribe to the same tick key as motors
        auto subscriber =
                exoskeleton::redis_tools::make_keyspace_subscriber(redis_, "sync:loop:next");

        subscriber.on_message([this, &stop_requested](const std::string &channel,
                                                      const std::string &msg) {
            if (msg != "set") {
                return;
            }

            // Check exit flag: same semantics as everywhere else
            if (auto exit_flag = redis_.get("exit"); exit_flag && *exit_flag == "1") {
                std::cout << "[IMUController] Exit signal received.\n";
                stop_requested = true;
                return;
            }

            // Process at most one command per tick from the IMU command list
            const std::string cmd_key = "command:" + device_key_;
            auto raw_command = redis_.lpop(cmd_key);
            if (!raw_command && is_ready_for_measurements()) {
                onFrameStoreIMU();
            }

            try {
                processCommand(*raw_command);
            } catch (const std::exception &e) {
                std::cerr << "[IMUController] Command error: " << e.what() << "\n";
                publishResponse("unknown", std::string("ER:") + e.what());
            }
            catch (...) {
                std::cerr << "[IMUController] Unknown command error\n";
                publishResponse("unknown", "ER:unknown_exception");
            }
        });

        // Main loop: just consume sync:loop:next events
        while (!stop_requested.load()) {
            try {
                subscriber.consume(); // blocks until next "set" on sync:loop:next
            } catch (const std::exception &e) {
                std::cerr << "[IMUController] Loop error: " << e.what() << "\n";
                log("ERROR", std::string("Main loop error: ") + e.what());
                break;
            }
            catch (...) {
                std::cerr << "[IMUController] Unknown loop error\n";
                log("ERROR", "Unknown exception in IMU main loop");
                break;
            }
        }

        std::cout << "[IMUController] Stopping...\n";

        // Same shutdown sequence as before
        stopConsumer(); // stop the consumer thread draining sample_queue_
        if (icm20948) {
            try {
                icm20948->stop(); // stop producer inside ICM
            } catch (...) {
            }
        }
        try {
            mcp_.close();
        } catch (...) {
        }
        connected_ = false;
    }

    void RedisSingleIMUController::processCommand(const std::string &raw_command) {
        if (raw_command == "" || raw_command == " ") return;
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
                    if (!icm20948) {
                        icm20948 = std::make_unique<ICM20948>(mcp_, 0x69, imu_id_, def_imu_cfg);
                    }
                    if (icm20948->Initialize()) {
                        if (icm20948->FIFOConfig()) {
                            // Start the IMU producer that pushes ImuSample into sample_queue_
                            // icm20948->start(sample_queue_);
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
                if (!icm20948) {
                    icm20948 = std::make_unique<ICM20948>(mcp_, 0x69, imu_id_, def_imu_cfg);
                }

                if (icm20948->Initialize()) {
                    if (icm20948->FIFOConfig()) {
                        // Start the IMU producer that pushes ImuSample into sample_queue_
                        // icm20948->start(sample_queue_);
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
                if (!icm20948) {
                    publishResponse("calibrate", "ER:IMU not initialized (run icminit first)");
                    return;
                }

                std::string cal_mode = params;
                std::transform(cal_mode.begin(), cal_mode.end(), cal_mode.begin(),
                               [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

                if (cal_mode == "save") {
                    std::cout << "[IMUController] Performing calibration (this may take up to 1 second)...\n";
                    auto start = std::chrono::steady_clock::now();

                    icm20948->CalibrateAccelGyro(1000); // 1 second calibration

                    auto end = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

                    calibration_loaded_ = true;
                    publishResponse("calibrate", "OK:calibration_complete_" + std::to_string(duration.count()) + "ms");
                    log("INFO", "Calibration performed and saved");
                } else if (cal_mode == "load") {
                    icm20948->loadCalibrationfromTxt(calibPathTXT());
                    publishResponse("calibrate", "OK:calibration_loaded");
                    calibration_loaded_ = true;
                    log("INFO", "Calibration loaded from file");
                } else {
                    publishResponse("calibrate", "ER:Invalid calibration mode (use 'save' or 'load')");
                }
            } else if (cmd == "start") {
                if (!icm20948) {
                    publishResponse("start", "ER:IMU not initialized");
                    return;
                }
                if (!calibration_loaded_) {
                    publishResponse("start", "ER:IMU not calibrated (run 'calibrate save' or 'calibrate load' first)");
                    return;
                }

                icm20948->start(sample_queue_);
                startConsumer(); // now starts the sync:data:ready subscriber
                sampling_active_ = true;
                sample_sequence_ = 0;
                // After IMU initializes and imu_id = 0
                // redis_.hset("run:addrs", "imu"+std::to_string(imu_id_),std::to_string(imu_id_)+"|");


                publishResponse("start", "OK:sampling_started");
                log("INFO", "IMU sampling started at 75Hz");
                std::cout << prefix << "Sampling started\n";
            } else if (cmd == "stop") {
                sampling_active_ = false;
                icm20948->stop();
                stopConsumer(); // Stop the consumer thread
                // After IMU initializes and imu_id = 0

                redis_.hdel("run:addrs", "imu" + std::to_string(imu_id_));
                publishResponse("stop", "OK:sampling_stopped");
                log("INFO", "IMU sampling stopped");
                std::cout << prefix << "Sampling stopped\n";
            } else if (cmd == "zero") {
                if (!sampling_active_) {
                    publishResponse("zero", "ER:Sampling not active");
                    return;
                }

                bool enable_zero = icm20948->setZeroing();
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
                    icm20948.reset();

                    publishResponse("disconnect", "OK");
                    log("INFO", "Disconnected from IMU");
                } else {
                    publishResponse("disconnect", "ER:Not connected");
                }
            } else if (cmd == "exercise") {
                // params is expected as "0|[exercisenum, exerciseparam]" or "[exercisenum, exerciseparam]"
                int channel = 0;
                std::string json_str;
                const auto pipePos = params.find('|');
                if (pipePos != std::string::npos) {
                    // left side: "0"
                    const std::string chan_str = params.substr(0, pipePos);
                    try {
                        channel = std::stoi(chan_str);
                    } catch (...) {
                        channel = 0;
                    }

                    // right side: "[exercisenum, exerciseparam]"
                    json_str = params.substr(pipePos + 1);
                } else {
                    // no channel given, assume params itself is the JSON array
                    json_str = params;
                }

                // Use the same helper as in fn_upload
                auto values = exoskeleton::redis_tools::parseJsonArray(json_str);
                if (values.size() < 1) {
                    std::cerr << prefix << "Invalid exercise array: " << json_str << "\n";
                    return;
                }

                const int exercise_num = values[0];
                const int threshold = (values.size() >= 2) ? values[1] : 0;
                const int cooldown_ms = (values.size() >= 3) ? values[2] : 0;
                if (exercise_num == 2) {
                    if (values.size() < 5) {
                        std::cerr << prefix <<
                                "Strength exercise needs 5 values [2, torque, sets, reps, muscle_type]\n";
                        return;
                    }

                    const int torque = values[1];
                    const int sets = values[2];
                    const int reps = values[3];
                    const int muscle_type = values[4]; // 0=biceps, 1=triceps

                    exercise_controller_.start_strength_exercise(channel, torque, sets, reps, muscle_type);
                    return;
                }
                //set exercise type
                exercise_controller_.set_exercise(channel, exercise_num, threshold, cooldown_ms);
            } else {
                publishResponse(cmd, "ER:Unknown command");
                std::cout << prefix << "Unknown command: '" << cmd << "'\n";
            }
        } catch (const std::exception &e) {
            std::cerr << prefix << "Error processing command: " << e.what() << "\n";
            publishResponse(cmd, std::string("ER:") + e.what());
        }
    }

    void RedisSingleIMUController::onFrameStoreIMU() {
        if (!is_ready_for_measurements()) {
            std::cout << "IMU not yet connected" << std::endl;
            return;
        }
        if (sample_queue_.size() == 0) return;
        // Take "now" once for this call
        auto now_tp = std::chrono::steady_clock::now();
        // ----------------------------------------------------
        // 1) Drain IMU samples from queue into buffered_samples_
        // ----------------------------------------------------
        {
            ImuSample s;
            while (sample_queue_.try_dequeue(s)) {
                std::lock_guard<std::mutex> lock(buffered_samples_mutex_);
                buffered_samples_.push_back(s);
            }
        }

        using StreamEntry = std::pair<std::string, std::map<std::string, std::string> >;

        // ----------------------------------------------------
        // 2) Read only new motor entries from xdata:0 since last call
        // ----------------------------------------------------
        static std::string last_id; // last seen motor stream ID
        std::vector<StreamEntry> entries;

        if (last_id.empty()) {
            redis_.xrevrange("xdata:0", "+", "-", std::back_inserter(entries));

            if (!entries.empty()) {
                // newest entry is first for XREVRANGE
                last_id = entries.front().first;
                //qDebug() << "[INIT] last_id set to:" << last_id.c_str();
            } else {
                qDebug() << "[INIT] xdata:0 is empty";
            }
            return; // important: we do not process historical data
        } {
            std::string end = "+";
            std::string start = "(" + last_id; // IDs strictly greater than last_id

            entries.clear();
            redis_.xrevrange("xdata:0", end, start, std::back_inserter(entries));

            if (entries.empty()) {
                return;
            }
        }


        // ----------------------------------------------------
        // Helper: take one IMU sample for a given motor time.
        // Drops old IMU samples so we use the closest (latest) IMU in the past.
        // ----------------------------------------------------
        auto take_imu_for_motor =
                [this](std::chrono::steady_clock::time_point motor_tp) -> std::optional<ImuSample> {
            //using namespace std::chrono;
            std::lock_guard<std::mutex> lock(buffered_samples_mutex_);

            if (buffered_samples_.empty()) {
                return std::nullopt;
            }

            // Drop oldest IMU samples as long as there is a newer one that is still <= motor time.
            // This keeps the most recent IMU sample that is not later than the motor sample.
            while (buffered_samples_.size() >= 2) {
                const ImuSample &first = buffered_samples_[0];
                const ImuSample &second = buffered_samples_[1];

                if (second.t_host <= motor_tp) {
                    // second is still in the past relative to motor -> first is older and can be dropped
                    buffered_samples_.pop_front();
                } else {
                    break;
                }
            }

            // Now front() is the IMU sample we use (closest in time on the past side in most cases).
            ImuSample chosen = buffered_samples_.front();
            buffered_samples_.pop_front(); // consume this IMU sample

            return chosen;
        };

        static std::optional<ImuSample> last_used_imu; // fallback if buffer is empty
        std::size_t motor_processed = 0;
        std::size_t imu_published = 0;
        // ----------------------------------------------------
        // 3) Process motor entries in chronological order (oldest -> newest)
        // ----------------------------------------------------
        //using namespace std::chrono;

        for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
            const auto &id = it->first;
            const auto &fields = it->second;

            auto it_t = fields.find("t");
            if (it_t == fields.end()) {
                qDebug() << "Motor sample id:" << id.c_str() << " (no 't' field)";
                continue;
            }

            std::int64_t motor_t_raw = 0;
            try {
                motor_t_raw = std::stoll(it_t->second);
            } catch (...) {
                qDebug() << "Motor sample id:" << id.c_str() << " (invalid 't')";
                continue;
            }

            // Motor timestamp as steady_clock::time_point (same base as IMU)
            std::chrono::steady_clock::time_point motor_tp{
                std::chrono::nanoseconds(motor_t_raw)
            };

            auto imu_opt = take_imu_for_motor(motor_tp);
            if (!imu_opt) {
                // No fresh IMU available; optionally reuse last_used_imu
                if (!last_used_imu) {
                    qDebug() << "Motor sample id:" << id.c_str()
                            << " t:" << QString::fromStdString(it_t->second)
                            << " (no IMU available)";
                    continue;
                }
                imu_opt = last_used_imu;
            }

            ImuSample sample = *imu_opt;
            last_used_imu = sample;

            ++motor_processed;

            // Build IMU record for Redis
            const std::string key = std::string("xdata:") + device_key_;

            std::vector<std::pair<std::string, std::string> > redisfields;


            auto imu_t_ns =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        sample.t_host.time_since_epoch()).count();

            // integer nanosecond diff, positive if motor is later than IMU
            std::int64_t diff_ns = motor_t_raw - imu_t_ns;
            // convert to milliseconds (truncate toward zero)
            std::int64_t diff_ms = diff_ns / 1000000;
            redisfields.emplace_back("imu_id", std::to_string(sample.imu_id));
            redisfields.emplace_back("seq", std::to_string(sample.seq));
            redisfields.emplace_back("t_ns", std::to_string(imu_t_ns));
            redisfields.emplace_back("motor_sample_time_diff", std::to_string(diff_ms));

            redisfields.emplace_back("euler_roll", std::to_string(sample.euler.at(0)));
            redisfields.emplace_back("euler_pitch", std::to_string(sample.euler.at(1)));
            redisfields.emplace_back("euler_yaw", std::to_string(sample.euler.at(2)));
            redisfields.emplace_back("gyro_roll", std::to_string(sample.gyro.at(0)));
            redisfields.emplace_back("gyro_pitch", std::to_string(sample.gyro.at(1)));
            redisfields.emplace_back("gyro_yaw", std::to_string(sample.gyro.at(2)));

            redisfields.emplace_back("fifo_size", std::to_string(sample.fifosize));
            redisfields.emplace_back("fifo_mult", std::to_string(sample.fifomult));

            std::uint8_t flags = 0;
            if (sample.accel_overflow) flags |= 0x01;
            if (sample.gyro_overflow) flags |= 0x02;
            //if (sample.mag_ok)         flags |= 0x04;
            redisfields.emplace_back("flags", std::to_string(flags));

            try {
                std::lock_guard<std::mutex> lock(redis_mutex_);
                redis_.xadd(key, "*", redisfields.begin(), redisfields.end());
                ++imu_published;
                // ts_imu_ns: std::int64_t, in nanoseconds
                // ts_imu_ns: std::int64_t, in nanoseconds
                control_.update_imu_state(sample.imu_id,
                                          imu_t_ns,
                                          sample.euler.at(0),
                                          sample.euler.at(1),
                                          sample.euler.at(2),
                                          sample.gyro.at(0),
                                          sample.gyro.at(1),
                                          sample.gyro.at(2))                       ;

                // Drive the exercise logic from the same IMU thread:
                exercise_controller_.updateExerciseFromIMU(sample, imu_t_ns);
            } catch (const std::exception &e) {
                std::cerr << "[IMUController] Error publishing IMU sample: " << e.what() << "\n";
            }
        }
        // Update last_id to newest motor entry we just processed
        last_id = entries.front().first;
    }

    bool RedisSingleIMUController::is_ready_for_measurements() const {
        // IMU must be created & initialized, calibration must be done,
        // and sampling must be active (producer + consumer running).
        return icm20948 != nullptr
               && calibration_loaded_
               && sampling_active_;
    }

    void RedisSingleIMUController::startConsumer() {
        if (consumer_running_) {
            return;
        }
        consumer_running_ = true;

        consumer_thread_ = std::jthread([this](std::stop_token st) {
            // Subscribe to "sync:data:ready" keyspace notifications.
            // We assume the same helper as for motors: make_keyspace_subscriber(redis_, key_name).
            auto subscriber =
                    exoskeleton::redis_tools::make_keyspace_subscriber(redis_, "sync:data:ready");

            subscriber.on_message([this, &st](const std::string &channel,
                                              const std::string &msg) {
                if (st.stop_requested()) {
                    return;
                }

                if (msg != "set") {
                    return; // we only care about SET events
                }

                // Read frame time from Redis key "sync:data:ready"
                auto const t_str = redis_.get("sync:data:ready");
                if (!t_str) {
                    return; // no valid timestamp
                }


                std::int64_t frame_time_ns = 0;
                try {
                    frame_time_ns = std::stoll(*t_str);

                    // frame_time_ns = std::stoull(trimmed);
                } catch (std::exception &e) {
                    std::cerr << "HELO: " << e.what() << std::endl;
                    // Malformed timestamp; skip this event
                    return;
                }
                // Convert int64 ns → steady_clock::time_point
                std::chrono::steady_clock::time_point frame_time{std::chrono::nanoseconds{frame_time_ns}};
                // Handle this frame: select best IMU sample <= frame_time_ns and publish
                //this->onFrameStoreIMU(frame_time);
            });

            // Main event loop: block on keyspace notifications
            while (!st.stop_requested()) {
                try {
                    subscriber.consume(); // waits for next keyspace event
                } catch (const std::exception &e) {
                    std::cerr << "[IMUController] Consumer subscriber exception: " << e.what() << "\n";
                    // Optionally log error and break; for now we break.
                    break;
                }
                catch (...) {
                    std::cerr << "[IMUController] Consumer subscriber unknown exception\n";
                    break;
                }
            }


            consumer_running_ = false;
        });
    }


    void RedisSingleIMUController::stopConsumer() {
        if (!consumer_running_)
            return;


        // Wake up the consumer if it's blocked on wait_dequeue
        try {
            sample_queue_.enqueue(ImuSample());
        } catch (...) {
        }

        if (consumer_thread_.joinable()) {
            consumer_thread_.request_stop();
            consumer_thread_.join();
        }
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

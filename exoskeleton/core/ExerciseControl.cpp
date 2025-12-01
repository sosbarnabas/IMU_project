#include "ExerciseControl.h"

namespace exoskeleton::core
{
    ExerciseController::ExerciseController(sw::redis::Redis& redis,
                                           Control& control,
                                           std::mutex& redis_mutex)
        : redis_(redis)
          , redis_mutex_(redis_mutex)
          , control_(control),
          exercise_{}
    {
    }

    // Called from processCommand("exercise", ...)
    void ExerciseController::set_exercise(int channel, int exercise_num, int exercise_param, int cooldown_ms)
    {
        exercise_.active = true;
        exercise_.channel = channel;
        exercise_.exercise_num = exercise_num;
        exercise_.exercise_param = exercise_param;
        exercise_.current_slot = -1;


        exercise_.cooldown_ms = cooldown_ms;
        exercise_.below_threshold_since_ns = -1;

        // Decide mode: 0 = existing velocity-based, 1 = new angle-based exercise
        //exercise_.mode = (exercise_num == 1) ? 1 : 0;

        // Angle-based specific init
        exercise_.elbow_motor_id = -1;
        exercise_.angle_initialized = false;
        exercise_.elbow_zeroed = false;
        exercise_.roll_zero_deg = 0.0;

        {
            load_devices_from_runaddrs();

            try
            {
                int e_ext_id = -1;
                auto it = exercise_.active_motors.find("e_ext");
                if (it != exercise_.active_motors.end())
                {
                    e_ext_id = it->second;
                    exercise_.elbow_motor_id = e_ext_id;
                }
            }
            catch (std::exception& e) { qDebug() << e.what(); }
        }


        qDebug() << "[IMU] Exercise mode enabled:"
            << "exercise_num" << exercise_num

            << "param" << exercise_param
            << "cooldown_ms" << cooldown_ms
            << "elbow_motor_id" << exercise_.elbow_motor_id;
    }

    void ExerciseController::start_strength_exercise(int channel, int torque, int sets, int reps, int muscle_type)
    {
        exercise_.active = true;

        exercise_.channel = channel;
        exercise_.exercise_num = 2;
        exercise_.current_slot = -1;

        exercise_.strength_torque = torque;
        exercise_.strength_sets = sets;
        exercise_.strength_reps = reps;
        exercise_.strength_muscle_type = muscle_type;

        exercise_.current_set = 1;
        exercise_.current_rep = 0;

        exercise_.strength_initialized = false;
        exercise_.in_rep = false;
        exercise_.rep_peak_speed = 0.0;
        exercise_.rep_max_angle = 0.0;
        exercise_.last_rep_end_ns = 0;

        exercise_.baseline_count = 0;
        exercise_.baseline_speed = 0.0;
        exercise_.baseline_angle = 0.0;
        exercise_.baseline_rest_ms = 0.0;
        exercise_.fatigue_index = 0.0;


        load_devices_from_runaddrs();

        const std::string ts_str = std::to_string(0); // or real timestamp

        for (const auto& [motor_name, motor_id] : exercise_.active_motors)
        {
            int slot = 0;

            if (muscle_type == 0)
            {
                // biceps: e_ext → slot 1, everyone else 0
                if (motor_name == "e_ext")
                    slot = 1;
            }
            else
            {
                // triceps: e_flex → slot 1, everyone else 0
                if (motor_name == "e_flex")
                    slot = 1;
            }

            const std::string cmd =
                ts_str + "|fn_select|" +
                std::to_string(motor_id) + "|" +
                std::to_string(slot);

            const std::string motor_key = "command:" + std::to_string(motor_id);
            qDebug() << "LPUSH:" << motor_key << cmd;
            redis_.lpush(motor_key, cmd);
        }


        qDebug() << "[Exercise] Strength exercise started:"
            << "torque" << torque
            << "sets" << sets
            << "reps" << reps
            << "muscle_type" << muscle_type;
    }


    // Called from IMU loop after update_imu_state()
    void ExerciseController::updateExerciseFromIMU(const ImuSample& sample, std::int64_t imu_t_ns)
    {
        if (!exercise_.active)
            return;
        if (exercise_.active_motors.empty())
            return;
        if (exercise_.exercise_num == 0)
        {
            double roll_v, pitch_v, yaw_v;
            if (!control_.get_imu_gyro_velocity(sample.imu_id, roll_v, pitch_v, yaw_v))
                return;

            const double v = roll_v; // use roll velocity; change axis if needed
            const int cooldown_ms = exercise_.cooldown_ms;
            const std::int64_t now_ns = imu_t_ns;
            int desired_slot = exercise_.current_slot;
            // Slot selection: |v| > exercise_param -> slot 1, else slot 0

            if (std::abs(v) > exercise_.exercise_param)
            {
                exercise_.below_threshold_since_ns = -1;
                desired_slot = 1;
            }
            else
            {
                // Below threshold
                if (exercise_.current_slot == 1 && cooldown_ms > 0)
                {
                    // We are in slot 1 and must wait cooldown_ms below threshold before going back to 0
                    if (exercise_.below_threshold_since_ns < 0)
                    {
                        // start cooldown timer
                        exercise_.below_threshold_since_ns = now_ns;
                        return; // do not switch yet
                    }

                    const std::int64_t dt_ns = now_ns - exercise_.below_threshold_since_ns;
                    const double dt_ms = static_cast<double>(dt_ns) * 1e-6;

                    if (dt_ms >= cooldown_ms)
                    {
                        desired_slot = 0;
                        exercise_.below_threshold_since_ns = -1;
                    }
                    else
                    {
                        // still in cooldown window, stay in slot 1
                        return;
                    }
                }
                else
                {
                    // cooldown_ms == 0 or we are already in slot 0/-1:
                    desired_slot = 0;
                    exercise_.below_threshold_since_ns = -1;
                }
            }

            // Only act if slot changed
            if (desired_slot == exercise_.current_slot)
                return;

            exercise_.current_slot = desired_slot;

            // Build and send function slot select command to Redis
            // You know the exact format; here is a typical pattern:
            // |timestamp|fn_select|0|[desired_slot]
            std::string ts_str = std::to_string(imu_t_ns); // or other timestamp


            for (const auto& [motor_name, motor_id] : exercise_.active_motors)
            {
                const std::string cmd =
                    ts_str + "|fn_select|" +
                    std::to_string(motor_id) + "|" +
                    std::to_string(desired_slot);

                const std::string motor_key = "command:" + std::to_string(motor_id);
                redis_.lpush(motor_key, cmd);
            }

            if (desired_slot == 0) { redis_tools::send_ok(redis_, "commandres:exercise", "slow"); }
            else { redis_tools::send_ok(redis_, "commandres:exercise", "fast"); }

            qDebug() << "[IMU] Exercise fn_select sent:"
                << "slot" << desired_slot
                << "roll_v[deg/s]" << v
                << "motors";
        }
        else if (exercise_.exercise_num == 1)
        {
            // 1) On first sample after exercise start: capture zero and "zero" motors/IMU
            if (!exercise_.angle_initialized)
            {
                exercise_.roll_zero_deg = sample.euler.at(0); // use current roll as reference
                exercise_.angle_initialized = true;


                std::string ts_str = std::to_string(imu_t_ns); // or other timestamp

                for (const auto& [motor_name, motor_id] : exercise_.active_motors)
                {
                    const std::string cmd =
                        ts_str + "|zero|" +
                        std::to_string(motor_id) + "|";
                    const std::string motor_key = "command:" + std::to_string(motor_id);
                    redis_.lpush(motor_key, cmd);
                }
                const std::string imu_zero_cmd = ts_str + "|zero|0|";
                const std::string imu_zero_key = "command:imu:0";
                redis_.lpush(imu_zero_key, imu_zero_cmd);

                qDebug() << "[IMU] Angle exercise initialized. roll_zero_deg ="
                    << exercise_.roll_zero_deg;

                // No further logic on this first sample
                return;
            }


            const double roll_deg = sample.euler.at(0);

            const double target_angle = static_cast<double>(exercise_.exercise_param);

            // 3) When target angle reached (or exceeded) and not yet zeroed → zero elbow flexor
            if (!exercise_.elbow_zeroed && abs(roll_deg - target_angle) < 0.5)
            {
                std::string ts_str = std::to_string(imu_t_ns); // or other timestamp

                for (const auto& [motor_name, motor_id] : exercise_.active_motors)
                {
                    if (motor_name == "e_flex")
                    {
                        const std::string cmd =
                            ts_str + "|zero|" +
                            std::to_string(motor_id) + "|";
                        const std::string motor_key = "command:" + std::to_string(motor_id);
                        redis_.lpush(motor_key, cmd);
                        redis_tools::send_ok(redis_, "commandres:exercise", "zero");
                        exercise_.elbow_zeroed = true;

                        qDebug() << "[IMU] Elbow flexor zeroed at roll_rel_deg ="
                            << roll_deg
                            << "target_angle =" << target_angle;
                    }
                }
            }


            // This exercise has no slot logic; once elbow is zeroed, you can
            // leave it active or add an auto-finish flag if you want.
            return;
        }
        else if (exercise_.exercise_num == 2)
        {
            if (exercise_.current_set > exercise_.strength_sets)
                return; // exercise logically finished

            // 1) Initialize on first sample: zero motors and IMU
            if (!exercise_.strength_initialized)
            {
                std::string ts_str = std::to_string(imu_t_ns); // or other timestamp
                for (const auto& [motor_name, motor_id] : exercise_.active_motors)
                {
                    const std::string cmd =
                        ts_str + "|zero|" +
                        std::to_string(motor_id) + "|";
                    const std::string motor_key = "command:" + std::to_string(motor_id);
                    redis_.lpush(motor_key, cmd);
                }
                const std::string imu_zero_cmd = ts_str + "|zero|0|";
                const std::string imu_zero_key = "command:imu:0";
                redis_.lpush(imu_zero_key, imu_zero_cmd);


                exercise_.roll_zero_deg = sample.euler.at(0);
                exercise_.strength_initialized = true;
                exercise_.in_rep = false;
                exercise_.rep_peak_speed = 0.0;
                exercise_.rep_max_angle = 0.0;
                return;
            }

            // 2) Get kinematics
            double roll_v, pitch_v, yaw_v;
            if (!control_.get_imu_fused_velocity(sample.imu_id, roll_v, pitch_v, yaw_v))
                return;

            const double roll_deg = sample.euler.at(0);
            const double roll_rel_deg = roll_deg - exercise_.roll_zero_deg;
            const double abs_v = std::abs(roll_v);

            // Simple thresholds for movement vs rest
            constexpr double V_START = 20.0; // deg/s
            constexpr double V_END = 5.0; // deg/s

            const std::int64_t now_ns = imu_t_ns;

            // 3) Update max speed & angle during rep
            if (exercise_.in_rep)
            {
                if (abs_v > exercise_.rep_peak_speed)
                    exercise_.rep_peak_speed = abs_v;

                const double abs_angle = std::abs(roll_rel_deg);
                if (abs_angle > exercise_.rep_max_angle)
                    exercise_.rep_max_angle = abs_angle;
            }

            // 4) Rep state machine: detect start/end
            if (!exercise_.in_rep)
            {
                // Possibly start new rep
                if (abs_v > V_START)
                {
                    exercise_.in_rep = true;
                    exercise_.rep_start_ns = now_ns;
                    exercise_.rep_peak_speed = abs_v;
                    exercise_.rep_max_angle = std::abs(roll_rel_deg);
                }
            }
            else
            {
                // In rep, check for end
                if (abs_v < V_END)
                {
                    exercise_.in_rep = false;
                    exercise_.rep_end_ns = now_ns;

                    // Compute movement speed (peak speed) & rest
                    const double movement_speed = exercise_.rep_peak_speed; // deg/s

                    double rest_time_ms = 0.0;
                    if (exercise_.last_rep_end_ns > 0)
                    {
                        const std::int64_t dt_rest_ns = exercise_.rep_start_ns - exercise_.last_rep_end_ns;
                        rest_time_ms = static_cast<double>(dt_rest_ns) * 1e-6;
                    }

                    // Log to Redis
                    exercise_.current_rep += 1;
                    const int rep_no = exercise_.current_rep;
                    const int set_no = exercise_.current_set;
                    const double max_angle = exercise_.rep_max_angle;

                    // === Update fatigue based on this rep ===
                    {
                        constexpr int BASELINE_REPS = 4;

                        if (exercise_.baseline_count < BASELINE_REPS)
                        {
                            exercise_.baseline_count++;

                            const double k = 1.0 / exercise_.baseline_count;
                            exercise_.baseline_speed += k * (movement_speed - exercise_.baseline_speed);
                            exercise_.baseline_angle += k * (max_angle - exercise_.baseline_angle);

                            if (rest_time_ms > 0.0)
                            {
                                exercise_.baseline_rest_ms += k * (rest_time_ms - exercise_.baseline_rest_ms);
                            }

                            exercise_.fatigue_index = 0.0;
                        }
                        else
                        {
                            const double speed_ratio = (exercise_.baseline_speed > 1e-6)
                                                           ? movement_speed / exercise_.baseline_speed
                                                           : 1.0;

                            const double angle_ratio = (exercise_.baseline_angle > 1e-6)
                                                           ? max_angle / exercise_.baseline_angle
                                                           : 1.0;

                            const double rest_ratio = (exercise_.baseline_rest_ms > 1e-3 && rest_time_ms > 0.0)
                                                          ? rest_time_ms / exercise_.baseline_rest_ms
                                                          : 1.0;

                            double f_speed = std::clamp(1.0 - speed_ratio, 0.0, 1.0);
                            double f_angle = std::clamp(1.0 - angle_ratio, 0.0, 1.0);
                            double f_rest = std::clamp(rest_ratio - 1.0, 0.0, 1.0);

                            constexpr double W_SPEED = 0.6;
                            constexpr double W_ANGLE = 0.1;
                            constexpr double W_REST = 0.3;

                            double f = W_SPEED * f_speed + W_ANGLE * f_angle + W_REST * f_rest;

                            constexpr double ALPHA = 0.8;
                            exercise_.fatigue_index = (1.0 - ALPHA) * exercise_.fatigue_index + ALPHA * f;
                        }
                    }

                    // === Log rep result + fatigue ===
                    std::ostringstream oss;
                    oss << "rep:" << rep_no
                        << ";set:" << set_no
                        << ";movement_speed:" << movement_speed
                        << ";rest_speed:" << rest_time_ms
                        << ";arm_max_angle:" << max_angle
                        << ";fatigue:" << exercise_.fatigue_index
                        << ";";

                    redis_.lpush("commandres:exercise", oss.str());
                    // qDebug() << "Exercise log" << oss.str();
                    qDebug() << "Fatique: " << exercise_.fatigue_index;

                    exercise_.last_rep_end_ns = exercise_.rep_end_ns;

                    // Prepare for next rep
                    exercise_.rep_peak_speed = 0.0;
                    exercise_.rep_max_angle = 0.0;

                    // 5) Handle set / exercise completion
                    if (exercise_.current_rep >= exercise_.strength_reps)
                    {
                        exercise_.current_rep = 0;
                        exercise_.current_set += 1;

                        if (exercise_.current_set > exercise_.strength_sets)
                        {
                            // Exercise finished
                            redis_.lpush("commandres:exercise", "exercise:finished");
                            stop_exercise();
                        }
                    }
                }
            }
        }
    }

    void ExerciseController::stop_exercise()
    {
        exercise_.active = false;
        exercise_.finished = true;

        redis_.lpush("commandres:exercise", "exercise:stopped");
    }

    void ExerciseController::load_devices_from_runaddrs()
    {
        std::lock_guard<std::mutex> lock(redis_mutex_);

        exercise_.active_motors.clear();
        exercise_.imu_id = -1;

        std::unordered_map<std::string, std::string> run_addrs;
        redis_.hgetall("run:addrs",
                       std::inserter(run_addrs, run_addrs.begin()));

        for (const auto& kv : run_addrs)
        {
            const std::string& name = kv.first; // e.g. "e_flex", "imu0"
            const std::string& val = kv.second; // e.g. "0|", "3|..."

            auto pipe_pos = val.find('|');
            std::string num_str = (pipe_pos == std::string::npos)
                                      ? val
                                      : val.substr(0, pipe_pos);

            num_str.erase(0, num_str.find_first_not_of(" \t\n\r"));
            num_str.erase(num_str.find_last_not_of(" \t\n\r") + 1);

            int id = -1;
            try { id = std::stoi(num_str); }
            catch (...) { continue; }

            if (name.find("imu") != std::string::npos)
            {
                exercise_.imu_id = id;
            }
            else
            {
                // store motor name → id
                exercise_.active_motors[name] = id;
            }
        }

        qDebug() << "[Exercise] run:addrs loaded:"
            << "imu_id:" << exercise_.imu_id;
        for (const auto& kv : exercise_.active_motors)
        {
            qDebug() << " motor" << QString::fromStdString(kv.first)
                << "id" << kv.second;
        }
    }
}

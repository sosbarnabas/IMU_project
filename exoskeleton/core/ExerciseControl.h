#pragma once

#include <cstdint>
#include <sw/redis++/redis++.h>
#include "ImuSample.h"
#include "RedisTools.h"
#include "Control.h"
#include <thread>
#include <mutex>
#include <map>
#include <QDebug>
#include  "cmath"
namespace exoskeleton::core
{
    struct ExerciseContext
    {
        bool active = false;
        bool finished = false;

        // 0 = velocity-based slot selection (existing logic)
        // 1 = angle-based elbow zero exercise
        // int  mode           = 0;

        int channel = 0;
        int exercise_num = 0; // as received from command
        int exercise_param = 0; // threshold (deg/s) or angle (deg)
        int current_slot = -1;

        //std::vector<int> active_motor_ids;
        std::map<std::string, int> active_motors; // key = motor name, value = id

        // For velocity-based exercise
        int cooldown_ms = 0;
        std::int64_t below_threshold_since_ns = -1;

        // For angle-based elbow exercise
        int elbow_motor_id = -1;
        bool angle_initialized = false;
        bool elbow_zeroed = false;
        double roll_zero_deg = 0.0; // IMU roll at exercise start
        int imu_id = -1;


        // ----- NEW: strength exercise (mode 2) -----
        bool   strength_initialized  = false;
        int    strength_torque       = 0;
        int    strength_sets         = 0;
        int    strength_reps         = 0;
        int    strength_muscle_type  = 0; // b=0=biceps, t=1=triceps

        int    current_set           = 0;
        int    current_rep           = 0;

        bool   in_rep                = false;
        std::int64_t rep_start_ns    = 0;
        std::int64_t rep_end_ns      = 0;
        std::int64_t last_rep_end_ns = 0;

        double rep_peak_speed        = 0.0; // deg/s
        double rep_max_angle         = 0.0; // deg (relative)
        //exercise fatique
        // Strength exercise baselines
        int    baseline_count        = 0;
        double baseline_speed        = 0.0;  // avg movement_speed of first N reps
        double baseline_angle        = 0.0;  // avg arm_max_angle of first N reps
        double baseline_rest_ms      = 0.0;  // avg rest time of first N rests

        double fatigue_index         = 0.0;  // 0..1+ (higher = more tired)

    };

    class Control; // forward declaration
    //class RedisSingleIMUController; // optional if needed

    class ExerciseController
    {
    public:
        ExerciseController(sw::redis::Redis& redis,
                           Control& control,
                           std::mutex& redis_mutex);

        // Called from processCommand("exercise", ...)
        void set_exercise(int channel,
                          int exercise_num,
                          int exercise_param,
                          int cooldown_ms);

        // Called from IMU loop after update_imu_state()
        void updateExerciseFromIMU(const ImuSample& sample, std::int64_t imu_t_ns);

        void start_strength_exercise(int channel,
                                 int torque,
                                 int sets,
                                 int reps,
                                 int muscle_type); // NEW

        void stop_exercise();



    private:
       void load_devices_from_runaddrs();
        // internal persistent state (your ExerciseContext lives here)
        sw::redis::Redis& redis_;
        std::mutex&  redis_mutex_;  // NEW
        Control& control_;
        ExerciseContext exercise_;
    };
} // namespace exoskeleton::core

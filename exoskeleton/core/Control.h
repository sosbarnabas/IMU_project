#pragma once

#include <deque>
#include <unordered_map>
#include <cstddef>
#include <cstdint>

namespace exoskeleton::core
{
    // ------------------------ Motor state ------------------------

    struct MotorState
    {
        std::int64_t timestamp_ns; // absolute or relative time in nanoseconds
        double       position_deg; // motor position in degrees
        double       torque;       // motor torque (units as in your system)
    };

    /**
     * Holds a finite history of samples for one motor and computes velocity
     * from the samples via finite difference over a configurable window.
     */
    class MotorHistory
    {
    public:
        explicit MotorHistory(std::size_t window_size = 5);

        // Append a new sample; oldest samples are dropped once window_size is exceeded
        void push_sample(const MotorState& s);

        // Compute velocity [deg/s] using first and last samples in the buffer
        // Returns false if not enough data (fewer than 2 samples or zero dt)
        bool compute_motor_velocity(double& vel_deg_s) const;

        // Pointer to latest sample or nullptr if none
        const MotorState* latest() const;

        std::size_t size() const { return buffer_.size(); }

    private:
        std::size_t           window_size_;
        std::deque<MotorState> buffer_;
    };

    // ------------------------ IMU state --------------------------

    struct IMUState
    {
        std::int64_t timestamp_ns; // time in nanoseconds

        // Orientation (e.g. in degrees)
        double euler_roll_deg;
        double euler_pitch_deg;
        double euler_yaw_deg;

        // Angular velocities from gyro [deg/s]
        double gyro_roll_dps;
        double gyro_pitch_dps;
        double gyro_yaw_dps;
    };

    /**
     * Holds a finite history of IMU samples and computes angular velocities
     * in roll, pitch and yaw using Euler differences and/or gyro data.
     */
    class IMUHistory
    {
    public:
        explicit IMUHistory(std::size_t window_size = 5, double gyro_weight = 0.7);

        void push_sample(const IMUState& s);

        // Euler-based angular velocities [deg/s]
        bool compute_euler_velocity(double& roll_vel,
                                    double& pitch_vel,
                                    double& yaw_vel) const;

        // Gyro-based velocities (averaged gyro) [deg/s]
        bool compute_gyro_velocity(double& roll_vel,
                                   double& pitch_vel,
                                   double& yaw_vel) const;

        // Fused velocities: gyro_weight * gyro + (1 - gyro_weight) * euler_diff
        bool compute_fused_velocity(double& roll_vel,
                                    double& pitch_vel,
                                    double& yaw_vel) const;

        const IMUState* latest() const;

        std::size_t size() const { return buffer_.size(); }

        double gyro_weight() const { return gyro_weight_; }

    private:
        std::size_t         window_size_;
        double              gyro_weight_;
        std::deque<IMUState> buffer_;
    };

    // ------------------------ Control context --------------------

    /**
     * High–level control context that stores the latest motor and IMU data
     * and exposes velocities computed over a configurable history length.
     *
     * You can later extend this with real control laws (PD, impedance, etc.).
     */
    class Control
    {
    public:
        explicit Control(std::size_t vel_window_size = 5, double imu_gyro_weight = 0.7);

        // ----- Motor interface -----

        void update_motor_state(int motor_id,
                                std::int64_t timestamp_ns,
                                double position_deg,
                                double torque);

        // Get current velocity estimate for motor_id [deg/s]
        // Returns false if not enough data
        bool get_motor_velocity(int motor_id, double& vel_deg_s) const;

        const MotorState* get_latest_state(int motor_id) const;

        // ----- IMU interface -----

        void update_imu_state(int imu_id,
                              std::int64_t timestamp_ns,
                              double euler_roll_deg,
                              double euler_pitch_deg,
                              double euler_yaw_deg,
                              double gyro_roll_dps,
                              double gyro_pitch_dps,
                              double gyro_yaw_dps);

        // Euler-based velocities [deg/s]
        bool get_imu_euler_velocity(int imu_id,
                                    double& roll_vel,
                                    double& pitch_vel,
                                    double& yaw_vel) const;

        // Gyro-based velocities [deg/s]
        bool get_imu_gyro_velocity(int imu_id,
                                   double& roll_vel,
                                   double& pitch_vel,
                                   double& yaw_vel) const;

        // Fused velocities [deg/s]
        bool get_imu_fused_velocity(int imu_id,
                                    double& roll_vel,
                                    double& pitch_vel,
                                    double& yaw_vel) const;

        const IMUState* get_latest_imu_state(int imu_id) const;

        // Optionally change window at runtime (resets histories)
        void set_velocity_window_size(std::size_t window_size);

    private:
        std::unordered_map<int, MotorHistory> motors_;
        std::unordered_map<int, IMUHistory>   imus_;

        std::size_t vel_window_size_;
        double      imu_gyro_weight_;
    };

} // namespace exoskeleton::core

#pragma once

#include <deque>
#include <unordered_map>
#include <cstddef>

namespace exoskeleton::core
{
    // Single motor sample (you can extend this with more fields if needed)
    struct MotorState
    {
        double timestamp_s;   // absolute or relative time in seconds
        double position_deg;  // motor position in degrees
        double torque;        // motor torque (units as in your system)
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
        bool compute_velocity(double& vel_deg_s) const;

        // Pointer to latest sample or nullptr if none
        const MotorState* latest() const;

        std::size_t size() const { return buffer_.size(); }

    private:
        std::size_t window_size_;
        std::deque<MotorState> buffer_;
    };

    /**
     * High–level control context that stores the latest motor data and exposes
     * per-motor velocities computed over a configurable history length.
     *
     * You can later extend this with real control laws (PD, impedance, etc.).
     */
    class Control
    {
    public:
        explicit Control(std::size_t vel_window_size = 5);

        // Add a new sample for this motor
        void update_motor_state(int motor_id,
                                double timestamp_s,
                                double position_deg,
                                double torque);

        // Get current velocity estimate for motor_id [deg/s]
        // Returns false if not enough data
        bool get_motor_velocity(int motor_id, double& vel_deg_s) const;

        // Get latest stored state for this motor; nullptr if none
        const MotorState* get_latest_state(int motor_id) const;

        // Optionally change window at runtime (resets histories)
        void set_velocity_window_size(std::size_t window_size);

    private:
        std::unordered_map<int, MotorHistory> motors_;
        std::size_t vel_window_size_;
    };

} // namespace exoskeleton::core

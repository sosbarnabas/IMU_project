#include "Control.h"

namespace exoskeleton::core
{
    // ===================== MotorHistory =====================

    MotorHistory::MotorHistory(std::size_t window_size)
        : window_size_{window_size}
    {
        if (window_size_ < 2) {
            window_size_ = 2; // need at least 2 samples to compute velocity
        }
    }

    void MotorHistory::push_sample(const MotorState& s)
    {
        buffer_.push_back(s);

        // Keep only the last window_size_ samples
        while (buffer_.size() > window_size_) {
            buffer_.pop_front();
        }
    }

    bool MotorHistory::compute_velocity(double& vel_deg_s) const
    {
        if (buffer_.size() < 2) {
            return false;
        }

        const MotorState& first = buffer_.front();
        const MotorState& last  = buffer_.back();

        const double dt = last.timestamp_s - first.timestamp_s;
        if (dt <= 0.0) {
            return false;
        }

        const double dpos = last.position_deg - first.position_deg;
        vel_deg_s = dpos / dt;
        return true;
    }

    const MotorState* MotorHistory::latest() const
    {
        if (buffer_.empty()) return nullptr;
        return &buffer_.back();
    }

    // ======================== Control =======================

    Control::Control(std::size_t vel_window_size)
        : vel_window_size_{vel_window_size}
    {
        if (vel_window_size_ < 2) {
            vel_window_size_ = 2;
        }
    }

    void Control::update_motor_state(int motor_id,
                                     double timestamp_s,
                                     double position_deg,
                                     double torque)
    {
        MotorState s{timestamp_s, position_deg, torque};

        // lazily create history for this motor
        auto it = motors_.find(motor_id);
        if (it == motors_.end()) {
            it = motors_.emplace(motor_id, MotorHistory{vel_window_size_}).first;
        }

        it->second.push_sample(s);
    }

    bool Control::get_motor_velocity(int motor_id, double& vel_deg_s) const
    {
        auto it = motors_.find(motor_id);
        if (it == motors_.end()) return false;

        return it->second.compute_velocity(vel_deg_s);
    }

    const MotorState* Control::get_latest_state(int motor_id) const
    {
        auto it = motors_.find(motor_id);
        if (it == motors_.end()) return nullptr;

        return it->second.latest();
    }

    void Control::set_velocity_window_size(std::size_t window_size)
    {
        if (window_size < 2) window_size = 2;
        vel_window_size_ = window_size;

        // Recreate histories so their buffer limit matches the new window size
        for (auto& [id, hist] : motors_) {
            hist = MotorHistory{vel_window_size_};
        }
    }

} // namespace exoskeleton::core

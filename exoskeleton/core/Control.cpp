#include "Control.h"

#include <qlogging.h>
#include <QDebug>

namespace exoskeleton::core
{
    // Helper: convert nanoseconds difference to seconds
    static inline double ns_to_seconds(std::int64_t dt_ns)
    {
        return static_cast<double>(dt_ns) * 1e-9;
    }

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

        const std::int64_t dt_ns = last.timestamp_ns - first.timestamp_ns;
        if (dt_ns <= 0) {
            return false;
        }

        const double dt_s  = ns_to_seconds(dt_ns);
        const double dpos  = last.position_deg - first.position_deg;
        vel_deg_s          = dpos / dt_s;

        // qDebug() << "Motor v: dpos" << dpos << "dt[s]" << dt_s << "v[deg/s]" << vel_deg_s;
        return true;
    }

    const MotorState* MotorHistory::latest() const
    {
        if (buffer_.empty()) return nullptr;
        return &buffer_.back();
    }

    // ====================== IMUHistory =======================

    IMUHistory::IMUHistory(std::size_t window_size, double gyro_weight)
        : window_size_{window_size}
        , gyro_weight_{gyro_weight}
    {
        if (window_size_ < 2) {
            window_size_ = 2;
        }
        if (gyro_weight_ < 0.0) gyro_weight_ = 0.0;
        if (gyro_weight_ > 1.0) gyro_weight_ = 1.0;
    }

    void IMUHistory::push_sample(const IMUState& s)
    {
        buffer_.push_back(s);

        while (buffer_.size() > window_size_) {
            buffer_.pop_front();
        }
    }

    bool IMUHistory::compute_euler_velocity(double& roll_vel,
                                            double& pitch_vel,
                                            double& yaw_vel) const
    {
        if (buffer_.size() < 2) {
            return false;
        }

        const IMUState& first = buffer_.front();
        const IMUState& last  = buffer_.back();

        const std::int64_t dt_ns = last.timestamp_ns - first.timestamp_ns;
        if (dt_ns <= 0) {
            return false;
        }

        const double dt_s = ns_to_seconds(dt_ns);

        const double d_roll  = last.euler_roll_deg  - first.euler_roll_deg;
        const double d_pitch = last.euler_pitch_deg - first.euler_pitch_deg;
        const double d_yaw   = last.euler_yaw_deg   - first.euler_yaw_deg;

        roll_vel  = d_roll  / dt_s;
        pitch_vel = d_pitch / dt_s;
        yaw_vel   = d_yaw   / dt_s;

        // qDebug() << "IMU Euler v: roll" << roll_vel << "pitch" << pitch_vel << "yaw" << yaw_vel;
        return true;
    }

    bool IMUHistory::compute_gyro_velocity(double& roll_vel,
                                           double& pitch_vel,
                                           double& yaw_vel) const
    {
        if (buffer_.empty()) {
            return false;
        }

        double sum_roll  = 0.0;
        double sum_pitch = 0.0;
        double sum_yaw   = 0.0;

        for (const auto& s : buffer_) {
            sum_roll  += s.gyro_roll_dps;
            sum_pitch += s.gyro_pitch_dps;
            sum_yaw   += s.gyro_yaw_dps;
        }

        const double inv_n = 1.0 / static_cast<double>(buffer_.size());
        roll_vel  = sum_roll  * inv_n;
        pitch_vel = sum_pitch * inv_n;
        yaw_vel   = sum_yaw   * inv_n;

        // qDebug() << "IMU Gyro v(avg): roll" << roll_vel << "pitch" << pitch_vel << "yaw" << yaw_vel;
        return true;
    }

    bool IMUHistory::compute_fused_velocity(double& roll_vel,
                                            double& pitch_vel,
                                            double& yaw_vel) const
    {
        double e_roll, e_pitch, e_yaw;
        double g_roll, g_pitch, g_yaw;

        const bool ok_e = compute_euler_velocity(e_roll, e_pitch, e_yaw);
        const bool ok_g = compute_gyro_velocity(g_roll, g_pitch, g_yaw);

        if (!ok_e && !ok_g) {
            return false;
        }

        // If one of them is missing, fallback to the other
        if (!ok_e) {
            roll_vel  = g_roll;
            pitch_vel = g_pitch;
            yaw_vel   = g_yaw;
            return true;
        }
        if (!ok_g) {
            roll_vel  = e_roll;
            pitch_vel = e_pitch;
            yaw_vel   = e_yaw;
            return true;
        }

        const double w  = gyro_weight_;
        const double w2 = 1.0 - gyro_weight_;

        roll_vel  = w * g_roll  + w2 * e_roll;
        pitch_vel = w * g_pitch + w2 * e_pitch;
        yaw_vel   = w * g_yaw   + w2 * e_yaw;

        // qDebug() << "IMU fused v: roll" << roll_vel << "pitch" << pitch_vel << "yaw" << yaw_vel;
        return true;
    }

    const IMUState* IMUHistory::latest() const
    {
        if (buffer_.empty()) return nullptr;
        return &buffer_.back();
    }

    // ======================== Control =======================

    Control::Control(std::size_t vel_window_size, double imu_gyro_weight)
        : vel_window_size_{vel_window_size}
        , imu_gyro_weight_{imu_gyro_weight}
    {
        if (vel_window_size_ < 2) {
            vel_window_size_ = 2;
        }
        if (imu_gyro_weight_ < 0.0) imu_gyro_weight_ = 0.0;
        if (imu_gyro_weight_ > 1.0) imu_gyro_weight_ = 1.0;
    }

    // ----- Motor -----

    void Control::update_motor_state(int motor_id,
                                     std::int64_t timestamp_ns,
                                     double position_deg,
                                     double torque)
    {
        MotorState s{timestamp_ns, position_deg, torque};

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

    // ----- IMU -----

    void Control::update_imu_state(int imu_id,
                                   std::int64_t timestamp_ns,
                                   double euler_roll_deg,
                                   double euler_pitch_deg,
                                   double euler_yaw_deg,
                                   double gyro_roll_dps,
                                   double gyro_pitch_dps,
                                   double gyro_yaw_dps)
    {
        IMUState s{
            timestamp_ns,
            euler_roll_deg,
            euler_pitch_deg,
            euler_yaw_deg,
            gyro_roll_dps,
            gyro_pitch_dps,
            gyro_yaw_dps
        };

        auto it = imus_.find(imu_id);
        if (it == imus_.end()) {
            it = imus_.emplace(imu_id, IMUHistory{vel_window_size_, imu_gyro_weight_}).first;
        }

        it->second.push_sample(s);
    }

    bool Control::get_imu_euler_velocity(int imu_id,
                                         double& roll_vel,
                                         double& pitch_vel,
                                         double& yaw_vel) const
    {
        auto it = imus_.find(imu_id);
        if (it == imus_.end()) return false;

        return it->second.compute_euler_velocity(roll_vel, pitch_vel, yaw_vel);
    }

    bool Control::get_imu_gyro_velocity(int imu_id,
                                        double& roll_vel,
                                        double& pitch_vel,
                                        double& yaw_vel) const
    {
        auto it = imus_.find(imu_id);
        if (it == imus_.end()) return false;

        return it->second.compute_gyro_velocity(roll_vel, pitch_vel, yaw_vel);
    }

    bool Control::get_imu_fused_velocity(int imu_id,
                                         double& roll_vel,
                                         double& pitch_vel,
                                         double& yaw_vel) const
    {
        auto it = imus_.find(imu_id);
        if (it == imus_.end()) return false;

        return it->second.compute_fused_velocity(roll_vel, pitch_vel, yaw_vel);
    }

    const IMUState* Control::get_latest_imu_state(int imu_id) const
    {
        auto it = imus_.find(imu_id);
        if (it == imus_.end()) return nullptr;

        return it->second.latest();
    }

    void Control::set_velocity_window_size(std::size_t window_size)
    {
        if (window_size < 2) window_size = 2;
        vel_window_size_ = window_size;

        // Recreate motor histories so their buffer limit matches the new window size
        for (auto& [id, hist] : motors_) {
            hist = MotorHistory{vel_window_size_};
        }

        // Recreate IMU histories as well (keep same gyro weight)
        for (auto& [id, hist] : imus_) {
            hist = IMUHistory{vel_window_size_, imu_gyro_weight_};
        }
    }

} // namespace exoskeleton::core

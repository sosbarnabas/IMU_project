#include "Control.h"

#include <qlogging.h>
#include <QDebug>

namespace exoskeleton::core {
    // Helper: convert nanoseconds difference to seconds
    static inline double ns_to_seconds(std::int64_t dt_ns) {
        return static_cast<double>(dt_ns) * 1e-9;
    }

    // ===================== MotorHistory =====================

    MotorHistory::MotorHistory(std::size_t window_size)
        : window_size_{window_size} {
        if (window_size_ < 2) {
            window_size_ = 2; // need at least 2 samples to compute velocity
        }
    }

    void MotorHistory::push_sample(const MotorState &s) {
        buffer_.push_back(s);

        // Keep only the last window_size_ samples
        while (buffer_.size() > window_size_) {
            buffer_.pop_front();
        }
    }

    bool MotorHistory::compute_motor_velocity(double &vel_deg_s) const {
        const std::size_t N = buffer_.size();
        if (N < 2) {
            return false;
        }

        const MotorState &first = buffer_.front();
        const MotorState &last = buffer_.back();

        const std::int64_t dt_ns_total = last.timestamp_ns - first.timestamp_ns;
        if (dt_ns_total <= 0) {
            return false;
        }

        // Time origin at the first sample to keep numbers small and stable
        // ti = time since first sample [s]
        double sum_t = 0.0;
        double sum_p = 0.0;
        double sum_tt = 0.0;
        double sum_tp = 0.0;

        for (const auto &s: buffer_) {
            const std::int64_t dt_ns = s.timestamp_ns - first.timestamp_ns;
            // In case of a corrupted timestamp, skip non-monotonic entries
            if (dt_ns < 0) {
                continue;
            }

            const double t = ns_to_seconds(dt_ns); // [s]
            const double p = s.position_deg; // [deg]

            sum_t += t;
            sum_p += p;
            sum_tt += t * t;
            sum_tp += t * p;
        }

        // Effective number of valid samples (in case some were skipped)
        // If you never expect negative dt_ns, this is just N.
        const double n = static_cast<double>(N);

        const double denom = n * sum_tt - sum_t * sum_t;
        if (std::abs(denom) < 1e-12) {
            // Fallback: simple end-to-end finite difference
            const double dt_s = ns_to_seconds(dt_ns_total);
            if (dt_s <= 0.0) {
                return false;
            }
            const double dpos = last.position_deg - first.position_deg;
            vel_deg_s = dpos / dt_s;
            return true;
        }

        // Least-squares slope of position vs. time: this is your velocity [deg/s]
        vel_deg_s = (n * sum_tp - sum_t * sum_p) / denom;
        return true;
    }


    const MotorState *MotorHistory::latest() const {
        if (buffer_.empty()) return nullptr;
        return &buffer_.back();
    }

    // ====================== IMUHistory =======================

    IMUHistory::IMUHistory(std::size_t window_size, double gyro_weight)
        : window_size_{window_size}
          , gyro_weight_{gyro_weight} {
        if (window_size_ < 2) {
            window_size_ = 2;
        }
        if (gyro_weight_ < 0.0) gyro_weight_ = 0.0;
        if (gyro_weight_ > 1.0) gyro_weight_ = 1.0;
    }

    void IMUHistory::push_sample(const IMUState &s) {
        buffer_.push_back(s);

        while (buffer_.size() > window_size_) {
            buffer_.pop_front();
        }
    }

    bool IMUHistory::compute_euler_velocity(double &roll_vel,
                                            double &pitch_vel,
                                            double &yaw_vel) const {
        const std::size_t N = buffer_.size();
        if (N < 2) {
            return false;
        }

        const IMUState &first = buffer_.front();

        // Time-related sums (shared across all axes)
        double sum_t = 0.0;
        double sum_tt = 0.0;

        // Angle-related sums for each axis (we'll do a separate linear fit per axis)
        double sum_r = 0.0, sum_tr = 0.0; // roll
        double sum_p = 0.0, sum_tp = 0.0; // pitch
        double sum_y = 0.0, sum_ty = 0.0; // yaw

        // To handle wrapping (e.g. 179° → -179°), do simple angle unwrapping
        double base_roll = first.euler_roll_deg;
        double base_pitch = first.euler_pitch_deg;
        double base_yaw = first.euler_yaw_deg;

        double prev_roll = base_roll;
        double prev_pitch = base_pitch;
        double prev_yaw = base_yaw;

        double acc_roll = 0.0;
        double acc_pitch = 0.0;
        double acc_yaw = 0.0;

        std::size_t valid_n = 0;

        for (const auto &s: buffer_) {
            const std::int64_t dt_ns = s.timestamp_ns - first.timestamp_ns;
            if (dt_ns < 0) {
                // Skip corrupted / non-monotonic timestamps
                continue;
            }

            const double t = ns_to_seconds(dt_ns); // [s]

            // Raw wrapped angles
            double r = s.euler_roll_deg;
            double p = s.euler_pitch_deg;
            double y = s.euler_yaw_deg;

            // Unwrap roll
            double dr = r - prev_roll;
            while (dr > 180.0) dr -= 360.0;
            while (dr < -180.0) dr += 360.0;
            acc_roll += dr;
            prev_roll = r;
            const double r_unwrapped = base_roll + acc_roll;

            // Unwrap pitch
            double dp = p - prev_pitch;
            while (dp > 180.0) dp -= 360.0;
            while (dp < -180.0) dp += 360.0;
            acc_pitch += dp;
            prev_pitch = p;
            const double p_unwrapped = base_pitch + acc_pitch;

            // Unwrap yaw
            double dy = y - prev_yaw;
            while (dy > 180.0) dy -= 360.0;
            while (dy < -180.0) dy += 360.0;
            acc_yaw += dy;
            prev_yaw = y;
            const double y_unwrapped = base_yaw + acc_yaw;

            // Accumulate sums for linear regression
            sum_t += t;
            sum_tt += t * t;

            sum_r += r_unwrapped;
            sum_tr += t * r_unwrapped;

            sum_p += p_unwrapped;
            sum_tp += t * p_unwrapped;

            sum_y += y_unwrapped;
            sum_ty += t * y_unwrapped;

            ++valid_n;
        }

        if (valid_n < 2) {
            return false;
        }

        const double n = static_cast<double>(valid_n);
        const double denom = n * sum_tt - sum_t * sum_t;

        // Degenerate case: timestamps too close together -> fall back to simple diff
        if (std::abs(denom) < 1e-12) {
            const IMUState &last = buffer_.back();
            const std::int64_t dt_ns_total = last.timestamp_ns - first.timestamp_ns;
            const double dt_s = ns_to_seconds(dt_ns_total);
            if (dt_s <= 0.0) {
                return false;
            }

            const double d_roll = last.euler_roll_deg - first.euler_roll_deg;
            const double d_pitch = last.euler_pitch_deg - first.euler_pitch_deg;
            const double d_yaw = last.euler_yaw_deg - first.euler_yaw_deg;

            roll_vel = d_roll / dt_s;
            pitch_vel = d_pitch / dt_s;
            yaw_vel = d_yaw / dt_s;
            return true;
        }

        // Least-squares slopes (deg/s)
        roll_vel = (n * sum_tr - sum_t * sum_r) / denom;
        pitch_vel = (n * sum_tp - sum_t * sum_p) / denom;
        yaw_vel = (n * sum_ty - sum_t * sum_y) / denom;

        return true;
    }

    bool IMUHistory::compute_gyro_velocity(double &roll_vel,
                                           double &pitch_vel,
                                           double &yaw_vel) const {
        const std::size_t N = buffer_.size();
        if (N == 0) {
            return false;
        }

        // First pass: compute mean
        double sum_roll = 0.0;
        double sum_pitch = 0.0;
        double sum_yaw = 0.0;

        for (const auto &s: buffer_) {
            sum_roll += s.gyro_roll_dps;
            sum_pitch += s.gyro_pitch_dps;
            sum_yaw += s.gyro_yaw_dps;
        }

        const double inv_n = 1.0 / static_cast<double>(N);
        const double mean_roll = sum_roll * inv_n;
        const double mean_pitch = sum_pitch * inv_n;
        const double mean_yaw = sum_yaw * inv_n;

        // Second pass: compute variance for simple outlier rejection
        double var_roll = 0.0;
        double var_pitch = 0.0;
        double var_yaw = 0.0;

        for (const auto &s: buffer_) {
            const double dr = s.gyro_roll_dps - mean_roll;
            const double dp = s.gyro_pitch_dps - mean_pitch;
            const double dy = s.gyro_yaw_dps - mean_yaw;

            var_roll += dr * dr;
            var_pitch += dp * dp;
            var_yaw += dy * dy;
        }

        var_roll *= inv_n;
        var_pitch *= inv_n;
        var_yaw *= inv_n;

        const double std_roll = std::sqrt(var_roll);
        const double std_pitch = std::sqrt(var_pitch);
        const double std_yaw = std::sqrt(var_yaw);

        // Third pass: recompute mean without strong outliers (> 3σ)
        double refactor_roll = 0.0;
        double refactor_pitch = 0.0;
        double refactor_yaw = 0.0;
        std::size_t used = 0;

        const double k_sigma = 3.0;

        for (const auto &s: buffer_) {
            const double dr = std::abs(s.gyro_roll_dps - mean_roll);
            const double dp = std::abs(s.gyro_pitch_dps - mean_pitch);
            const double dy = std::abs(s.gyro_yaw_dps - mean_yaw);

            // Keep sample if all three axes are within threshold,
            // or if std ≈ 0 (no spread -> all same).
            const bool keep_r = (std_roll < 1e-6) || (dr <= k_sigma * std_roll);
            const bool keep_p = (std_pitch < 1e-6) || (dp <= k_sigma * std_pitch);
            const bool keep_y = (std_yaw < 1e-6) || (dy <= k_sigma * std_yaw);

            if (keep_r && keep_p && keep_y) {
                refactor_roll += s.gyro_roll_dps;
                refactor_pitch += s.gyro_pitch_dps;
                refactor_yaw += s.gyro_yaw_dps;
                ++used;
            }
        }

        if (used == 0) {
            // Fallback: original simple mean
            roll_vel = mean_roll;
            pitch_vel = mean_pitch;
            yaw_vel = mean_yaw;
            return true;
        }

        const double inv_used = 1.0 / static_cast<double>(used);
        roll_vel = refactor_roll * inv_used;
        pitch_vel = refactor_pitch * inv_used;
        yaw_vel = refactor_yaw * inv_used;

        return true;
    }


    bool IMUHistory::compute_fused_velocity(double &roll_vel,
                                            double &pitch_vel,
                                            double &yaw_vel) const {
        double e_roll, e_pitch, e_yaw;
        double g_roll, g_pitch, g_yaw;

        const bool ok_e = compute_euler_velocity(e_roll, e_pitch, e_yaw);
        const bool ok_g = compute_gyro_velocity(g_roll, g_pitch, g_yaw);

        if (!ok_e && !ok_g) {
            return false;
        }

        // If one of them is missing, fallback to the other
        if (!ok_e) {
            roll_vel = g_roll;
            pitch_vel = g_pitch;
            yaw_vel = g_yaw;
            return true;
        }
        if (!ok_g) {
            roll_vel = e_roll;
            pitch_vel = e_pitch;
            yaw_vel = e_yaw;
            return true;
        }

        const double w = gyro_weight_;
        const double w2 = 1.0 - gyro_weight_;

        roll_vel = w * g_roll + w2 * e_roll;
        pitch_vel = w * g_pitch + w2 * e_pitch;
        yaw_vel = w * g_yaw + w2 * e_yaw;

        return true;
    }


    const IMUState *IMUHistory::latest() const {
        if (buffer_.empty()) return nullptr;
        return &buffer_.back();
    }

    // ======================== Control =======================

    Control::Control(std::size_t vel_window_size, double imu_gyro_weight)
        : vel_window_size_{vel_window_size}
          , imu_gyro_weight_{imu_gyro_weight} {
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
                                     double torque) {
        MotorState s{timestamp_ns, position_deg, torque};

        // lazily create history for this motor
        auto it = motors_.find(motor_id);
        if (it == motors_.end()) {
            it = motors_.emplace(motor_id, MotorHistory{vel_window_size_}).first;
        }

        it->second.push_sample(s);
    }

    bool Control::get_motor_velocity(int motor_id, double &vel_deg_s) const {
        auto it = motors_.find(motor_id);
        if (it == motors_.end()) return false;

        return it->second.compute_motor_velocity(vel_deg_s);
    }

    const MotorState *Control::get_latest_state(int motor_id) const {
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
                                   double gyro_yaw_dps) {
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
                                         double &roll_vel,
                                         double &pitch_vel,
                                         double &yaw_vel) const {
        auto it = imus_.find(imu_id);
        if (it == imus_.end()) return false;

        return it->second.compute_euler_velocity(roll_vel, pitch_vel, yaw_vel);
    }

    bool Control::get_imu_gyro_velocity(int imu_id,
                                        double &roll_vel,
                                        double &pitch_vel,
                                        double &yaw_vel) const {
        auto it = imus_.find(imu_id);
        if (it == imus_.end()) return false;

        return it->second.compute_gyro_velocity(roll_vel, pitch_vel, yaw_vel);
    }

    bool Control::get_imu_fused_velocity(int imu_id,
                                         double &roll_vel,
                                         double &pitch_vel,
                                         double &yaw_vel) const {
        auto it = imus_.find(imu_id);
        if (it == imus_.end()) return false;

        return it->second.compute_fused_velocity(roll_vel, pitch_vel, yaw_vel);
    }

    const IMUState *Control::get_latest_imu_state(int imu_id) const {
        auto it = imus_.find(imu_id);
        if (it == imus_.end()) return nullptr;

        return it->second.latest();
    }

    void Control::set_velocity_window_size(std::size_t window_size) {
        if (window_size < 2) window_size = 2;
        vel_window_size_ = window_size;

        // Recreate motor histories so their buffer limit matches the new window size
        for (auto &[id, hist]: motors_) {
            hist = MotorHistory{vel_window_size_};
        }

        // Recreate IMU histories as well (keep same gyro weight)
        for (auto &[id, hist]: imus_) {
            hist = IMUHistory{vel_window_size_, imu_gyro_weight_};
        }
    }
} // namespace exoskeleton::core

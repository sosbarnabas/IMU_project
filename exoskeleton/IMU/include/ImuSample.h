#pragma once

#include <chrono>
#include <array>

struct ImuSample {
    int imu_id{}; // set by device ctor
    uint64_t seq{}; // per-IMU incrementing sequence
    std::chrono::steady_clock::time_point t_host; // timestamp at burst read (or per-sample back-computed)
    // SI units to keep UI simple
    std::array<float, 3> accel{0, 0, 0};
    std::array<float, 3> gyro{0, 0, 0};
    std::array<float, 3> mag{0, 0, 0};
    std::array<float, 3> euler{0, 0, 0};
    uint16_t fifosize;
    uint8_t fifomult;
    // optional status flags
    uint8_t fifo_overflow: 1{};
    uint8_t fifo_underflow: 1{};
    uint8_t mag_ok: 1{};
    uint8_t accel_overflow: 1{};
    uint8_t gyro_overflow: 1{};
    uint8_t reserved: 3{};
};

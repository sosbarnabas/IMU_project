#pragma once
#include "../motor/ExoMotorsInterface.h"
#include <vector>
#include <cmath>
#include <chrono>

namespace exoskeleton::core
{

    /**
     * Mock implementation of ExoMotorsInterface for testing without real hardware.
     * Generates synthetic motor data using sine wave temporal evolution.
     */
    class MockExoMotors : public ExoMotorsInterface
    {
    public:
        explicit MockExoMotors(double dt = 1.0 / 120.0, int n_motors = 7);

        SerialStatus connect() override;
        SerialStatus disconnect() override;
        [[nodiscard]] SerialStatus status() const override;

        SingleMotorData enable(int address) override;
        SingleMotorData raw_enable(int address) override;
        SingleMotorData disable(int address) override;
        SingleMotorData set_zero(int address) override;
        SingleMotorData set_offset(int address, int position) override;

        SingleMotorData upload_function(int address, int slot, const std::vector<int> &function) override;
        SingleMotorData select_function(int address, int slot) override;
        SingleMotorData set_function(int address, const std::vector<int> &function, int slot = 7) override;

        std::vector<SingleMotorData> read() override;
        std::vector<SingleMotorData> read_last() override;

        int n_motors() const override;

    private:
        static constexpr int N_SLOTS = 8;
        static constexpr int FUNCTION_LEN = 360;
        static constexpr int FULL_TURN = 114688;

        double dt_ns_; // sampling period in nanoseconds
        int n_motors_;
        bool connected_;

        // Motor state
        std::vector<bool> enabled_;
        std::vector<int> current_slot_;
        std::vector<int> position_;
        std::vector<int> offset_;
        std::vector<uint64_t> zeroed_time_; // timestamp when set_zero was called
        std::vector<int> cmd_cntr_;
        std::vector<std::vector<std::vector<int>>> functions_; // [address][slot][position]
        std::vector<SingleMotorData> latest_full_read_;

        // Sine wave data
        std::vector<double> wave_;

        /**
         * Generate mock motor data for a single motor.
         * Uses sine wave temporal evolution to simulate motor behavior.
         */
        SingleMotorData read_single(int address);

        void incr_cntr(int address);
    };

} // namespace exoskeleton::core

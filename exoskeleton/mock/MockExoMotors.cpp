#include "MockExoMotors.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace exoskeleton::core
{

    MockExoMotors::MockExoMotors(double dt, int n_motors)
        : dt_ns_(dt * 1e9),
          n_motors_(n_motors),
          connected_(false),
          enabled_(n_motors, false),
          current_slot_(n_motors, 0),
          position_(n_motors, 0),
          offset_(n_motors, 0),
          zeroed_time_(n_motors, 0),
          cmd_cntr_(n_motors, 0),
          functions_(n_motors, std::vector<std::vector<int>>(
                                   N_SLOTS, std::vector<int>(FUNCTION_LEN, 0)))
    {
        // Generate sine wave: 100 samples over 0 to 2π
        wave_.resize(100);
        for (int i = 0; i < 100; ++i)
        {
            wave_[i] = std::sin(2.0 * M_PI * i / 100.0);
        }
    }

    SerialStatus MockExoMotors::connect()
    {
        connected_ = true;
        // Clear functions on connect (like Python version)
        for (int a = 0; a < n_motors_; ++a)
        {
            try
            {
                for (int slot = 0; slot < N_SLOTS; ++slot)
                {
                    functions_[a][slot].assign(FUNCTION_LEN, 0);
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "MockExoMotors::connect: exception clearing functions at #" << a
                          << ": " << e.what() << "\n";
            }
        }
        return status();
    }

    SerialStatus MockExoMotors::disconnect()
    {
        connected_ = false;
        return status();
    }

    SerialStatus MockExoMotors::status() const
    {
        return SerialStatus{
            connected_,
            connected_ ? "MOCK_PORT" : "",
        };
    }

    SingleMotorData MockExoMotors::enable(int address)
    {
        // Implementation of enable: upload weak function, zero, enable, then set offset
        // Similar to SinglePortExoMotor::enable() and Python's ExoMotorsInterface.enable()

        // Create weak function: linear from 10 to -10
        std::vector<int> weak_func(FUNCTION_LEN);
        for (int i = 0; i < FUNCTION_LEN; ++i)
        {
            weak_func[i] = 10 - (20 * i) / (FUNCTION_LEN - 1);
        }

        upload_function(address, 7, weak_func);
        select_function(address, 7);
        set_zero(address);
        raw_enable(address);

        auto data = read();
        return set_offset(address, -data[address].position);
    }

    SingleMotorData MockExoMotors::raw_enable(int address)
    {
        enabled_[address] = true;
        set_zero(address);
        incr_cntr(address);
        return read_single(address);
    }

    SingleMotorData MockExoMotors::disable(int address)
    {
        enabled_[address] = false;
        incr_cntr(address);
        return read_single(address);
    }

    SingleMotorData MockExoMotors::set_zero(int address)
    {
        position_[address] = 0;
        offset_[address] = 0;
        zeroed_time_[address] = std::chrono::steady_clock::now().time_since_epoch().count();
        incr_cntr(address);
        return read_single(address);
    }

    SingleMotorData MockExoMotors::set_offset(int address, int position)
    {
        offset_[address] = position;
        incr_cntr(address);
        return read_single(address);
    }

    SingleMotorData MockExoMotors::upload_function(int address, int slot,
                                                   const std::vector<int> &function)
    {
        if (slot < 0 || slot >= N_SLOTS)
        {
            throw std::out_of_range("Slot out of range");
        }
        if (function.size() != FUNCTION_LEN)
        {
            throw std::invalid_argument("Function length must be " + std::to_string(FUNCTION_LEN));
        }
        functions_[address][slot] = function;
        incr_cntr(address);
        return read_single(address);
    }

    SingleMotorData MockExoMotors::select_function(int address, int slot)
    {
        if (slot < 0 || slot >= N_SLOTS)
        {
            throw std::out_of_range("Slot out of range");
        }
        current_slot_[address] = slot;
        incr_cntr(address);
        return read_single(address);
    }

    SingleMotorData MockExoMotors::set_function(int address, const std::vector<int> &function, int slot)
    {
        upload_function(address, slot, function);
        return select_function(address, slot);
    }

    std::vector<SingleMotorData> MockExoMotors::read()
    {
        auto data = std::vector<SingleMotorData>(n_motors_);
        for (int i = 0; i < n_motors_; ++i)
        {
            data[i] = read_single(i);
        }
        latest_full_read_ = data;
        return data;
    }

    std::vector<SingleMotorData> MockExoMotors::read_last()
    {
        if (latest_full_read_.empty())
        {
            read();
        }
        return latest_full_read_;
    }

    int MockExoMotors::n_motors() const
    {
        return n_motors_;
    }

    SingleMotorData MockExoMotors::read_single(int address)
    {
        uint64_t t = std::chrono::steady_clock::now().time_since_epoch().count();

        // Temporal evolution: sine wave over time
        // Period: 5 seconds = 5e9 nanoseconds (lower frequency for smoother motion)
        double phase = ((t - zeroed_time_[address]) % static_cast<uint64_t>(5e9)) / 5e9 * 100;
        int phase_idx = static_cast<int>(phase) % 100;
        double position_normalized = wave_[phase_idx];
        int position = static_cast<int>(position_normalized * FULL_TURN);

        // Apply offset
        position += offset_[address];

        // Map position to function index [0, 359]
        double fn_index = (position + FULL_TURN / 2) * 360.0 / FULL_TURN;
        if (fn_index < 0)
            fn_index = 0;
        if (fn_index > 359)
            fn_index = 359;

        // Get torque from current function
        int slot = current_slot_[address];
        int torque = functions_[address][slot][static_cast<int>(fn_index)];

        SingleMotorData result;
        result.enabled = enabled_[address];
        result.slot_idx = current_slot_[address];
        result.cmd_cntr = cmd_cntr_[address];
        result.position = position;
        result.torque = torque;
        result.t = t;
        result.n_tries = 0;
        return result;
    }

    void MockExoMotors::incr_cntr(int address)
    {
        cmd_cntr_[address] = (cmd_cntr_[address] + 1) % 8;
    }

} // namespace exoskeleton::core

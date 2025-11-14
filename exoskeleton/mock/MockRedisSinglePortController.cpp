#include "MockRedisSinglePortController.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <optional>

using json = nlohmann::json;

namespace exoskeleton::core
{

    // Redis field names (compatible with GUI expectations, matching RedisKeys.h patterns)
    namespace redis_fields
    {
        constexpr auto ENABLED = "enabled";
        constexpr auto SLOT_INDEX = "slot_idx";
        constexpr auto CMD_COUNTER = "cmd_cntr";
        constexpr auto POSITION = "position";
        constexpr auto TORQUE = "torque";
        constexpr auto TIMESTAMP = "t";
        constexpr auto RETRY_COUNT = "n_tries";
    }

    struct CommandRecord
    {
        long long timestamp = 0;
        std::string command;
        int address = 0;
        std::string parameters;
    };

    namespace {
        std::optional<CommandRecord> parseCommandRecord(const std::string &record)
    {
        const auto first = record.find('|');
        if (first == std::string::npos)
            return std::nullopt;

        const auto second = record.find('|', first + 1);
        if (second == std::string::npos)
            return std::nullopt;

        const auto third = record.find('|', second + 1);
        if (third == std::string::npos)
            return std::nullopt;

        CommandRecord result;
        try
        {
            result.timestamp = std::stoll(record.substr(0, first));
        }
        catch (...)
        {
            result.timestamp = 0;
        }

        result.command = record.substr(first + 1, second - first - 1);

        try
        {
            result.address = std::stoi(record.substr(second + 1, third - second - 1));
        }
        catch (...)
        {
            result.address = 0;
        }

        result.parameters = record.substr(third + 1);
        return result;
    }
    } // namespace

    MockRedisSinglePortController::MockRedisSinglePortController(
        int n_motors, std::shared_ptr<MockExoMotors> mock_motors, const std::string &redis_uri)
        : n_motors_(n_motors), motor_(mock_motors), redis_(redis_uri)
    {
        if (!motor_)
        {
            motor_ = std::make_shared<MockExoMotors>(1.0 / 120.0, n_motors);
        }
    }

    void MockRedisSinglePortController::loop()
    {
        std::cout << "[MockController] Connecting mock motors...\n";
        motor_->connect();
        std::cout << "[MockController] Mock motors connected.\n";

        // Publish address book so GUI can discover motors
        publishAddressBook();
        std::cout << "[MockController] Address book published to Redis.\n";

        std::cout << "[MockController] Starting main loop (GUI-compatible protocol)...\n";
        std::cout << "[MockController] Motor data written to Redis streams: xdata:0, xdata:1, ... \n";
        std::cout << "[MockController] Send commands via: redis-cli RPUSH command:0 '0|enable|0|'\n";

        auto last_data_push = std::chrono::steady_clock::now();
        const auto data_push_interval = std::chrono::milliseconds(100); // Push every 100ms

        while (true)
        {
            try
            {
                // Check exit flag
                if (auto exit_flag = redis_.get("exit"); exit_flag && *exit_flag == "1")
                {
                    std::cout << "[MockController] Exit signal received.\n";
                    break;
                }

                // Process commands for each address
                for (int i = 0; i < n_motors_; ++i)
                {
                    auto key = std::string("command:") + std::to_string(i);
                    auto raw_command = redis_.lpop(key);
                    if (raw_command)
                    {
                        try
                        {
                            processCommand(*raw_command);
                        }
                        catch (const std::exception &e)
                        {
                            std::cerr << "[MockController] Command error: " << e.what() << "\n";
                        }
                    }
                }

                // Continuously push motor data to Redis
                auto now = std::chrono::steady_clock::now();
                if (now - last_data_push >= data_push_interval)
                {
                    measureAndStore();
                    last_data_push = now;
                }

                // Small sleep to prevent busy-waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            catch (const std::exception &e)
            {
                std::cerr << "[MockController] Error in loop: " << e.what() << "\n";
                break;
            }
        }

        std::cout << "[MockController] Stopping...\n";
        motor_->disconnect();
    }

    void MockRedisSinglePortController::processCommand(const std::string &raw_command)
    {
        const auto parsed = parseCommandRecord(raw_command);
        if (!parsed)
        {
            std::cerr << "[MockController] Unparseable command: '" << raw_command << "'\n";
            return;
        }

        std::string cmd = parsed->command;
        std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                       [](unsigned char ch)
                       { return static_cast<char>(std::tolower(ch)); });

        const int addr = parsed->address;
        const std::string &params = parsed->parameters;
        const std::string prefix = "[Motor " + std::to_string(addr) + "] ";

        try
        {
            if (cmd == "enable")
            {
                motor_->enable(addr);
                std::cout << prefix << "Motor enabled.\n";
            }
            else if (cmd == "disable")
            {
                motor_->disable(addr);
                std::cout << prefix << "Motor disabled.\n";
            }
            else if (cmd == "zero")
            {
                motor_->set_zero(addr);
                std::cout << prefix << "Zeroing executed.\n";
            }
            else if (cmd == "offset")
            {
                try
                {
                    int offset = std::stoi(params);
                    motor_->set_offset(addr, offset);
                    std::cout << prefix << "Offset set to: " << offset << "\n";
                }
                catch (...)
                {
                    std::cout << prefix << "Invalid offset parameter: '" << params << "'.\n";
                }
            }
            else if (cmd == "fn_select")
            {
                try
                {
                    int slot = std::stoi(params);
                    motor_->select_function(addr, slot);
                    std::cout << prefix << "Selected function slot " << slot << ".\n";
                }
                catch (...)
                {
                    std::cout << prefix << "Invalid slot parameter: '" << params << "'.\n";
                }
            }
            else if (cmd == "fn_upload")
            {
                // Parse function upload: slot and values separated by spaces/commas
                try
                {
                    std::istringstream stream(params);
                    int slot;
                    if (!(stream >> slot))
                    {
                        std::cout << prefix << "Invalid fn_upload format (expected: slot value1 value2 ...).\n";
                        return;
                    }
                    // Store function (in a real motor, this would upload to hardware)
                    std::vector<int> values;
                    int value;
                    while (stream >> value)
                    {
                        values.push_back(value);
                    }
                    std::cout << prefix << "Function uploaded to slot " << slot
                              << " (" << values.size() << " values).\n";
                }
                catch (...)
                {
                    std::cout << prefix << "Error parsing fn_upload command.\n";
                }
            }
            else if (cmd == "fn_get")
            {
                try
                {
                    int slot = std::stoi(params);
                    std::cout << prefix << "Retrieved function from slot " << slot << ".\n";
                }
                catch (...)
                {
                    std::cout << prefix << "Invalid fn_get parameter: '" << params << "'.\n";
                }
            }
            else if (cmd == "read")
            {
                auto data = motor_->read();
                if (addr < static_cast<int>(data.size()))
                {
                    std::cout << prefix << "Position=" << data[addr].position
                              << ", Torque=" << data[addr].torque
                              << ", Counter=" << data[addr].cmd_cntr << "\n";
                }
            }
            else if (cmd == "status")
            {
                auto data = motor_->read();
                if (addr < static_cast<int>(data.size()))
                {
                    std::cout << prefix << "Enabled=" << (data[addr].enabled ? "yes" : "no")
                              << ", Position=" << data[addr].position
                              << ", Slot=" << data[addr].slot_idx << "\n";
                }
            }
            else
            {
                std::cout << prefix << "Unknown command: '" << cmd << "'.\n";
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << prefix << "Error processing command: " << e.what() << "\n";
        }
    }

    void MockRedisSinglePortController::measureAndStore()
    {
        auto data = motor_->read();
        if (data.empty())
            return;

        for (int i = 0; i < static_cast<int>(data.size()); ++i)
        {
            const auto &d = data[i];
            const auto key = std::string("xdata:") + std::to_string(i);

            std::vector<std::pair<std::string, std::string>> fields;
            fields.emplace_back(redis_fields::ENABLED, d.enabled ? "1" : "0");
            fields.emplace_back(redis_fields::SLOT_INDEX, std::to_string(d.slot_idx));
            fields.emplace_back(redis_fields::CMD_COUNTER, std::to_string(d.cmd_cntr));
            fields.emplace_back(redis_fields::POSITION, std::to_string(d.position));
            fields.emplace_back(redis_fields::TORQUE, std::to_string(d.torque));
            fields.emplace_back(redis_fields::TIMESTAMP, std::to_string(d.t));
            fields.emplace_back(redis_fields::RETRY_COUNT, std::to_string(d.n_tries));

            try
            {
                redis_.xadd(key, "*", fields.begin(), fields.end());
            }
            catch (const std::exception &e)
            {
                std::cerr << "Redis error writing data to " << key << ": " << e.what() << "\n";
            }
        }
    }

    void MockRedisSinglePortController::publishAddressBook()
    {
        // Motor names and their addresses (matching GUI expectations)
        std::vector<std::pair<std::string, std::string>> addrEntries;

        // Format: "motor_name" -> "address|port"
        // Using standard port numbering starting from 15001
        const int kBasePort = 15001;

        addrEntries.emplace_back("e_flex", "0|" + std::to_string(kBasePort + 0));
        addrEntries.emplace_back("e_ext", "1|" + std::to_string(kBasePort + 1));
        addrEntries.emplace_back("s_flex", "2|" + std::to_string(kBasePort + 2));
        addrEntries.emplace_back("s_ext", "3|" + std::to_string(kBasePort + 3));
        addrEntries.emplace_back("s_add_pron", "4|" + std::to_string(kBasePort + 4));
        addrEntries.emplace_back("s_abd", "5|" + std::to_string(kBasePort + 5));
        addrEntries.emplace_back("s_add_sup", "6|" + std::to_string(kBasePort + 6));

        try
        {
            redis_.hset("run:addrs", addrEntries.begin(), addrEntries.end());
        }
        catch (const std::exception &e)
        {
            std::cerr << "Redis error publishing address book: " << e.what() << "\n";
        }
    }

} // namespace exoskeleton::core

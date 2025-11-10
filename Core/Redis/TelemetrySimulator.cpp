// TelemetrySimulator.cpp - Merged standalone simulator with all support code
#include "RedisKeys.h"

#include <sw/redis++/redis++.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <cctype>
#include <unordered_map>
#include <sstream>
#include <optional>
#include <algorithm>

namespace {

using namespace std::chrono_literals;

// ============================================================================
// Constants
// ============================================================================

namespace constants {
    constexpr double kEnabledAmplitude = 50000.0;
    constexpr double kDisabledAmplitude = 5000.0;
    constexpr double kEnabledPhaseIncrement = 0.12;
    constexpr double kDisabledPhaseIncrement = 0.02;
    constexpr int kMinTorque = -150;
    constexpr int kMaxTorque = 150;
    constexpr auto kDefaultRedisUri = "tcp://127.0.0.1:6379";
    constexpr int kMinUpdatePeriodMs = 10;
    constexpr int kDefaultUpdatePeriodMs = 100;
    constexpr int kBasePort = 15001;
}

// ============================================================================
// Type Definitions
// ============================================================================

struct MotorConfig {
    std::string envField;
    std::string displayName;
    int address;
    int port;
    int slotIndex;
};

struct MotorState {
    bool connected {true};
    bool zeroed {false};
    int offset {0};
    int selectedFunction {-1};
    int lastPosition {0};
    int lastTorque {0};
    std::unordered_map<int, std::vector<int>> functions;
    bool enabled {true};
    int commandCounter {0};
    int tries {0};
    double phase {0.0};
};

struct CommandRecord {
    long long timestamp {0};
    std::string command;
    int address {0};
    std::string parameters;
};

// ============================================================================
// Utility Functions
// ============================================================================

std::atomic<bool> g_running {true};

void handleSignal(int)
{
    g_running.store(false, std::memory_order_relaxed);
}

long long nowNs()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}

std::string commandKeyForAddress(int address)
{
    return std::string("command:") + std::to_string(address);
}

int calculatePosition(const MotorConfig& cfg, const MotorState& state)
{
    const double amplitude = state.enabled 
        ? constants::kEnabledAmplitude 
        : constants::kDisabledAmplitude;
    return static_cast<int>(std::sin(state.phase) * amplitude) + cfg.slotIndex * 1000 + state.offset;
}

std::optional<CommandRecord> parseCommandRecord(const std::string& record)
{
    const auto first = record.find('|');
    if (first == std::string::npos) return std::nullopt;

    const auto second = record.find('|', first + 1);
    if (second == std::string::npos) return std::nullopt;

    const auto third = record.find('|', second + 1);
    if (third == std::string::npos) return std::nullopt;

    CommandRecord result;
    try {
        result.timestamp = std::stoll(record.substr(0, first));
    } catch (...) {
        result.timestamp = 0;
    }

    result.command = record.substr(first + 1, second - first - 1);

    try {
        result.address = std::stoi(record.substr(second + 1, third - second - 1));
    } catch (...) {
        result.address = 0;
    }

    result.parameters = record.substr(third + 1);
    return result;
}

bool parseFnUploadParameters(const std::string& parameters, int& slot, std::vector<int>& values)
{
    std::string normalized;
    normalized.reserve(parameters.size());

    for (char ch : parameters) {
        if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '-' || ch == '+') {
            normalized.push_back(ch);
        } else {
            normalized.push_back(' ');
        }
    }

    std::istringstream stream(normalized);
    if (!(stream >> slot)) return false;

    int value = 0;
    while (stream >> value) {
        values.push_back(value);
    }

    return true;
}

void printState(const MotorConfig& cfg, const MotorState& state, const std::string& prefix)
{
    std::cout << prefix << "Address " << cfg.address
              << ": connected=" << (state.connected ? "yes" : "no")
              << ", enabled=" << (state.enabled ? "yes" : "no")
              << ", offset=" << state.offset
              << ", last position=" << state.lastPosition
              << ", last torque=" << state.lastTorque;

    if (state.selectedFunction >= 0) {
        std::cout << ", selected function=" << state.selectedFunction;
    }

    std::cout << std::endl;
}

void publishEnvironment(sw::redis::Redis& redis, const std::vector<MotorConfig>& motors)
{
    std::vector<std::pair<std::string, std::string>> envEntries;
    envEntries.reserve(3 + motors.size());

    envEntries.emplace_back(exo::redis::keys::FIELD_CONTROLLER_CMD, "/opt/exo/controller");
    envEntries.emplace_back(exo::redis::keys::FIELD_LOG_FOLDER, "/var/log/exo");
    envEntries.emplace_back(exo::redis::keys::FIELD_SCRIPTS_FOLDER, "/opt/exo/scripts");

    for (const auto& motor : motors) {
        if (motor.envField.empty()) continue;
        envEntries.emplace_back(motor.envField, motor.displayName);
    }

    redis.hset(exo::redis::keys::CONF_ENV, envEntries.begin(), envEntries.end());
}

void publishAddressBook(sw::redis::Redis& redis, const std::vector<MotorConfig>& motors)
{
    std::vector<std::pair<std::string, std::string>> addrEntries;
    addrEntries.reserve(motors.size());

    for (const auto& motor : motors) {
        const std::string spec = std::to_string(motor.address) + "|" + std::to_string(motor.port);

        // Extract motor name without "motor_" prefix
        std::string motorName;
        if (motor.envField.empty()) {
            motorName = motor.displayName;
        } else {
            // Remove "motor_" prefix if it exists
            std::string envField = motor.envField;
            static const std::string prefix = "motor_";
            if (envField.find(prefix) == 0) {
                motorName = envField.substr(prefix.length());
            } else {
                motorName = envField;
            }
        }

        addrEntries.emplace_back(motorName, spec);
    }

    redis.hset(exo::redis::keys::RUN_ADDRS, addrEntries.begin(), addrEntries.end());
}

std::vector<MotorConfig> defaultMotors()
{
    return {
        {exo::redis::keys::FIELD_MOTOR_E_FLEX, "cstny001", 0, constants::kBasePort, 1},
        {exo::redis::keys::FIELD_MOTOR_E_EXT, "cstny002", 1, constants::kBasePort + 1, 2},
        {exo::redis::keys::FIELD_MOTOR_S_FLEX, "cstny003", 2, constants::kBasePort + 2, 3},
        {exo::redis::keys::FIELD_MOTOR_S_EXT, "cstny004", 3, constants::kBasePort + 3, 4},
        {exo::redis::keys::FIELD_MOTOR_S_ADD_PRON, "", 4, constants::kBasePort + 4, 5},
        {exo::redis::keys::FIELD_MOTOR_S_ABD, "", 5, constants::kBasePort + 5, 6},
        {exo::redis::keys::FIELD_MOTOR_S_ADD_SUP, "cstny007", 6, constants::kBasePort + 6, 7},
    };
}

// ============================================================================
// Command Handling
// ============================================================================

std::string makePrefix(const MotorConfig& cfg)
{
    return "[Motor " + (cfg.displayName.empty() ? std::to_string(cfg.address) : cfg.displayName) + "] ";
}

void handleCommand(const MotorConfig& cfg, MotorState& state, const CommandRecord& record)
{
    std::string command = record.command;
    std::transform(command.begin(), command.end(), command.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    const std::string prefix = makePrefix(cfg);

    if (command == "status") {
        printState(cfg, state, prefix);
    } else if (command == "connect") {
        state.connected = true;
        std::cout << prefix << "Connection initiated." << std::endl;
    } else if (command == "disconnect") {
        state.connected = false;
        state.enabled = false;
        std::cout << prefix << "Serial connection closed." << std::endl;
    } else if (command == "enable") {
        if (!state.connected) {
            std::cout << prefix << "Cannot enable: not connected." << std::endl;
            return;
        }
        state.enabled = true;
        std::cout << prefix << "Motor enabled." << std::endl;
    } else if (command == "disable") {
        state.enabled = false;
        std::cout << prefix << "Motor disabled." << std::endl;
    } else if (command == "zero") {
        state.phase = 0.0;
        state.offset = 0;
        state.zeroed = true;
        std::cout << prefix << "Zeroing executed." << std::endl;
    } else if (command == "offset") {
        try {
            state.offset = std::stoi(record.parameters);
            state.zeroed = false;
            std::cout << prefix << "Offset set to: " << state.offset << std::endl;
        } catch (...) {
            std::cout << prefix << "Invalid offset parameter: '" << record.parameters << "'." << std::endl;
        }
    } else if (command == "read") {
        std::cout << prefix << "Current position=" << state.lastPosition
                  << ", torque=" << state.lastTorque
                  << ", command counter=" << state.commandCounter;
        if (state.selectedFunction >= 0) {
            std::cout << ", selected slot=" << state.selectedFunction;
        }
        std::cout << std::endl;
    } else if (command == "fn_upload") {
        int slot = -1;
        std::vector<int> values;
        if (!parseFnUploadParameters(record.parameters, slot, values)) {
            std::cout << prefix << "Failed to parse function upload parameters." << std::endl;
            return;
        }
        state.functions[slot] = values;
        std::cout << prefix << "Function uploaded to slot " << slot
                  << " (" << values.size() << " values)." << std::endl;
    } else if (command == "fn_get") {
        std::cout << prefix << "Available slots: ";
        if (state.functions.empty()) {
            std::cout << "none uploaded." << std::endl;
        } else {
            bool first = true;
            for (const auto& entry : state.functions) {
                if (!first) std::cout << ", ";
                first = false;
                std::cout << entry.first;
            }
            std::cout << std::endl;
        }
    } else if (command == "fn_select") {
        try {
            const int slot = std::stoi(record.parameters);
            if (state.functions.find(slot) == state.functions.end()) {
                std::cout << prefix << "Slot not uploaded: " << slot << std::endl;
                return;
            }
            state.selectedFunction = slot;
            std::cout << prefix << "Slot selected: " << slot << std::endl;
        } catch (...) {
            std::cout << prefix << "Invalid slot identifier: '" << record.parameters << "'." << std::endl;
        }
    } else {
        std::cout << prefix << "Unknown command: '" << record.command << "'";
        if (!record.parameters.empty()) {
            std::cout << " (parameters: '" << record.parameters << "')";
        }
        std::cout << std::endl;
    }
}

void processPendingCommands(sw::redis::Redis& redis, const MotorConfig& cfg, MotorState& state)
{
    const auto key = commandKeyForAddress(cfg.address);

    while (true) {
        sw::redis::OptionalString record;
        try {
            record = redis.lpop(key);
        } catch (const sw::redis::Error& err) {
            std::cerr << "[Motor " << cfg.address << "] Redis error reading command: " << err.what() << std::endl;
            return;
        }

        if (!record) break;

        const auto parsed = parseCommandRecord(*record);
        if (!parsed) {
            std::cerr << "[Motor " << cfg.address << "] Unparseable command: '" << *record << "'" << std::endl;
            continue;
        }

        if (parsed->address != cfg.address) {
            std::cerr << "[Motor " << cfg.address << "] Address mismatch in command (found: " << parsed->address << ")" << std::endl;
            continue;
        }

        handleCommand(cfg, state, *parsed);
    }
}

// ============================================================================
// Telemetry Data Generation
// ============================================================================

std::vector<std::pair<std::string, std::string>> sampleFields(const MotorConfig& cfg, MotorState& state, std::mt19937& rng)
{
    static std::uniform_int_distribution<int> torqueDist(constants::kMinTorque, constants::kMaxTorque);

    state.commandCounter++;
    state.tries = state.connected ? 0 : 1;
    state.phase += state.enabled ? constants::kEnabledPhaseIncrement : constants::kDisabledPhaseIncrement;

    const auto position = calculatePosition(cfg, state);
    const auto torque = (state.connected && state.enabled) ? torqueDist(rng) : 0;
    state.lastPosition = position;
    state.lastTorque = torque;

    std::vector<std::pair<std::string, std::string>> fields;
    fields.reserve(7);

    fields.emplace_back(exo::redis::keys::FIELD_ENABLED, (state.connected && state.enabled) ? "1" : "0");
    // Use state.selectedFunction if available, otherwise default to 0 or cfg.slotIndex
    const int activeSlot = (state.selectedFunction >= 0) ? state.selectedFunction : 0;
    fields.emplace_back(exo::redis::keys::FIELD_SLOT_INDEX, std::to_string(activeSlot));
    fields.emplace_back(exo::redis::keys::FIELD_CMD_COUNTER, std::to_string(state.commandCounter));
    fields.emplace_back(exo::redis::keys::FIELD_POSITION, std::to_string(position));
    fields.emplace_back(exo::redis::keys::FIELD_TORQUE, std::to_string(torque));
    fields.emplace_back(exo::redis::keys::FIELD_TIMESTAMP, std::to_string(nowNs()));
    fields.emplace_back(exo::redis::keys::FIELD_RETRY_COUNT, std::to_string(state.tries));

    return fields;
}

void sendSample(sw::redis::Redis& redis, const MotorConfig& cfg, MotorState& state, std::mt19937& rng)
{
    const auto key = std::string(exo::redis::keys::XDATA_PREFIX) + std::to_string(cfg.address);
    auto fields = sampleFields(cfg, state, rng);
    redis.xadd(key, "*", fields.begin(), fields.end());
}

void printBanner(const std::string& uri, const std::vector<MotorConfig>& motors, std::chrono::milliseconds period)
{
    std::cout << "Redis telemetry simulator\n";
    std::cout << "  URI: " << uri << "\n";
    std::cout << "  Period: " << period.count() << " ms\n";
    std::cout << "  Motors: " << motors.size() << "\n";
    for (const auto& motor : motors) {
        std::cout << "    - " << (motor.envField.empty() ? motor.displayName : motor.envField)
                  << " (addr " << motor.address << ", port " << motor.port << ")\n";
    }
    std::cout << "Press CTRL+C to stop...\n";
}

} // namespace

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv)
{
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    const std::string uri = (argc > 1) ? argv[1] : constants::kDefaultRedisUri;
    const auto period = std::chrono::milliseconds(
        (argc > 2) ? std::max(constants::kMinUpdatePeriodMs, std::atoi(argv[2]))
                   : constants::kDefaultUpdatePeriodMs
    );

    auto motors = defaultMotors();
    std::vector<MotorState> states(motors.size());

    try {
        sw::redis::Redis redis(uri);

        publishEnvironment(redis, motors);
        publishAddressBook(redis, motors);

        printBanner(uri, motors, period);

        std::mt19937 rng(static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));

        while (g_running.load(std::memory_order_relaxed)) {
            for (std::size_t i = 0; i < motors.size(); ++i) {
                processPendingCommands(redis, motors[i], states[i]);
                sendSample(redis, motors[i], states[i], rng);
            }
            std::this_thread::sleep_for(period);
        }

        std::cout << "Shutting down..." << std::endl;
    } catch (const sw::redis::Error& err) {
        std::cerr << "Redis error: " << err.what() << std::endl;
        return 1;
    }

    return 0;
}
#pragma once
#include <string>
#include <memory>
#include <sw/redis++/redis++.h>
#include <sw/redis++/redis++.h>
#include "MockExoMotors.h"
#include "../core/RedisTools.h"

namespace exoskeleton::core
{

    /**
     * Mock motor controller that integrates with Redis using the same protocol as TelemetrySimulator.
     * Reads commands from Redis queues and pushes motor data in the GUI-compatible format.
     */
    class MockRedisSinglePortController
    {
    public:
        /**
         * Create a mock controller for testing without real motors.
         * @param n_motors number of motors to simulate
         * @param mock_motors mock motors interface
         * @param redis_uri Redis connection URI (default: tcp://127.0.0.1:6379)
         */
        MockRedisSinglePortController(int n_motors,
                                      std::shared_ptr<MockExoMotors> mock_motors = nullptr,
                                      const std::string &redis_uri = "tcp://127.0.0.1:6379");

        /**
         * Run the control loop (blocking).
         * Continuously reads from mock motors and pushes data to Redis.
         * Processes commands from Redis queue "command:<address>" (GUI-compatible format).
         * Exits when "exit" flag is set in Redis to "1".
         */
        void loop();

    private:
        int n_motors_;
        std::shared_ptr<MockExoMotors> motor_;
        sw::redis::Redis redis_;

        /**
         * Process a single command from Redis (pipe-delimited format: timestamp|cmd|address|params).
         * Commands: status, connect, disconnect, enable, disable, zero, offset, read, fn_upload, fn_select, fn_get
         */
        void processCommand(const std::string &raw_command);

        /**
         * Read from mock motors and push data to Redis streams in GUI-compatible format.
         */
        void measureAndStore();

        /**
         * Publish motor address book to run:addrs hash for GUI discovery.
         */
        void publishAddressBook();
    };

} // namespace exoskeleton::core

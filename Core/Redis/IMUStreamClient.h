#pragma once

#include <sw/redis++/redis++.h>
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include "../../include/sample.h"

namespace imu::redis
{

    /**
     * IMU-specific Redis Stream client for writing/reading IMU samples.
     * Uses XADD for writing, XREAD for consuming.
     */
    class IMUStreamClient
    {
    public:
        explicit IMUStreamClient(const std::string &redis_uri = "tcp://127.0.0.1:6379");
        ~IMUStreamClient() = default;

        // Connect to Redis
        bool connect();
        void disconnect();

        // XADD: Write an ImuSample to a Redis stream
        bool xadd_sample(const ImuSample &sample, const std::string &stream_key = "imu:samples");

        // XREAD: Read samples from a stream (blocking)
        // Returns a vector of (stream_id, sample) pairs
        std::vector<std::pair<std::string, ImuSample>>
        xread_samples(const std::string &stream_key, const std::string &start_id = "$", int block_ms = 1000, int count = 10);

        // Get the last ID in a stream
        std::optional<std::string> xrevrange_last_id(const std::string &stream_key);

        // Delete samples from stream after a given ID
        long xdel_samples(const std::string &stream_key, const std::vector<std::string> &ids);

        // Trim stream to max length
        long xtrim_stream(const std::string &stream_key, long max_len);

        // Get stream info (approx length)
        long xlen(const std::string &stream_key);

    private:
        std::string m_redis_uri;
        sw::redis::Redis m_redis;
        bool m_connected = false;

        // Helper to pack ImuSample into Redis hash fields
        std::unordered_map<std::string, std::string> sample_to_fields(const ImuSample &s);

        // Helper to unpack Redis hash fields back into ImuSample
        ImuSample fields_to_sample(const std::unordered_map<std::string, std::string> &fields);
    };

}

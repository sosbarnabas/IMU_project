#include "IMUStreamClient.h"
#include <sstream>
#include <iomanip>
#include <iostream>

namespace imu::redis
{

    IMUStreamClient::IMUStreamClient(const std::string &redis_uri)
        : m_redis_uri(redis_uri), m_redis(redis_uri) {}

    bool IMUStreamClient::connect()
    {
        try
        {
            m_redis.ping();
            m_connected = true;
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "IMUStreamClient: Failed to connect to Redis: " << e.what() << std::endl;
            m_connected = false;
            return false;
        }
    }

    void IMUStreamClient::disconnect()
    {
        m_connected = false;
    }

    std::unordered_map<std::string, std::string> IMUStreamClient::sample_to_fields(const ImuSample &s)
    {
        std::unordered_map<std::string, std::string> fields;

        fields["imu_id"] = std::to_string(s.imu_id);
        fields["seq"] = std::to_string(s.seq);

        auto t_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        s.t_host.time_since_epoch())
                        .count();
        fields["t_ms"] = std::to_string(t_ms);

        // Pack accelerometer
        std::ostringstream accel_ss;
        accel_ss << std::fixed << std::setprecision(6)
                 << s.accel[0] << "," << s.accel[1] << "," << s.accel[2];
        fields["accel"] = accel_ss.str();

        // Pack gyroscope
        std::ostringstream gyro_ss;
        gyro_ss << std::fixed << std::setprecision(6)
                << s.gyro[0] << "," << s.gyro[1] << "," << s.gyro[2];
        fields["gyro"] = gyro_ss.str();

        // Pack magnetometer if available
        if (s.mag_ok)
        {
            std::ostringstream mag_ss;
            mag_ss << std::fixed << std::setprecision(6)
                   << s.mag[0] << "," << s.mag[1] << "," << s.mag[2];
            fields["mag"] = mag_ss.str();
            fields["mag_ok"] = "1";
        }
        else
        {
            fields["mag_ok"] = "0";
        }

        // Pack Euler angles if available
        if (s.euler.has_value())
        {
            std::ostringstream euler_ss;
            euler_ss << std::fixed << std::setprecision(6)
                     << (*s.euler)[0] << "," << (*s.euler)[1] << "," << (*s.euler)[2];
            fields["euler"] = euler_ss.str();
        }

        fields["fifo_overflow"] = s.fifo_overflow ? "1" : "0";
        fields["fifo_underflow"] = s.fifo_underflow ? "1" : "0";

        return fields;
    }

    ImuSample IMUStreamClient::fields_to_sample(const std::unordered_map<std::string, std::string> &fields)
    {
        ImuSample sample{};

        if (fields.count("imu_id"))
            sample.imu_id = std::stoi(fields.at("imu_id"));
        if (fields.count("seq"))
            sample.seq = std::stoull(fields.at("seq"));

        if (fields.count("t_ms"))
        {
            auto t_ms = std::stoll(fields.at("t_ms"));
            sample.t_host = std::chrono::steady_clock::time_point(
                std::chrono::milliseconds(t_ms));
        }

        // Parse accelerometer
        if (fields.count("accel"))
        {
            std::istringstream iss(fields.at("accel"));
            char comma;
            iss >> sample.accel[0] >> comma >> sample.accel[1] >> comma >> sample.accel[2];
        }

        // Parse gyroscope
        if (fields.count("gyro"))
        {
            std::istringstream iss(fields.at("gyro"));
            char comma;
            iss >> sample.gyro[0] >> comma >> sample.gyro[1] >> comma >> sample.gyro[2];
        }

        // Parse magnetometer
        if (fields.count("mag_ok"))
        {
            sample.mag_ok = fields.at("mag_ok") == "1" ? 1 : 0;
            if (sample.mag_ok && fields.count("mag"))
            {
                std::vector<float> mag(3);
                std::istringstream iss(fields.at("mag"));
                char comma;
                iss >> mag[0] >> comma >> mag[1] >> comma >> mag[2];
                sample.mag = mag;
            }
        }

        // Parse Euler angles
        if (fields.count("euler"))
        {
            std::vector<float> euler(3);
            std::istringstream iss(fields.at("euler"));
            char comma;
            iss >> euler[0] >> comma >> euler[1] >> comma >> euler[2];
            sample.euler = euler;
        }

        if (fields.count("fifo_overflow"))
        {
            sample.fifo_overflow = fields.at("fifo_overflow") == "1" ? 1 : 0;
        }
        if (fields.count("fifo_underflow"))
        {
            sample.fifo_underflow = fields.at("fifo_underflow") == "1" ? 1 : 0;
        }

        return sample;
    }

    bool IMUStreamClient::xadd_sample(const ImuSample &sample, const std::string &stream_key)
    {
        if (!m_connected)
            return false;

        try
        {
            auto fields = sample_to_fields(sample);
            m_redis.xadd(stream_key, "*", fields.begin(), fields.end());
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "IMUStreamClient: xadd failed: " << e.what() << std::endl;
            return false;
        }
    }

    std::vector<std::pair<std::string, ImuSample>>
    IMUStreamClient::xread_samples(const std::string &stream_key, const std::string &start_id, int block_ms, int count)
    {
        std::vector<std::pair<std::string, ImuSample>> results;

        if (!m_connected)
            return results;

        try
        {
            // XREAD COUNT count BLOCK block_ms STREAMS stream_key start_id
            auto reply = m_redis.xread(stream_key, start_id,
                                       std::make_pair(count,
                                                      std::chrono::milliseconds(block_ms)));

            // Parse the reply
            for (const auto &[key, messages] : reply)
            {
                for (const auto &[msg_id, fields] : messages)
                {
                    ImuSample sample = fields_to_sample(fields);
                    results.emplace_back(msg_id, sample);
                }
            }
            return results;
        }
        catch (const std::exception &e)
        {
            std::cerr << "IMUStreamClient: xread failed: " << e.what() << std::endl;
            return results;
        }
    }

    std::optional<std::string> IMUStreamClient::xrevrange_last_id(const std::string &stream_key)
    {
        if (!m_connected)
            return std::nullopt;

        try
        {
            auto reply = m_redis.xrevrange(stream_key, "+", "-", std::make_pair(1, 1));
            for (const auto &[key, messages] : reply)
            {
                for (const auto &[msg_id, fields] : messages)
                {
                    return msg_id;
                }
            }
            return std::nullopt;
        }
        catch (const std::exception &e)
        {
            std::cerr << "IMUStreamClient: xrevrange failed: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    long IMUStreamClient::xdel_samples(const std::string &stream_key, const std::vector<std::string> &ids)
    {
        if (!m_connected)
            return 0;

        try
        {
            return m_redis.xdel(stream_key, ids.begin(), ids.end());
        }
        catch (const std::exception &e)
        {
            std::cerr << "IMUStreamClient: xdel failed: " << e.what() << std::endl;
            return 0;
        }
    }

    long IMUStreamClient::xtrim_stream(const std::string &stream_key, long max_len)
    {
        if (!m_connected)
            return 0;

        try
        {
            return m_redis.xtrim(stream_key, max_len);
        }
        catch (const std::exception &e)
        {
            std::cerr << "IMUStreamClient: xtrim failed: " << e.what() << std::endl;
            return 0;
        }
    }

    long IMUStreamClient::xlen(const std::string &stream_key)
    {
        if (!m_connected)
            return 0;

        try
        {
            return m_redis.xlen(stream_key);
        }
        catch (const std::exception &e)
        {
            std::cerr << "IMUStreamClient: xlen failed: " << e.what() << std::endl;
            return 0;
        }
    }

}

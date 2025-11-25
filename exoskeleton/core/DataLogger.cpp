#include "DataLogger.h"
#include "RedisTools.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace exoskeleton::core
{
    DataLogger::DataLogger(sw::redis::Redis& redis, const std::string& output_path)
        : redis_{redis}, output_path_{output_path}, start_id_{}, end_id_{}, record_count_{0}, is_logging_{false}
    {
    }

    void DataLogger::startLogging()
    {
        try
        {
            // Capture the current latest stream ID from xdata:0 (motor 0's telemetry)
            using StreamEntry = std::pair<std::string, std::map<std::string, std::string>>;
                        // Capture the end stream ID
            std::vector<StreamEntry> entries;
            redis_.xrevrange("xdata:0", "+", "-", 1, std::back_inserter(entries));
            if (!entries.empty())
            {
                start_id_ = entries.front().first;
            }
            else
            {
                start_id_ = "0-0";
            }
            is_logging_ = true;
            redis_tools::log(redis_, "DataLogger", "Logging started from ID: " + start_id_);
        }
        catch (const std::exception& e)
        {
            redis_tools::log(redis_, "DataLogger", "Error in startLogging: " + std::string(e.what()),
                             redis_tools::LogLevel::error);
        }
    }

    size_t DataLogger::stopLogging()
    {
        try
        {
            is_logging_ = false;

            // Capture the current latest stream ID from xdata:0 (motor 0's telemetry)
            using StreamEntry = std::pair<std::string, std::map<std::string, std::string>>;
            // Capture the end stream ID
            std::vector<StreamEntry> entries;

            // Capture the end stream ID
            redis_.xrevrange("xdata:0", "+", "-", std::back_inserter(entries));
            if (!entries.empty())
            {
                end_id_ = entries.front().first;
            }
            else
            {
                end_id_ = start_id_;
            }

            // Calculate record count between start and end
            record_count_ = calculateRecordCount();

            // Detect active motors and IMU
            auto active_motors = detectActiveMotors();
            bool imu_active = isImuActive();

            // Export to CSV
            writeToCSV(active_motors, imu_active);

            redis_tools::log(redis_, "DataLogger",
                             "Logging stopped. Records: " + std::to_string(record_count_) +
                             ", CSV written to: " + output_path_);

            return record_count_;
        }
        catch (const std::exception& e)
        {
            redis_tools::log(redis_, "DataLogger", "Error in stopLogging: " + std::string(e.what()),
                             redis_tools::LogLevel::error);
            return 0;
        }
    }

    size_t DataLogger::calculateRecordCount()
    {
        try
        {
            // Capture the current latest stream ID from xdata:0 (motor 0's telemetry)
            using StreamEntry = std::pair<std::string, std::map<std::string, std::string>>;
            // Capture the end stream ID
            std::vector<StreamEntry> entries;
            // Count records in xdata:0 between start_id (exclusive) and end_id (inclusive)
            redis_.xrange("xdata:0", "(" + start_id_, end_id_, std::back_inserter(entries));
            return entries.size();
        }
        catch (const std::exception& e)
        {
            redis_tools::log(redis_, "DataLogger", "Error calculating record count: " + std::string(e.what()),
                             redis_tools::LogLevel::error);
            return 0;
        }
    }

    std::vector<int> DataLogger::detectActiveMotors()
    {
        std::vector<int> active_motors;
        try
        {
            for (int i = 0; i < 7; ++i)
            {
                std::string key = "xdata:" + std::to_string(i);
                // Capture the current latest stream ID from xdata:0 (motor 0's telemetry)
                using StreamEntry = std::pair<std::string, std::map<std::string, std::string>>;
                // Capture the end stream ID
                std::vector<StreamEntry> entries;
                redis_.xrevrange(key, "+", "-", 1, std::back_inserter(entries));
                if (!entries.empty())
                {
                    active_motors.push_back(i);
                }
            }
        }
        catch (const std::exception& e)
        {
            redis_tools::log(redis_, "DataLogger", "Error detecting motors: " + std::string(e.what()),
                             redis_tools::LogLevel::error);
        }
        return active_motors;
    }

    bool DataLogger::isImuActive()
    {
        try
        {
            // Capture the current latest stream ID from xdata:0 (motor 0's telemetry)
            using StreamEntry = std::pair<std::string, std::map<std::string, std::string>>;
            // Capture the end stream ID
            std::vector<StreamEntry> entries;
            redis_.xrevrange("xdata:imu:0", "+", "-", 1, std::back_inserter(entries));
            return !entries.empty();
        }
        catch (const std::exception& e)
        {
            redis_tools::log(redis_, "DataLogger", "Error checking IMU: " + std::string(e.what()),
                             redis_tools::LogLevel::error);
            return false;
        }
    }

    std::map<std::string, std::vector<std::map<std::string, std::string>>> DataLogger::readStreamRange(
        const std::string& stream_key,
        const std::string& start_id,
        const std::string& end_id)
    {
        std::map<std::string, std::vector<std::map<std::string, std::string>>> result;
        try
        {
            // Capture the current latest stream ID from xdata:0 (motor 0's telemetry)
            using StreamEntry = std::pair<std::string, std::map<std::string, std::string>>;
            // Capture the end stream ID
            std::vector<StreamEntry> entries;
            redis_.xrange(stream_key, "(" + start_id, end_id, std::back_inserter(entries));
            for (const auto& entry : entries)
            {
                result[stream_key].push_back(entry.second);
            }
        }
        catch (const std::exception& e)
        {
            redis_tools::log(redis_, "DataLogger", "Error reading stream " + stream_key + ": " + std::string(e.what()),
                             redis_tools::LogLevel::error);
        }
        return result;
    }

    void DataLogger::writeToCSV(const std::vector<int>& active_motors, bool imu_active)
    {
        try
        {
            std::ofstream csv_file(output_path_);
            if (!csv_file.is_open())
            {
                throw std::runtime_error("Cannot open CSV file: " + output_path_);
            }

            // Build CSV header
            std::string header = "timestamp,stream_id";

            // Add motor columns
            for (int motor_id : active_motors)
            {
                //header += ",motor_" + std::to_string(motor_id) + "_enabled";
                header += ",motor_" + std::to_string(motor_id) + "_position";
               // header += ",motor_" + std::to_string(motor_id) + "_torque";
               // header += ",motor_" + std::to_string(motor_id) + "_cmd_cntr";
               // header += ",motor_" + std::to_string(motor_id) + "_slot_idx";
                header += ",motor_" + std::to_string(motor_id) + "_t";
               // header += ",motor_" + std::to_string(motor_id) + "_n_tries";
            }

            // Add IMU columns
            if (imu_active)
            {
                header += ",imu_t_ns,imu_seq,imu_imu_id";
                //header += ",imu_accel_x,imu_accel_y,imu_accel_z";
               // header += ",imu_gyro_x,imu_gyro_y,imu_gyro_z";
               // header += ",imu_mag_x,imu_mag_y,imu_mag_z";
                header += ",imu_euler_roll,imu_euler_pitch,imu_euler_yaw";
               // header += ",imu_fifo_size,imu_fifo_mult,imu_flags";
            }

            csv_file << header << "\n";

            // Read motor data
            std::map<int, std::vector<std::map<std::string, std::string>>> motor_data;
            for (int motor_id : active_motors)
            {
                std::string key = "xdata:" + std::to_string(motor_id);
                auto stream_data = readStreamRange(key, start_id_, end_id_);
                motor_data[motor_id] = stream_data[key];
            }

            // Read IMU data
            std::vector<std::map<std::string, std::string>> imu_data;
            if (imu_active)
            {
                auto stream_data = readStreamRange("xdata:imu:0", start_id_, end_id_);
                imu_data = stream_data["xdata:imu:0"];
            }

            // Determine max rows
            size_t max_rows = 0;
            for (const auto& [motor_id, data] : motor_data)
            {
                max_rows = std::max(max_rows, data.size());
            }
            max_rows = std::max(max_rows, imu_data.size());

            // Write rows
            for (size_t row = 0; row < max_rows; ++row)
            {
                std::ostringstream line;

                // Timestamp and stream ID
                if (row < motor_data.begin()->second.size())
                {
                    // Get timestamp from first motor's data
                    auto& first_motor_data = motor_data.begin()->second[row];
                    if (first_motor_data.count("timestamp"))
                    {
                        line << first_motor_data["timestamp"];
                    }
                }
                line << ",";

                if (row == 0)
                {
                    line << start_id_;
                }
                else if (row == max_rows - 1)
                {
                    line << end_id_;
                }
                else
                {
                    line << "...";
                }

                // Motor data columns
                for (int motor_id : active_motors)
                {
                    line << ",";
                    if (row < motor_data[motor_id].size())
                    {
                        auto& data = motor_data[motor_id][row];
                       // line << (data.count("enabled") ? data["enabled"] : "");
                        line << "," << (data.count("position") ? data["position"] : "");
                       // line << "," << (data.count("torque") ? data["torque"] : "");
                        //line << "," << (data.count("cmd_cntr") ? data["cmd_cntr"] : "");
                       // line << "," << (data.count("slot_idx") ? data["slot_idx"] : "");
                        line << "," << (data.count("t") ? data["t"] : "");
                      //  line << "," << (data.count("n_tries") ? data["n_tries"] : "");
                    }
                    else
                    {
                        line << ",,,,,,";
                    }
                }

                // IMU data columns
                if (imu_active)
                {
                    line << ",";
                    if (row < imu_data.size())
                    {
                        auto& data = imu_data[row];
                        line << (data.count("t_ns") ? data["t_ns"] : "");
                        line << "," << (data.count("seq") ? data["seq"] : "");
                        line << "," << (data.count("imu_id") ? data["imu_id"] : "");
                        line << "," << (data.count("euler_roll") ? data["euler_roll"] : "");
                        line << "," << (data.count("euler_pitch") ? data["euler_pitch"] : "");
                        line << "," << (data.count("euler_yaw") ? data["euler_yaw"] : "");
                    }
                    else
                    {
                        line << ",,,,,,,,,,,,,,,,,";
                    }
                }

                csv_file << line.str() << "\n";
            }

            csv_file.close();
        }
        catch (const std::exception& e)
        {
            redis_tools::log(redis_, "DataLogger", "Error writing CSV: " + std::string(e.what()),
                             redis_tools::LogLevel::error);
        }
    }
} // exoskeleton::core

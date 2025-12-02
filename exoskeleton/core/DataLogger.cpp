#include "DataLogger.h"
#include "RedisTools.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>

namespace exoskeleton::core
{
    using EntryMap = std::map<std::string, std::string>;
    using StreamEntry = std::pair<std::string, EntryMap>;

    DataLogger::DataLogger(sw::redis::Redis& redis, const std::string& output_path)
        : redis_{redis},
          output_path_{output_path},
          start_id_{"0-0"},
          end_id_{"0-0"},
          record_count_{0},
          is_logging_{false},
          active_motors_{},
          imu_active_{false},
          imu_id_{-1},
          ref_motor_id_{-1},
          ref_stream_key_{}
    {
    }

    void DataLogger::loadRunAddrs()
    {
        active_motors_.clear();
        imu_active_ = false;
        imu_id_ = -1;
        ref_motor_id_ = -1;
        ref_stream_key_.clear();

        try
        {
            // run:addrs is a hash: key = name ("motor0"/"imu0"), value = "<num>|..."
            std::unordered_map<std::string, std::string> run_addrs;
            redis_.hgetall("run:addrs",
                           std::inserter(run_addrs, run_addrs.begin()));

            for (const auto& kv : run_addrs)
            {
                const auto& name = kv.first;
                const auto& val = kv.second;

                // Extract <num> from "<num>|..."
                auto pipe_pos = val.find('|');
                std::string num_str = (pipe_pos == std::string::npos)
                                          ? val
                                          : val.substr(0, pipe_pos);

                int id = -1;
                try
                {
                    id = std::stoi(num_str);
                }
                catch (const std::exception& e)
                {

                    continue; // skip malformed entry
                }

                if (name.find("imu") != std::string::npos)
                {
                    imu_active_ = true;
                    imu_id_ = id;
                }
                else
                {
                    active_motors_.push_back(id);
                }
            }

            // Sort & deduplicate motors
            std::sort(active_motors_.begin(), active_motors_.end());
            active_motors_.erase(std::unique(active_motors_.begin(),
                                             active_motors_.end()),
                                 active_motors_.end());

            // Choose reference motor stream:
            //  - Prefer motor 0 if present, else first motor in list.
            if (!active_motors_.empty())
            {
                auto it = std::find(active_motors_.begin(),
                                    active_motors_.end(), 0);
                if (it != active_motors_.end())
                    ref_motor_id_ = 0;
                else
                    ref_motor_id_ = active_motors_.front();

                ref_stream_key_ = "xdata:" + std::to_string(ref_motor_id_);
            }
            else
            {
                ref_motor_id_ = -1;
                ref_stream_key_ = "";
            }

            std::ostringstream msg;
            msg << "run:addrs parsed. Motors: [";
            for (size_t i = 0; i < active_motors_.size(); ++i)
            {
                if (i > 0) msg << ",";
                msg << active_motors_[i];
            }
            msg << "], IMU active: " << (imu_active_ ? "yes" : "no");
            if (imu_active_) msg << " (id=" << imu_id_ << ")";
            msg << ", ref motor id: " << ref_motor_id_;

            redis_tools::log(redis_, "DataLogger", msg.str());
        }
        catch (const std::exception& e)
        {
            redis_tools::log(redis_, "DataLogger",
                             "Error in loadRunAddrs: " + std::string(e.what()),
                             redis_tools::LogLevel::error);
        }
    }

    void DataLogger::startLogging(std::string path)
    {
        try
        {
            // Determine active motors & IMU from run:addrs
            loadRunAddrs();

            if (active_motors_.empty() || ref_stream_key_.empty())
            {
                redis_tools::log(redis_, "DataLogger",
                                 "startLogging: no active motors found in run:addrs.",
                                 redis_tools::LogLevel::error);
                is_logging_ = false;
                return;
            }

            // Capture the latest stream ID from the reference motor stream
            std::vector<StreamEntry> entries;
            redis_.xrevrange(ref_stream_key_, "+", "-", 1,
                             std::back_inserter(entries));

            if (!entries.empty())
            {
                start_id_ = entries.front().first;
            }
            else
            {
                start_id_ = "0-0";
            }

            is_logging_ = true;
            record_count_ = 0;
            output_path_ = path;

            redis_tools::log(redis_, "DataLogger",
                             "Logging started from ref stream '" + ref_stream_key_ +
                             "' at ID: " + start_id_ + ", output path: " + output_path_);

        }

        catch (const std::exception& e)
        {
            redis_tools::log(redis_, "DataLogger",
                             "Error in startLogging: " + std::string(e.what()),
                             redis_tools::LogLevel::error);
            is_logging_ = false;
        }
    }

    size_t DataLogger::stopLogging()
    {
        try
        {
            if (!is_logging_)
            {
                redis_tools::log(redis_, "DataLogger",
                                 "stopLogging called but logging is not active.",
                                 redis_tools::LogLevel::error);
                return 0;
            }

            is_logging_ = false;

            if (ref_stream_key_.empty())
            {
                redis_tools::log(redis_, "DataLogger",
                                 "stopLogging: ref_stream_key_ is empty.",
                                 redis_tools::LogLevel::error);
                return 0;
            }

            // Capture the latest stream ID from the reference motor stream
            std::vector<StreamEntry> entries;
            redis_.xrevrange(ref_stream_key_, "+", "-", 1,
                             std::back_inserter(entries));

            if (!entries.empty())
            {
                end_id_ = entries.front().first;
            }
            else
            {
                end_id_ = start_id_;
            }

            // Calculate record count between start and end based on ref stream
            record_count_ = calculateRecordCount();

            // Export to CSV – uses active_motors_ and imu_active_/imu_id_
            writeToCSV();

            std::ostringstream msg;
            msg << "Logging stopped. Reference stream: " << ref_stream_key_
                << ", records: " << record_count_
                << ", CSV: " << output_path_;

            redis_tools::log(redis_, "DataLogger", msg.str());

            return record_count_;
        }
        catch (const std::exception& e)
        {
            redis_tools::log(redis_, "DataLogger",
                             "Error in stopLogging: " + std::string(e.what()),
                             redis_tools::LogLevel::error);
            return 0;
        }
    }

    size_t DataLogger::calculateRecordCount()
    {
        try
        {
            if (ref_stream_key_.empty())
                return 0;

            auto data = readStreamRange(ref_stream_key_, start_id_, end_id_);
            auto it = data.find(ref_stream_key_);
            if (it != data.end())
                return it->second.size();

            return 0;
        }
        catch (const std::exception& e)
        {
            redis_tools::log(redis_, "DataLogger",
                             "Error calculating record count: " + std::string(e.what()),
                             redis_tools::LogLevel::error);
            return 0;
        }
    }

    std::map<std::string, std::vector<EntryMap>>
    DataLogger::readStreamRange(const std::string& stream_key,
                                const std::string& start_id,
                                const std::string& end_id)
    {
        std::map<std::string, std::vector<EntryMap>> result;

        try
        {
            std::vector<StreamEntry> entries;

            // (start_id, end_id] ⇒ "(" + start_id
            redis_.xrange(stream_key, "(" + start_id, end_id,
                          std::back_inserter(entries));

            for (const auto& entry : entries)
            {
                result[stream_key].push_back(entry.second);
            }
        }
        catch (const std::exception& e)
        {
            redis_tools::log(redis_, "DataLogger",
                             "Error reading stream " + stream_key + ": " +
                             std::string(e.what()),
                             redis_tools::LogLevel::error);
        }

        return result;
    }

    void DataLogger::writeToCSV()
    {
        try
        {
            std::ofstream csv_file(output_path_);
            if (!csv_file.is_open())
            {
                throw std::runtime_error("Cannot open CSV file: " + output_path_);
            }

            // ------------------------------------------------------------
            // Build CSV header: ONLY requested fields
            //  For each motor: motor_id, motor_t, motor_position, motor_torque
            //  For IMU: imu_id, imu_t_ns, imu_euler_roll, imu_euler_pitch, imu_euler_yaw
            // ------------------------------------------------------------
            std::ostringstream header;

            for (size_t i = 0; i < active_motors_.size(); ++i)
            {
                int motor_id = active_motors_[i];
                if (i > 0) header << ",";
                header << "motor_" << motor_id << "_id";
                header << ",motor_" << motor_id << "_t";
                header << ",motor_" << motor_id << "_position";
                header << ",motor_" << motor_id << "_torque";
            }

            if (imu_active_)
            {
                if (!active_motors_.empty()) header << ",";
                header << "imu_id,imu_t_ns,imu_euler_roll,imu_euler_pitch,imu_euler_yaw";
            }

            csv_file << header.str() << "\n";

            // ------------------------------------------------------------
            // Read motor data for all active motors in [start_id_, end_id_]
            // ------------------------------------------------------------
            std::map<int, std::vector<EntryMap>> motor_data;
            size_t max_rows = 0;

            for (int motor_id : active_motors_)
            {
                std::string key = "xdata:" + std::to_string(motor_id);
                auto stream_data = readStreamRange(key, start_id_, end_id_);
                auto it = stream_data.find(key);

                if (it != stream_data.end())
                {
                    motor_data[motor_id] = std::move(it->second);
                    max_rows = std::max(max_rows, motor_data[motor_id].size());
                }
                else
                {
                    motor_data[motor_id] = {};
                }
            }

            // ------------------------------------------------------------
            // Read IMU data (if active) in [start_id_, end_id_]
            // ------------------------------------------------------------
            std::vector<EntryMap> imu_data;
            if (imu_active_ && imu_id_ >= 0)
            {
                std::string imu_key = "xdata:imu:" + std::to_string(imu_id_);
                auto stream_data = readStreamRange(imu_key, start_id_, end_id_);
                auto it = stream_data.find(imu_key);
                if (it != stream_data.end())
                {
                    imu_data = std::move(it->second);
                    max_rows = std::max(max_rows, imu_data.size());
                }
            }

            // If calculateRecordCount already determined record_count_, you can
            // choose to use it; otherwise fall back to max_rows.
            if (record_count_ == 0)
                record_count_ = max_rows;

            const size_t rows = record_count_;

            // ------------------------------------------------------------
            // Write rows
            //   - Each row index corresponds to the same index into each
            //     motor's vector (if available) and IMU vector (if available).
            // ------------------------------------------------------------
            for (size_t row = 0; row < rows; ++row)
            {
                std::ostringstream line;

                // Motor data columns
                for (size_t i = 0; i < active_motors_.size(); ++i)
                {
                    int motor_id = active_motors_[i];

                    if (i > 0) line << ",";

                    // motor_id column (constant)
                    line << motor_id;

                    const auto& vec = motor_data[motor_id];

                    if (row < vec.size())
                    {
                        const auto& data = vec[row];

                        // timestamp
                        line << "," << (data.count("t") ? data.at("t") : "");

                        // position
                        line << "," << (data.count("position") ? data.at("position") : "");

                        // torque
                        line << "," << (data.count("torque") ? data.at("torque") : "");
                    }
                    else
                    {
                        // no data for this motor at this row
                        line << ",,,";
                    }
                }

                // IMU data columns
                if (imu_active_)
                {
                    if (!active_motors_.empty()) line << ",";

                    if (row < imu_data.size())
                    {
                        const auto& data = imu_data[row];

                        // imu_id
                        line << (data.count("imu_id")
                                     ? data.at("imu_id")
                                     : std::to_string(imu_id_));

                        // timestamp
                        line << "," << (data.count("t_ns") ? data.at("t_ns") : "");

                        // euler angles
                        line << "," << (data.count("euler_roll") ? data.at("euler_roll") : "");
                        line << "," << (data.count("euler_pitch") ? data.at("euler_pitch") : "");
                        line << "," << (data.count("euler_yaw") ? data.at("euler_yaw") : "");
                    }
                    else
                    {
                        // no IMU data for this row
                        line << imu_id_ << ",,,,"; // imu_id plus 4 empty fields
                    }
                }

                csv_file << line.str() << "\n";
            }

            csv_file.close();
        }
        catch (const std::exception& e)
        {
            redis_tools::log(redis_, "DataLogger",
                             "Error writing CSV: " + std::string(e.what()),
                             redis_tools::LogLevel::error);
        }
    }
} // namespace exoskeleton::core

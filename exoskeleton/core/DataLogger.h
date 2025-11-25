#pragma once

#include <string>
#include <vector>
#include <map>
#include <sw/redis++/redis++.h>

namespace exoskeleton::core
{

    class DataLogger
    {
    public:
        DataLogger(sw::redis::Redis &redis, const std::string &output_path = "exoskeleton_data_log.csv");

        void startLogging();
        size_t stopLogging();
        bool isLogging() const { return is_logging_; }

        void setOutputPath(const std::string &path) { output_path_ = path; }
        const std::string &getOutputPath() const { return output_path_; }

    private:
        sw::redis::Redis &redis_;
        std::string output_path_;
        std::string start_id_;
        std::string end_id_;
        size_t record_count_;
        bool is_logging_;

        size_t calculateRecordCount();
        std::vector<int> detectActiveMotors();
        bool isImuActive();
        std::map<std::string, std::vector<std::map<std::string, std::string>>> readStreamRange(
            const std::string &stream_key,
            const std::string &start_id,
            const std::string &end_id);
        void writeToCSV(const std::vector<int> &active_motors, bool imu_active);
    };

} // exoskeleton::core

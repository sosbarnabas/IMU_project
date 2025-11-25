#pragma once

#include <string>
#include <vector>
#include <map>
#include <sw/redis++/redis++.h>

namespace exoskeleton::core
{

    class DataLogger {
    public:
        DataLogger(sw::redis::Redis& redis, const std::string& output_path);

        void startLogging();
        size_t stopLogging();

    private:
        void loadRunAddrs();
        size_t calculateRecordCount();
        std::map<std::string, std::vector<std::map<std::string, std::string>>>
            readStreamRange(const std::string& stream_key,
                            const std::string& start_id,
                            const std::string& end_id);
        void writeToCSV();

        sw::redis::Redis& redis_;
        std::string output_path_;

        std::string start_id_;
        std::string end_id_;
        size_t record_count_;
        bool is_logging_;

        // New members
        std::vector<int> active_motors_;
        bool imu_active_{false};
        int imu_id_{-1};
        int ref_motor_id_{-1};
        std::string ref_stream_key_;
    };


} // exoskeleton::core

#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>
#include <sw/redis++/redis++.h>
namespace exoskeleton::redis {

/**
 * @brief Loads .env file and uploads to Redis conf:env
 * 
 * Parses .env file with format:
 * key="value"
 * # comments are ignored
 * 
 * Strips "exo_" prefix from keys before uploading to Redis.
 */
class EnvLoader {
public:
    /**
     * @brief Load .env file from path
     * @param env_path Path to .env file (default: .env in current directory)
     * @return Map of key-value pairs (with exo_ prefix stripped)
     */
    static std::unordered_map<std::string, std::string> load_env_file(
        const std::string& env_path = ".env"
    );

    /**
     * @brief Upload environment settings to Redis conf:env
     * @param redis Redis connection
     * @param env_path Path to .env file
     * @param overwrite If true, delete existing conf:env first; if false, only update missing keys
     * @return Number of keys uploaded
     */
    static int upload_to_redis(
        sw::redis::Redis& redis,
        const std::string& env_path = ".env",
        bool overwrite = false
    );

private:
    static std::string strip_quotes(const std::string& value);
    static std::string strip_prefix(const std::string& key, const std::string& prefix = "exo_");
};

} // namespace exoskeleton::redis

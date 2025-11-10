#include "EnvLoader.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <sw/redis++/redis++.h>

namespace exoskeleton::redis {

std::string EnvLoader::strip_quotes(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::string EnvLoader::strip_prefix(const std::string& key, const std::string& prefix) {
    if (key.size() > prefix.size() && key.substr(0, prefix.size()) == prefix) {
        return key.substr(prefix.size());
    }
    return key;
}

std::unordered_map<std::string, std::string> EnvLoader::load_env_file(
    const std::string& env_path
) {
    std::unordered_map<std::string, std::string> env_map;
    
    std::ifstream file(env_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open .env file: " + env_path);
    }

    std::string line;
    int line_number = 0;
    while (std::getline(file, line)) {
        ++line_number;
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Parse key=value
        auto pos = line.find('=');
        if (pos == std::string::npos) {
            std::cerr << "Warning: Invalid line " << line_number << " in .env: " << line << std::endl;
            continue;
        }
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        // Trim key and value
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // Strip quotes from value
        value = strip_quotes(value);
        
        // Strip exo_ prefix from key
        key = strip_prefix(key);
        
        env_map[key] = value;
    }
    
    return env_map;
}

int EnvLoader::upload_to_redis(
    sw::redis::Redis& redis,
    const std::string& env_path,
    bool overwrite
) {
    std::cout << "[EnvLoader] Starting .env file upload from: " << env_path << std::endl;
    
    auto env_map = load_env_file(env_path);
    
    if (env_map.empty()) {
        std::cerr << "[EnvLoader] Warning: No valid entries found in .env file" << std::endl;
        return 0;
    }
    
    std::cout << "[EnvLoader] Loaded " << env_map.size() << " entries from .env" << std::endl;
    
    // Check if conf:env already exists
    bool exists = redis.exists("conf:env");
    std::cout << "[EnvLoader] conf:env exists in Redis: " << (exists ? "YES" : "NO") << std::endl;
    
    if (exists && overwrite) {
        std::cout << "[EnvLoader] Deleting existing conf:env..." << std::endl;
        redis.del("conf:env");
    } else if (exists && !overwrite) {
        std::cout << "[EnvLoader] conf:env exists, updating only missing keys..." << std::endl;
        // Filter out existing keys
        std::vector<std::string> existing_keys;
        redis.hkeys("conf:env", std::back_inserter(existing_keys));
        std::cout << "[EnvLoader] Found " << existing_keys.size() << " existing keys in conf:env" << std::endl;
        for (const auto& key : existing_keys) {
            env_map.erase(key);
        }
        if (env_map.empty()) {
            std::cout << "[EnvLoader] All keys already exist in conf:env, nothing to update" << std::endl;
            return 0;
        }
    }
    
    // Upload to Redis
    std::cout << "[EnvLoader] Uploading " << env_map.size() << " keys to conf:env..." << std::endl;
    
    // Convert to vector of pairs for hset
    std::vector<std::pair<std::string, std::string>> pairs(env_map.begin(), env_map.end());
    redis.hset("conf:env", pairs.begin(), pairs.end());
    
    std::cout << "[EnvLoader] Successfully uploaded " << env_map.size() << " keys to Redis conf:env" << std::endl;
    
    return static_cast<int>(env_map.size());
}

} // namespace exoskeleton::redis

#pragma once

#include <sw/redis++/redis++.h>
#include <optional>
#include <string>
#include <unordered_map>

namespace exoskeleton::redis_tools {

/// Retrieve all fields of a hash into a map
std::unordered_map<std::string, std::string>
hgetall_map(sw::redis::Redis &redis, const std::string &key);

/// GET key → optional<string> (nullopt if not found)
std::optional<std::string>
get_opt(sw::redis::Redis &redis, const std::string &key);

/**
 * GETSET key 0 → returns old flag value as bool
 * If clear=true, resets flag to 0 after reading; otherwise leaves it.
 */
bool get_flag(sw::redis::Redis &redis,
              const std::string &key,
              bool clear = false);

/**
 * Create a Subscriber for keyspace notifications on given key.
 * Internally runs CONFIG SET notify-keyspace-events KEA.
 */
sw::redis::Subscriber make_keyspace_subscriber(
    sw::redis::Redis &redis,
    const std::string &key);

} // namespace redis_tools

#pragma once

#include <QString>
#include <memory>
#include <functional>

namespace sw::redis { class Redis; }

namespace exo::redis {

/**
 * @brief Result of a command send operation
 */
struct CommandResult {
    bool success;
    QString errorMessage;
    QString uniqueId;  // Unique ID for tracking response
};

/**
 * @brief Response from executed command
 */
struct CommandResponse {
    bool success;       // true if OK:, false if ER:
    QString value;      // Return value after OK: or ER:
    bool found;         // true if response was found in Redis
};

/**
 * @brief Client for sending commands to motors via Redis
 */
class CommandClient {
public:
    explicit CommandClient(const QString& uri);
    ~CommandClient();

    /**
     * @brief Set the Redis URI connection string
     */
    void setUri(const QString& uri);

    /**
     * @brief Get the current Redis URI
     */
    QString uri() const { return uri_; }

    /**
     * @brief Check if client is connected
     */
    bool isConnected() const;

    /**
     * @brief Ensure connection is established
     * @return true if connected or connection successful
     */
    bool ensureConnection();

    /**
     * @brief Disconnect from Redis
     */
    void disconnect();

    /**
     * @brief Send a command to a motor
     * @param command Command name
     * @param address Motor address
     * @param parameters Optional command parameters
     * @return Result of the send operation with unique ID for tracking
     */
    CommandResult sendCommand(const QString& command, int address, const QString& parameters = QString());

    /**
     * @brief Check command response from backend
     * @param command Command name
     * @param address Motor address
     * @param uniqueId Unique ID from sendCommand result
     * @return Command response (OK: or ER:)
     */
    CommandResponse checkCommandResponse(const QString& command, int address, const QString& uniqueId);

private:
    QString buildCommandRecord(const QString& command, int address, const QString& parameters) const;
    QString commandKeyForAddress(int address) const;
    QString generateUniqueId() const;
    QString commandResponseKey(const QString& command, int address, const QString& uniqueId) const;

    QString uri_;
    std::unique_ptr<sw::redis::Redis> client_;
};

} // namespace exo::redis

#include "CommandClient.h"

#include <sw/redis++/redis++.h>
#include <QDateTime>
#include <QDebug>
#include <QUuid>

namespace exo::redis {

CommandClient::CommandClient(const QString& uri)
    : uri_(uri)
{
}

CommandClient::~CommandClient()
{
    disconnect();
}

void CommandClient::setUri(const QString& uri)
{
    if (uri_ == uri) {
        return;
    }
    qDebug() << "[CommandClient] Setting Redis URI:" << uri;
    uri_ = uri;
    disconnect();
}

bool CommandClient::isConnected() const
{
    return client_ != nullptr;
}

bool CommandClient::ensureConnection()
{
    if (client_) {
        qDebug() << "[CommandClient] Already connected to Redis";
        return true;
    }

    if (uri_.isEmpty()) {
        qWarning() << "[CommandClient] Cannot connect: Redis URI is empty";
        return false;
    }

    qDebug() << "[CommandClient] Attempting to connect to Redis:" << uri_;
    try {
        client_ = std::make_unique<sw::redis::Redis>(uri_.toStdString());
        qDebug() << "[CommandClient] Successfully connected to Redis";
        return true;
    } catch (const sw::redis::Error& err) {
        qWarning() << "[CommandClient] Redis connection failed (sw::redis::Error):" << QString::fromUtf8(err.what());
        client_.reset();
        return false;
    } catch (const std::exception& ex) {
        qWarning() << "[CommandClient] Redis connection failed (std::exception):" << QString::fromUtf8(ex.what());
        client_.reset();
        return false;
    }
}

void CommandClient::disconnect()
{
    if (client_) {
        qDebug() << "[CommandClient] Disconnecting from Redis";
    }
    client_.reset();
}

CommandResult CommandClient::sendCommand(const QString& command, int address, const QString& parameters)
{
    qDebug() << "[CommandClient] sendCommand called:" << command << "address:" << address << "params:" << parameters;
    
    if (!ensureConnection()) {
        qWarning() << "[CommandClient] Failed to ensure connection for command:" << command;
        return {false, QStringLiteral("Failed to connect to Redis"), QString()};
    }

    // Generate unique ID for tracking response
    const QString uniqueId = generateUniqueId();
    
    const QString record = buildCommandRecord(command, address, parameters);
    const QString key = commandKeyForAddress(address);
    
    qDebug() << "[CommandClient] Sending to key:" << key << "record:" << record << "uniqueId:" << uniqueId;

    try {
        client_->rpush(key.toStdString(), record.toStdString());
        qDebug() << "[CommandClient] Command sent successfully:" << command << "to address:" << address << "uniqueId:" << uniqueId;
        return {true, QString(), uniqueId};
    } catch (const sw::redis::Error& err) {
        const QString errorMsg = QString::fromUtf8(err.what());
        qWarning() << "[CommandClient] Redis error (sw::redis::Error) sending command:" << command << "error:" << errorMsg;
        client_.reset(); // Disconnect on error
        return {false, errorMsg, QString()};
    } catch (const std::exception& ex) {
        const QString errorMsg = QString::fromUtf8(ex.what());
        qWarning() << "[CommandClient] Redis error (std::exception) sending command:" << command << "error:" << errorMsg;
        client_.reset(); // Disconnect on error
        return {false, errorMsg, QString()};
    }
}

CommandResponse CommandClient::checkCommandResponse(const QString& command, int address, const QString& uniqueId)
{
    if (!ensureConnection()) {
        qWarning() << "[CommandClient] Failed to ensure connection for checking response";
        return {false, QString(), false};
    }

    // Format: commandres:<command>:<address>:<uniqueId>
    const QString responseKey = commandResponseKey(command, address, uniqueId);
    
    qDebug() << "[CommandClient] Checking response at key:" << responseKey;

    try {
        auto result = client_->get(responseKey.toStdString());
        
        if (!result) {
            qDebug() << "[CommandClient] No response found yet for:" << responseKey;
            return {false, QString(), false};
        }
        
        QString response = QString::fromStdString(*result);
        qDebug() << "[CommandClient] Got response:" << response;
        
        // Parse response: "OK:<value>" or "ER:<value>"
        if (response.startsWith("OK:")) {
            QString value = response.mid(3); // Remove "OK:" prefix
            qDebug() << "[CommandClient] Command succeeded with value:" << value;
            return {true, value, true};
        } else if (response.startsWith("ER:")) {
            QString value = response.mid(3); // Remove "ER:" prefix
            qWarning() << "[CommandClient] Command failed with error:" << value;
            return {false, value, true};
        } else {
            qWarning() << "[CommandClient] Invalid response format:" << response;
            return {false, response, true};
        }
        
    } catch (const sw::redis::Error& err) {
        qWarning() << "[CommandClient] Redis error checking response:" << QString::fromUtf8(err.what());
        client_.reset();
        return {false, QString(), false};
    } catch (const std::exception& ex) {
        qWarning() << "[CommandClient] Exception checking response:" << QString::fromUtf8(ex.what());
        client_.reset();
        return {false, QString(), false};
    }
}

QString CommandClient::buildCommandRecord(const QString& command, int address, const QString& parameters) const
{
    const qint64 timestamp = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    return QStringLiteral("%1|%2|%3|%4")
        .arg(timestamp)
        .arg(command)
        .arg(address)
        .arg(parameters);
}

QString CommandClient::commandKeyForAddress(int address) const
{
    return QStringLiteral("command:%1").arg(address);
}

QString CommandClient::generateUniqueId() const
{
    // Generate UUID without braces
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString CommandClient::commandResponseKey(const QString& command, int address, const QString& uniqueId) const
{
    // Format: commandres:<command>:<address>:<uniqueId>
    return QStringLiteral("commandres:%1:%2:%3")
        .arg(command)
        .arg(address)
        .arg(uniqueId);
}

} // namespace exo::redis

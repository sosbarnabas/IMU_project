#pragma once

#include <QObject>
#include <QVector>
#include <QHash>
#include <QString>
#include <memory>
#include "CommandClient.h"
namespace exo::redis { 
    class CommandClient; 
}

/**
 * @brief Manages slot functions for motors including fetching, caching and default initialization
 */
class SlotFunctionManager : public QObject {
    Q_OBJECT

public:
    explicit SlotFunctionManager(QObject* parent = nullptr);
    ~SlotFunctionManager() override = default;

    /**
     * @brief Set the Redis URI for fetching functions
     */
    void setRedisUri(const QString& uri);

    /**
     * @brief Fetch function data for a specific motor address and slot
     * @param motorAddress Motor address
     * @param slotId Slot ID to fetch
     */
    void fetchSlotFunction(int motorAddress, int slotId);

    /**
     * @brief Load default function for slot 1 and upload to all motors
     * @param motorAddresses Map of motor addresses to names
     */
    void initializeDefaultFunctions(const QHash<int, QString>& motorAddresses);

    /**
     * @brief Get cached function data for a motor/slot combination
     * @param motorAddress Motor address
     * @param slotId Slot ID
     * @return Function values (empty if not cached or not found)
     */
    QVector<double> getCachedFunction(int motorAddress, int slotId) const;

    /**
     * @brief Cache a function that was uploaded externally
     * @param motorAddress Motor address
     * @param slotId Slot ID
     * @param values Function values (360 points)
     */
    void cacheFunction(int motorAddress, int slotId, const QVector<double>& values);

signals:
    /**
     * @brief Emitted when a slot function is fetched successfully
     * @param motorAddress Motor address
     * @param slotId Slot ID
     * @param values Function values (360 points)
     */
    void functionFetched(int motorAddress, int slotId, const QVector<double>& values);

    /**
     * @brief Emitted when a slot function fetch fails or slot is empty
     * @param motorAddress Motor address
     * @param slotId Slot ID
     */
    void functionEmpty(int motorAddress, int slotId);

    /**
     * @brief Emitted when default functions are initialized
     */
    void defaultFunctionsInitialized();

private:
    bool loadDefaultFunction(QVector<double>& outValues);
    bool uploadFunctionToMotor(int address, int slot, const QVector<double>& values);
    QString functionCacheKey(int motorAddress, int slotId) const;

    std::unique_ptr<exo::redis::CommandClient> commandClient_;
    QHash<QString, QVector<double>> functionCache_;
};

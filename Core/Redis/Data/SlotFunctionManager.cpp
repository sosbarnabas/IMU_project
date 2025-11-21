#include "SlotFunctionManager.h"
#include "CommandClient.h"
#include "../../Widgets/UIUtilities/Constants.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QtConcurrent>

SlotFunctionManager::SlotFunctionManager(QObject* parent)
    : QObject(parent)
    , commandClient_(std::make_unique<exo::redis::CommandClient>(QString()))
{
}

void SlotFunctionManager::setRedisUri(const QString& uri)
{
    qDebug() << "[SlotFunctionManager] Setting Redis URI:" << uri;
    if (commandClient_) {
        commandClient_->setUri(uri);
    }
}

QString SlotFunctionManager::functionCacheKey(int motorAddress, int slotId) const
{
    return QStringLiteral("%1:%2").arg(motorAddress).arg(slotId);
}

QVector<double> SlotFunctionManager::getCachedFunction(int motorAddress, int slotId) const
{
    const QString key = functionCacheKey(motorAddress, slotId);
    return functionCache_.value(key);
}

void SlotFunctionManager::cacheFunction(int motorAddress, int slotId, const QVector<double>& values)
{
    const QString cacheKey = functionCacheKey(motorAddress, slotId);
    functionCache_[cacheKey] = values;
    qDebug() << "[SlotFunctionManager] Cached function for address:" << motorAddress << "slot:" << slotId
             << "with" << values.size() << "values";
}

void SlotFunctionManager::fetchSlotFunction(int motorAddress, int slotId)
{
    qDebug() << "[SlotFunctionManager] Fetching function for address:" << motorAddress << "slot:" << slotId;
    
    // Check cache first
    const QString cacheKey = functionCacheKey(motorAddress, slotId);
    if (functionCache_.contains(cacheKey)) {
        qDebug() << "[SlotFunctionManager] Returning cached function";
        emit functionFetched(motorAddress, slotId, functionCache_[cacheKey]);
        return;
    }

    // For now, we don't have a Redis command to fetch function data from a slot
    // This would require implementing a new command in the firmware/backend
    // As a placeholder, we'll emit functionEmpty for slots other than slot 1
    // Slot 1 will have the default function loaded at startup
    
    qDebug() << "[SlotFunctionManager] No cached function found, emitting empty";
    emit functionEmpty(motorAddress, slotId);
}

bool SlotFunctionManager::loadDefaultFunction(QVector<double>& outValues)
{
    // Try to load default function from Data/SpringFunctions/default_slot1.json
    const QString appPath = QCoreApplication::applicationDirPath();
    const QStringList possiblePaths = {
        appPath + "/../Data/SpringFunctions/default_slot1.json",
        appPath + "/Data/SpringFunctions/default_slot1.json",
        QDir::currentPath() + "/Data/SpringFunctions/default_slot1.json"
    };

    QString filePath;
    for (const QString& path : possiblePaths) {
        if (QFile::exists(path)) {
            filePath = path;
            break;
        }
    }

    if (filePath.isEmpty()) {
        qWarning() << "[SlotFunctionManager] Default function file not found in any expected location";
        return false;
    }

    qDebug() << "[SlotFunctionManager] Loading default function from:" << filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[SlotFunctionManager] Failed to open default function file:" << filePath;
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "[SlotFunctionManager] Invalid JSON in default function file:" << parseError.errorString();
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonValue pointsValue = root.value(QStringLiteral("points"));
    if (!pointsValue.isArray()) {
        qWarning() << "[SlotFunctionManager] Default function missing 'points' array";
        return false;
    }

    const QJsonArray pointsArray = pointsValue.toArray();
    QVector<QPoint> points;
    points.reserve(pointsArray.size());

    for (const QJsonValue& value : pointsArray) {
        if (!value.isObject())
            continue;
        const QJsonObject obj = value.toObject();
        if (!obj.contains(QStringLiteral("x")) || !obj.contains(QStringLiteral("y")))
            continue;
        const QJsonValue xVal = obj.value(QStringLiteral("x"));
        const QJsonValue yVal = obj.value(QStringLiteral("y"));
        if (!xVal.isDouble() || !yVal.isDouble())
            continue;
        const int x = qRound(xVal.toDouble());
        const int y = qRound(yVal.toDouble());
        points.append(QPoint(x, y));
    }

    if (points.isEmpty()) {
        qWarning() << "[SlotFunctionManager] No valid control points in default function";
        return false;
    }

    // Convert control points to UI::SpringFunction::POINTS_COUNT-element array using linear interpolation
    outValues.clear();
    outValues.reserve(UI::SpringFunction::POINTS_COUNT);

    for (int x = 0; x < UI::SpringFunction::POINTS_COUNT; ++x) {
        // Find the two control points that surround this x value
        int leftIdx = 0;
        int rightIdx = points.size() - 1;

        for (int i = 0; i < points.size() - 1; ++i) {
            if (points[i].x() <= x && points[i + 1].x() >= x) {
                leftIdx = i;
                rightIdx = i + 1;
                break;
            }
        }

        const QPoint& left = points[leftIdx];
        const QPoint& right = points[rightIdx];

        double y;
        if (left.x() == right.x()) {
            y = left.y();
        } else {
            const double t = double(x - left.x()) / (right.x() - left.x());
            y = left.y() + t * (right.y() - left.y());
        }

        outValues.append(y);
    }

    qDebug() << "[SlotFunctionManager] Loaded default function with" << points.size() << "control points," 
             << outValues.size() << "interpolated values";
    return true;
}

bool SlotFunctionManager::uploadFunctionToMotor(int address, int slot, const QVector<double>& values)
{
    qDebug() << "[SlotFunctionManager] Uploading function to address:" << address << "slot:" << slot;

    if (!commandClient_) {
        qWarning() << "[SlotFunctionManager] CommandClient is null";
        return false;
    }

    if (!commandClient_->ensureConnection()) {
        qWarning() << "[SlotFunctionManager] Failed to connect to Redis";
        return false;
    }

    if (values.size() != UI::SpringFunction::POINTS_COUNT) {
        qWarning() << "[SlotFunctionManager] Invalid values size:" << values.size() << "(expected" << UI::SpringFunction::POINTS_COUNT << ")";
        return false;
    }

    // Build parameters: slot number followed by UI::SpringFunction::POINTS_COUNT values
    QString parameters ="["+QString::number(slot);
    for (const double& value : values) {
        const int intValue = qRound(value);
        parameters += "," + QString::number(intValue);
    }
    parameters +="]";
    const auto result = commandClient_->sendCommand("fn_upload", address, parameters);

    if (!result.success) {
        qWarning() << "[SlotFunctionManager] fn_upload failed:" << result.errorMessage;
        return false;
    }

    // Cache the uploaded function
    const QString cacheKey = functionCacheKey(address, slot);
    functionCache_[cacheKey] = values;

    qDebug() << "[SlotFunctionManager] Successfully uploaded and cached function";
    return true;
}

void SlotFunctionManager::initializeDefaultFunctions(const QHash<int, QString>& motorAddresses)
{
    if (motorAddresses.isEmpty()) {
        qDebug() << "[SlotFunctionManager] No motors to initialize";
        return;
    }

    qDebug() << "[SlotFunctionManager] Initializing default functions for" << motorAddresses.size() << "motors";

    // Load default function
    QVector<double> defaultValues;
    if (!loadDefaultFunction(defaultValues)) {
        qWarning() << "[SlotFunctionManager] Failed to load default function";
        return;
    }

    // Upload to all motors in slot 1 - run async to avoid blocking UI
    QtConcurrent::run([this, motorAddresses, defaultValues]() {
        int successCount = 0;
        int failCount = 0;

        QList<int> addresses = motorAddresses.keys();
        for (int addr : addresses) {
            if (uploadFunctionToMotor(addr, 1, defaultValues)) {
                ++successCount;
            } else {
                ++failCount;
            }
        }

        qDebug() << "[SlotFunctionManager] Default function initialization complete: succeeded=" 
                 << successCount << "failed=" << failCount;
        
        emit defaultFunctionsInitialized();
    });
}

#pragma once

#include <QObject>
#include <QMap>
#include <QHash>
#include <QStringList>
#include <QJsonObject>
#include <QTimer>
#include <memory>

class HomePage;
class SettingsPage;
class SlotWidget;
class RedisTelemetryClient;
class SlotFunctionManager;
struct TelemetrySample;

namespace exoskeleton::redis {
    class Facade;
}
namespace exoskeleton::settings {
    class Settings;
    class UserParams;
}

class TelemetryController : public QObject {
    Q_OBJECT
public:
    explicit TelemetryController(HomePage *home, SettingsPage *settings, QObject *parent = nullptr);
    ~TelemetryController() override;

    QString redisUri() const { return m_redisUri; }
    SlotFunctionManager* functionManager() const { return m_functionManager; }
    
    // Access to Redis facade for advanced operations
    exoskeleton::redis::Facade* redisFacade() const { return m_redisFacade.get(); }
signals:
    void addressesUpdated(const QHash<int, QString>& addresses);
    void redisUriChanged(const QString& uri);
    void settingsLoaded(const exoskeleton::settings::Settings& settings);
    void userParamsLoaded(const exoskeleton::settings::UserParams& params);

private slots:
    void handleEnvLoaded(const QJsonObject &env);
    void handleAddressesUpdated(const QHash<int, QString> &addresses);
    void handleSampleReceived(int addr, const TelemetrySample &sample);
    void handleErrorOccurred(const QString &message);
    void handleFunctionFetched(int motorAddress, int slotId, const QVector<double>& values);
    void handleFunctionEmpty(int motorAddress, int slotId);
    void handleDefaultFunctionsInitialized();
    void processBatchedUpdates();

private:
    void startTelemetry();
    void applyMotorTitles();
    void updateSlotTelemetry(const QString &motorName, int addr, const TelemetrySample &sample);
    void updateHomeMotors();
    void initializeDefaultFunctions();
    void loadRedisSettings();
    void checkUserParamsChanges();

    HomePage *m_home {nullptr};
    SettingsPage *m_settings {nullptr};
    RedisTelemetryClient *m_client {nullptr};
    SlotFunctionManager *m_functionManager {nullptr};
    std::unique_ptr<exoskeleton::redis::Facade> m_redisFacade;
    QHash<int, QString> m_addrToMotor;
    QHash<QString, QString> m_motorSerials;
    QStringList m_visibleMotors;
    QString m_redisUri;
    QHash<int, int> m_lastSlotForAddress; // Track last slot for each motor address
    bool m_defaultFunctionsInitialized {false};
    
    // Batching for performance optimization
    QTimer *m_batchTimer {nullptr};
    struct PendingUpdate {
        QString motorName;
        double timestamp;
        double position;
    };
    QVector<PendingUpdate> m_pendingUpdates;
    
    // User params monitoring
    QTimer *m_userParamsCheckTimer {nullptr};
};

#include "TelemetryController.h"

#include "RedisTelemetryClient.h"
#include "SlotFunctionManager.h"
#include "RedisFacade.h"
#include "Settings.h"
#include "UserParams.h"
#include "HomePage.h"
#include "SlotWidget.h"
#include "SettingsPage.h"

#include <QDebug>
#include <QProcessEnvironment>
#include <iostream>

namespace {

constexpr auto kDefaultRedisUri = "tcp://127.0.0.1:6379";

QString normalizedMotorName(QString name)
{
    if (name.isEmpty()) {
        return name;
    }
    return name.trimmed().toUpper();
}

QStringList normalizedMotorList(const QHash<int, QString> &addresses)
{
    QStringList motors;
    motors.reserve(addresses.size());
    for (auto it = addresses.cbegin(); it != addresses.cend(); ++it) {
        const auto normalized = normalizedMotorName(it.value());
        if (!normalized.isEmpty() && !motors.contains(normalized)) {
            motors.push_back(normalized);
        }
    }
    return motors;
}

QStringList motorsWithSerials(const QStringList &motors, const QHash<QString, QString> &serials)
{
    QStringList filtered;
    filtered.reserve(motors.size());

    for (const auto &motor : motors) {
        const auto normalized = normalizedMotorName(motor);
        if (normalized.isEmpty()) {
            continue;
        }

        const auto serial = serials.value(normalized);
        if (serial.isEmpty()) {
            continue;
        }

        if (!filtered.contains(normalized)) {
            filtered.push_back(normalized);
        }
    }

    return filtered;
}

QString redisUriFromEnvironment()
{
    const auto uri = qEnvironmentVariable("EXO_REDIS_URI");
    if (!uri.isEmpty()) {
        return uri;
    }
    return QString::fromLatin1(kDefaultRedisUri);
}

int blockTimeoutFromEnvironment(int fallback)
{
    bool ok = false;
    const auto value = qEnvironmentVariableIntValue("EXO_REDIS_BLOCK_MS", &ok);
    if (ok && value > 0) {
        return value;
    }
    return fallback;
}

} // namespace

TelemetryController::TelemetryController(HomePage *home, SettingsPage *settings, QObject *parent)
    : QObject(parent)
    , m_home(home)
    , m_settings(settings)
    , m_client(new RedisTelemetryClient(this))
    , m_functionManager(new SlotFunctionManager(this))
{
    connect(m_client, &RedisTelemetryClient::envLoaded, this, &TelemetryController::handleEnvLoaded);
    connect(m_client, &RedisTelemetryClient::addressesUpdated, this, &TelemetryController::handleAddressesUpdated);
    connect(m_client, &RedisTelemetryClient::sampleReceived, this, &TelemetryController::handleSampleReceived);
    connect(m_client, &RedisTelemetryClient::errorOccurred, this, &TelemetryController::handleErrorOccurred);

    // Connect SlotFunctionManager signals
    connect(m_functionManager, &SlotFunctionManager::functionFetched, 
            this, &TelemetryController::handleFunctionFetched);
    connect(m_functionManager, &SlotFunctionManager::functionEmpty,
            this, &TelemetryController::handleFunctionEmpty);
    connect(m_functionManager, &SlotFunctionManager::defaultFunctionsInitialized,
            this, &TelemetryController::handleDefaultFunctionsInitialized);

    const int blockMs = blockTimeoutFromEnvironment(250);
    m_client->setBlockMs(blockMs);
    m_redisUri = redisUriFromEnvironment();
    m_client->setRedisUri(m_redisUri);
    m_functionManager->setRedisUri(m_redisUri);

    if (m_home)
        m_home->setCommandRedisUri(m_redisUri);
    
    // Emit Redis URI for other components
    emit redisUriChanged(m_redisUri);
    
    // Initialize Redis Facade for Settings and UserParams
    try {
        qDebug() << "[TelemetryController] Creating RedisFacade with URI:" << m_redisUri;
        m_redisFacade = std::make_unique<exoskeleton::redis::Facade>(m_redisUri.toStdString());
        
        // Initialize conf:env from .env file if it doesn't exist
        qDebug() << "[TelemetryController] Checking for .env file initialization...";
        std::cout << "[TelemetryController] About to call initialize_env_from_file..." << std::endl;
        
        if (m_redisFacade->initialize_env_from_file("C:/Users/orban/CLionProjects/ExoGui/.env", false)) {
            qDebug() << "[TelemetryController] conf:env initialized from .env file";
            std::cout << "[TelemetryController] conf:env successfully initialized from .env" << std::endl;
        } else {
            qDebug() << "[TelemetryController] conf:env already exists or no keys uploaded";
            std::cout << "[TelemetryController] conf:env already exists or initialization skipped" << std::endl;
        }
        
        // Initialize run:addrs from conf:env if it doesn't exist
        qDebug() << "[TelemetryController] Initializing run:addrs from conf:env...";
       m_redisFacade->initialize_run_addrs_from_env();
        
        loadRedisSettings();
    } catch (const std::exception& e) {
        qWarning() << "Failed to initialize RedisFacade:" << e.what();
        std::cerr << "[TelemetryController] Exception: " << e.what() << std::endl;
    }
    
    // Setup batch processing timer for performance
    m_batchTimer = new QTimer(this);
    m_batchTimer->setInterval(50);  // 20 Hz update rate
    m_batchTimer->setSingleShot(false);
    connect(m_batchTimer, &QTimer::timeout, this, &TelemetryController::processBatchedUpdates);
    m_batchTimer->start();
    
    // Reserve space for pending updates
    m_pendingUpdates.reserve(100);
    
    // Setup user params monitoring timer
    m_userParamsCheckTimer = new QTimer(this);
    m_userParamsCheckTimer->setInterval(1000);  // Check every second
    connect(m_userParamsCheckTimer, &QTimer::timeout, this, &TelemetryController::checkUserParamsChanges);
    m_userParamsCheckTimer->start();

    startTelemetry();
}

TelemetryController::~TelemetryController()
{
    if (m_client) {
        m_client->stop();
    }
}

void TelemetryController::startTelemetry()
{
    if (!m_client) {
        return;
    }

    if (!m_client->start()) {
        qWarning() << "RedisTelemetryClient could not be started";
    }
}

void TelemetryController::handleEnvLoaded(const QJsonObject &env)
{
    QHash<QString, QString> serials;
    serials.reserve(env.size());

    for (auto it = env.constBegin(); it != env.constEnd(); ++it) {
        const auto key = it.key();
        if (!key.startsWith(QStringLiteral("motor_"), Qt::CaseInsensitive)) {
            continue;
        }

        const QString normalized = normalizedMotorName(key);
        const QString value = it.value().toString();
        serials.insert(normalized, value);
    }

    m_motorSerials = serials;
    if (m_settings)
        m_settings->setSerialsFromTelemetry(serials);
    updateHomeMotors();
    applyMotorTitles();
}

void TelemetryController::handleAddressesUpdated(const QHash<int, QString> &addresses)
{
    if (addresses.isEmpty()) {
        return;
    }

    QHash<int, QString> normalizedAddresses;
    normalizedAddresses.reserve(addresses.size());
    for (auto it = addresses.cbegin(); it != addresses.cend(); ++it) {
        const QString normalized = normalizedMotorName(it.value());
        if (normalized.isEmpty()) {
            continue;
        }
        normalizedAddresses.insert(it.key(), normalized);
    }

    if (normalizedAddresses.isEmpty()) {
        return;
    }

    m_addrToMotor = normalizedAddresses;
    if (m_home)
        m_home->setMotorAddresses(m_addrToMotor);
    
    // Emit addresses for other components (e.g., SpringPage)
    emit addressesUpdated(m_addrToMotor);
    
    const auto motors = normalizedMotorList(m_addrToMotor);
    if (motors != m_visibleMotors) {
        m_visibleMotors = motors;
        updateHomeMotors();
        if (m_settings) {
            m_settings->setEnabledFromTelemetry(m_visibleMotors);
        }
    }

    applyMotorTitles();
    
    // Initialize default functions once we have addresses
    if (!m_defaultFunctionsInitialized) {
        initializeDefaultFunctions();
    }
}

void TelemetryController::handleSampleReceived(int addr, const TelemetrySample &sample)
{
    const auto motorName = m_addrToMotor.value(addr);
    if (motorName.isEmpty()) {
        return;
    }

    updateSlotTelemetry(motorName, addr, sample);
    
    // Batch position data for LivePlot (only if motor is enabled)
    if (m_home && sample.enabled) {
        // Convert nanoseconds timestamp to seconds
        double timestamp = sample.t_ns / 1e9;
        
        // Add to pending updates instead of immediate update
        m_pendingUpdates.append({motorName, timestamp, static_cast<double>(sample.position)});
    }
}

void TelemetryController::processBatchedUpdates()
{
    if (m_pendingUpdates.isEmpty() || !m_home) {
        return;
    }
    
    // Process all pending updates in batch
    for (const auto& update : m_pendingUpdates) {
        m_home->updateMotorData(update.motorName, update.timestamp, update.position);
    }
    
    m_pendingUpdates.clear();
}

void TelemetryController::updateHomeMotors()
{
    if (!m_home) {
        return;
    }

    const auto filtered = motorsWithSerials(m_visibleMotors, m_motorSerials);
    m_home->setMotors(filtered);
}

void TelemetryController::handleErrorOccurred(const QString &message)
{
    qWarning() << "RedisTelemetryClient error:" << message;
}

void TelemetryController::applyMotorTitles()
{
    if (!m_home) {
        return;
    }

    for (auto it = m_addrToMotor.cbegin(); it != m_addrToMotor.cend(); ++it) {
        const QString &motorName = it.value();
        if (SlotWidget *slot = m_home->slotForMotor(motorName)) {
            const QString serial = m_motorSerials.value(motorName);
            if (!serial.isEmpty()) {
                slot->setTitle(serial);
            } else {
                slot->setTitle(motorName);
            }
        }
    }
}

void TelemetryController::updateSlotTelemetry(const QString &motorName, int addr, const TelemetrySample &sample)
{
    if (!m_home) {
        return;
    }

    if (SlotWidget *slot = m_home->slotForMotor(motorName)) {
        // Check if enabled state changed
        static QHash<int, bool> lastEnabledState;
        bool wasEnabled = lastEnabledState.value(addr, true);
        if (wasEnabled != sample.enabled) {
            lastEnabledState[addr] = sample.enabled;
            // Notify HomePage about motor enabled state change
            m_home->onMotorEnabled(motorName, sample.enabled);
        }
        
        slot->setMotorActive(sample.enabled);
        slot->setValues(sample.position, sample.torque);
        slot->setSlotId(sample.slot_idx);
        slot->setToolTip(tr("Motor: %1\nAddress: %2\nCmd counter: %3\nRetries: %4\nTimestamp (ns): %5")
                             .arg(motorName)
                             .arg(addr)
                             .arg(sample.cmd_cntr)
                             .arg(sample.n_tries)
                             .arg(sample.t_ns));
        
        // Check if slot changed for this motor
        const int lastSlot = m_lastSlotForAddress.value(addr, -1);
        if (lastSlot != sample.slot_idx) {
            qDebug() << "[TelemetryController] Slot changed for address" << addr 
                     << "from" << lastSlot << "to" << sample.slot_idx;
            m_lastSlotForAddress[addr] = sample.slot_idx;
            
            // Fetch the function for this slot
            if (m_functionManager) {
                m_functionManager->fetchSlotFunction(addr, sample.slot_idx);
            }
        }
    }
}

void TelemetryController::initializeDefaultFunctions()
{
    if (!m_functionManager || m_addrToMotor.isEmpty()) {
        return;
    }
    
    qDebug() << "[TelemetryController] Initializing default functions for" << m_addrToMotor.size() << "motors";
    m_defaultFunctionsInitialized = true;
    m_functionManager->initializeDefaultFunctions(m_addrToMotor);
}

void TelemetryController::handleFunctionFetched(int motorAddress, int slotId, const QVector<double>& values)
{
    if (!m_home) {
        return;
    }
    
    const QString motorName = m_addrToMotor.value(motorAddress);
    if (motorName.isEmpty()) {
        return;
    }
    
    qDebug() << "[TelemetryController] Function fetched for" << motorName << "slot" << slotId 
             << "with" << values.size() << "values";
    
    if (SlotWidget *slot = m_home->slotForMotor(motorName)) {
        slot->setGraph(values);
        slot->setFunctionEmpty(false);
    }
}

void TelemetryController::handleFunctionEmpty(int motorAddress, int slotId)
{
    if (!m_home) {
        return;
    }
    
    const QString motorName = m_addrToMotor.value(motorAddress);
    if (motorName.isEmpty()) {
        return;
    }
    
    qDebug() << "[TelemetryController] Function empty for" << motorName << "slot" << slotId;
    
    if (SlotWidget *slot = m_home->slotForMotor(motorName)) {
        slot->setFunctionEmpty(true);
    }
}

void TelemetryController::handleDefaultFunctionsInitialized()
{
    qDebug() << "[TelemetryController] Default functions initialization completed";
    
    // After default functions are uploaded, motors may be in slot 1
    // Fetch the default function for all motors in slot 1
    if (!m_functionManager) {
        return;
    }
    
    for (auto it = m_addrToMotor.cbegin(); it != m_addrToMotor.cend(); ++it) {
        const int addr = it.key();
        // Check if this motor is in slot 1
        const int currentSlot = m_lastSlotForAddress.value(addr, -1);
        if (currentSlot == 1) {
            m_functionManager->fetchSlotFunction(addr, 1);
        }
    }
}

void TelemetryController::loadRedisSettings()
{
    if (!m_redisFacade) {
        return;
    }
    
    try {
        // Load environment settings (conf:env)
        auto settings = m_redisFacade->load_env(false);
        qDebug() << "[TelemetryController] Loaded Redis settings:";
        qDebug() << "  - multiport_motors:" << settings.multiport_motors;
        qDebug() << "  - mock_motors:" << settings.mock_motors;
        qDebug() << "  - restapi_port:" << settings.restapi_port;
        
        emit settingsLoaded(settings);
        
        // Try to load user params (conf:user)
        try {
            auto userParams = m_redisFacade->load_user_params();
            qDebug() << "[TelemetryController] Loaded user params:";
            qDebug() << "  - user_id:" << userParams.user_id;
            qDebug() << "  - motorforce:" << userParams.motorforce;
            qDebug() << "  - assist:" << userParams.assist;
            qDebug() << "  - bodyweight:" << userParams.bodyweight;
            
            emit userParamsLoaded(userParams);
        } catch (const std::exception& e) {
            qDebug() << "[TelemetryController] User params not available:" << e.what();
        }
        
    } catch (const std::exception& e) {
        qWarning() << "[TelemetryController] Failed to load Redis settings:" << e.what();
    }
}

void TelemetryController::checkUserParamsChanges()
{
    if (!m_redisFacade) {
        return;
    }
    
    try {
        // Check if user params changed (conf:user:set flag)
        if (m_redisFacade->user_params_changed(true)) {
            qDebug() << "[TelemetryController] User params changed, reloading...";
            auto userParams = m_redisFacade->load_user_params();
            emit userParamsLoaded(userParams);
        }
        
        // Check if DB params changed (dbchanged flag)
        if (m_redisFacade->db_params_changed(true)) {
            qDebug() << "[TelemetryController] DB params changed, reloading settings...";
            loadRedisSettings();
        }
    } catch (const std::exception& e) {
        // Silently ignore errors during monitoring
    }
}

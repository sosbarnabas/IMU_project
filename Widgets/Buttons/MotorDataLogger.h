#pragma once

#include <QObject>
#include <QString>
#include <QHash>
#include <QStringList>
#include <QDateTime>
#include <memory>

class QFile;
class QTextStream;
class QTimer;

/**
 * @brief Handles motor telemetry data logging to CSV files
 * 
 * Supports two modes:
 * - Manual: Log current snapshot on demand
 * - Continuous: Log all telemetry data with periodic flushing
 */
class MotorDataLogger : public QObject {
    Q_OBJECT

public:
    explicit MotorDataLogger(QObject* parent = nullptr);
    ~MotorDataLogger() override;

    // Logging state
    bool isLogging() const { return isLogging_; }
    
    // Update motor data (called from telemetry)
    void updateMotorData(const QString& motorName, double timestamp, double position, double torque);
    
public slots:
    // Manual logging (snapshot)
    void logManualSnapshot();
    
    // Continuous logging
    void startContinuousLogging();
    void stopContinuousLogging();

signals:
    void loggingStarted(const QString& fileName);
    void loggingStopped(const QString& fileName, int recordCount);
    void loggingError(const QString& error);
    void statusMessage(const QString& message);

private:
    void writeLogRow();
    QString selectSaveFile(const QString& defaultName);

    // Logging state
    bool isLogging_ = false;
    QDateTime loggingStartTime_;
    double lastLoggedTimestamp_ = 0.0;
    int recordCount_ = 0;
    
    // Motor data cache (motorName → value)
    QHash<QString, double> lastPosition_;
    QHash<QString, double> lastTorque_;
    
    // Motor order (fixed for consistent CSV columns)
    QStringList motorOrder_;
    
    // File I/O
    std::unique_ptr<QFile> logFile_;
    std::unique_ptr<QTextStream> logStream_;
    
    // Periodic flush timer
    QTimer* logFlushTimer_ = nullptr;
    static constexpr int FLUSH_INTERVAL_MS = 100; // 10 Hz
};

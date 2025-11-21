#include "MotorDataLogger.h"
#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QDir>
#include <QTimer>
#include <QDebug>

MotorDataLogger::MotorDataLogger(QObject* parent)
    : QObject(parent)
{
    // Fixed motor order for consistent CSV columns
    motorOrder_ = {
        "MOTOR_E_FLEX", "MOTOR_E_EXT", "MOTOR_S_FLEX", "MOTOR_S_EXT",
        "MOTOR_S_ADD_PRON", "MOTOR_S_ABD", "MOTOR_S_ADD_SUP"
    };
    
    // Initialize all motors with 0 values
    for (const QString& motorName : motorOrder_) {
        lastPosition_[motorName] = 0.0;
        lastTorque_[motorName] = 0.0;
    }
    
    // Setup periodic flush timer
    logFlushTimer_ = new QTimer(this);
    logFlushTimer_->setInterval(FLUSH_INTERVAL_MS);
    connect(logFlushTimer_, &QTimer::timeout, this, &MotorDataLogger::writeLogRow);
}

MotorDataLogger::~MotorDataLogger()
{
    stopContinuousLogging();
}

void MotorDataLogger::updateMotorData(const QString& motorName, double timestamp, 
                                      double position, double torque)
{
    lastPosition_[motorName] = position;
    lastTorque_[motorName] = torque;
    
    // Update timestamp only if newer
    if (timestamp > lastLoggedTimestamp_) {
        lastLoggedTimestamp_ = timestamp;
    }
}

void MotorDataLogger::logManualSnapshot()
{
    QString fileName = selectSaveFile("telemetry_snapshot_" + 
                                     QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".csv");
    
    if (fileName.isEmpty()) {
        return;
    }
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit loggingError(tr("Could not open file for writing: %1").arg(fileName));
        return;
    }
    
    QTextStream out(&file);
    
    // Write header
    out << "Timestamp";
    for (const QString& motorName : motorOrder_) {
        out << ";" << motorName << "_pos;" << motorName << "_torq";
    }
    out << "\n";
    
    // Write single data row
    out << QString::number(lastLoggedTimestamp_, 'f', 6);
    for (const QString& motorName : motorOrder_) {
        out << ";" << QString::number(lastPosition_.value(motorName, 0.0), 'f', 6)
            << ";" << QString::number(lastTorque_.value(motorName, 0.0), 'f', 6);
    }
    out << "\n";
    
    file.close();
    
    emit statusMessage(tr("✓ Logged snapshot to %1").arg(QFileInfo(fileName).fileName()));
    qDebug() << "[MotorDataLogger] Logged snapshot to" << fileName;
}

void MotorDataLogger::startContinuousLogging()
{
    if (isLogging_) {
        emit loggingError(tr("Logging is already active. Stop it first."));
        return;
    }
    
    QString fileName = selectSaveFile("telemetry_continuous_" + 
                                     QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".csv");
    
    if (fileName.isEmpty()) {
        return;
    }
    
    // Open file for writing
    logFile_ = std::make_unique<QFile>(fileName);
    if (!logFile_->open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit loggingError(tr("Could not open file for writing: %1").arg(fileName));
        logFile_.reset();
        return;
    }
    
    logStream_ = std::make_unique<QTextStream>(logFile_.get());
    
    // Write CSV header
    *logStream_ << "Timestamp";
    for (const QString& motorName : motorOrder_) {
        *logStream_ << ";" << motorName << "_pos;" << motorName << "_torq";
    }
    *logStream_ << "\n";
    logStream_->flush();
    
    // Initialize state
    isLogging_ = true;
    loggingStartTime_ = QDateTime::currentDateTime();
    recordCount_ = 0;
    lastLoggedTimestamp_ = 0.0;
    
    // Start periodic log writing
    logFlushTimer_->start();
    
    emit loggingStarted(fileName);
    emit statusMessage(tr("✓ Started logging to %1").arg(QFileInfo(fileName).fileName()));
    qDebug() << "[MotorDataLogger] Started continuous logging to" << fileName;
}

void MotorDataLogger::stopContinuousLogging()
{
    if (!isLogging_) {
        return;
    }
    
    // Stop the timer
    logFlushTimer_->stop();
    
    // Write final row if there's pending data
    writeLogRow();
    
    // Close file
    QString fileName;
    if (logFile_) {
        fileName = logFile_->fileName();
        logFile_->close();
        logFile_.reset();
    }
    logStream_.reset();
    
    // Update state
    isLogging_ = false;
    
    // Calculate duration
    auto duration = loggingStartTime_.secsTo(QDateTime::currentDateTime());
    
    emit loggingStopped(fileName, recordCount_);
    emit statusMessage(tr("✓ Stopped logging. Saved %1 records (%2s)")
                      .arg(recordCount_)
                      .arg(duration));
    
    qDebug() << "[MotorDataLogger] Stopped logging. Saved" << recordCount_ 
             << "records to" << fileName;
}

void MotorDataLogger::writeLogRow()
{
    if (!isLogging_ || !logStream_) {
        return;
    }
    
    // Write timestamp and all motor data
    *logStream_ << QString::number(lastLoggedTimestamp_, 'f', 6);
    
    for (const QString& motorName : motorOrder_) {
        *logStream_ << ";" << QString::number(lastPosition_.value(motorName, 0.0), 'f', 6)
                    << ";" << QString::number(lastTorque_.value(motorName, 0.0), 'f', 6);
    }
    
    *logStream_ << "\n";
    logStream_->flush();
    
    recordCount_++;
}

QString MotorDataLogger::selectSaveFile(const QString& defaultName)
{
    QString defaultPath = QDir::homePath() + "/" + defaultName;
    
    QString fileName = QFileDialog::getSaveFileName(nullptr,
                                                    tr("Save Telemetry Log"),
                                                    defaultPath,
                                                    tr("CSV Files (*.csv);;All Files (*)"));
    
    if (fileName.isEmpty()) {
        return QString();
    }
    
    // Ensure .csv extension
    if (!fileName.endsWith(".csv", Qt::CaseInsensitive)) {
        fileName.append(".csv");
    }
    
    return fileName;
}

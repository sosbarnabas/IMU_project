#include "ImuWidget.h"
#include "../UIUtilities/StyleHelper.h"
#include "../UIUtilities/Constants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QDebug>
#include <QDateTime>

#include <sw/redis++/redis++.h>
#include <iterator>

ImuWidget::ImuWidget(QWidget* parent)
    : QWidget(parent)
    , readTimer_(new QTimer(this))
    , lastStreamId_("0-0")
{
    setupUI();
    
    // Setup timer for reading data
    readTimer_->setInterval(0);  // 20 Hz refresh rate
    connect(readTimer_, &QTimer::timeout, this, &ImuWidget::readImuData);
}

ImuWidget::~ImuWidget()
{
    stopReading();
    disconnectFromRedis();
}

void ImuWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);
    
    // Title
    titleLabel_ = new QLabel("IMU Data (Euler Angles)", this);
    QFont titleFont = titleLabel_->font();
    titleFont.setPointSize(10);
    titleFont.setBold(true);
    titleLabel_->setFont(titleFont);
    titleLabel_->setAlignment(Qt::AlignCenter);
    titleLabel_->setStyleSheet(QString("color: %1;").arg(UI::Colors::TEXT));
    mainLayout->addWidget(titleLabel_);
    
    // Data display area with card background
    auto* dataCard = new QWidget(this);
    dataCard->setStyleSheet(QString(
        "QWidget {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 6px;"
        "}"
    ).arg(UI::Colors::CARD_BG).arg(UI::Colors::CARD_BORDER));
    
    auto* dataLayout = new QVBoxLayout(dataCard);
    dataLayout->setContentsMargins(12, 10, 12, 10);
    dataLayout->setSpacing(6);
    
    // Roll
    auto* rollRow = new QHBoxLayout();
    rollRow->setSpacing(8);
    auto* rollLabelTitle = new QLabel("Roll:", dataCard);
    rollLabelTitle->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;").arg(UI::Colors::TEXT));
    rollLabelTitle->setFixedWidth(50);
    rollLabel_ = new QLabel("0.00°", dataCard);
    rollLabel_->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: bold;").arg(UI::Colors::SUCCESS));
    rollLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rollLabel_->setMinimumWidth(70);
    rollRow->addWidget(rollLabelTitle);
    rollRow->addWidget(rollLabel_, 1);
    dataLayout->addLayout(rollRow);
    
    // Pitch
    auto* pitchRow = new QHBoxLayout();
    pitchRow->setSpacing(8);
    auto* pitchLabelTitle = new QLabel("Pitch:", dataCard);
    pitchLabelTitle->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;").arg(UI::Colors::TEXT));
    pitchLabelTitle->setFixedWidth(50);
    pitchLabel_ = new QLabel("0.00°", dataCard);
    pitchLabel_->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: bold;").arg(UI::Colors::SUCCESS));
    pitchLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pitchLabel_->setMinimumWidth(70);
    pitchRow->addWidget(pitchLabelTitle);
    pitchRow->addWidget(pitchLabel_, 1);
    dataLayout->addLayout(pitchRow);
    
    // Yaw
    auto* yawRow = new QHBoxLayout();
    yawRow->setSpacing(8);
    auto* yawLabelTitle = new QLabel("Yaw:", dataCard);
    yawLabelTitle->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;").arg(UI::Colors::TEXT));
    yawLabelTitle->setFixedWidth(50);
    yawLabel_ = new QLabel("0.00°", dataCard);
    yawLabel_->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: bold;").arg(UI::Colors::SUCCESS));
    yawLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    yawLabel_->setMinimumWidth(70);
    yawRow->addWidget(yawLabelTitle);
    yawRow->addWidget(yawLabel_, 1);
    dataLayout->addLayout(yawRow);
    
    mainLayout->addWidget(dataCard);



    // Apply overall styling and set fixed size
    setStyleSheet(QString("QWidget { background: transparent; }"));
    setFixedSize(200, 140);
}
void ImuWidget::setTelemetryClient(RedisTelemetryClient* client)
{
    telemetryClient_ = client;
}

void ImuWidget::setRedisUri(const QString& uri)
{
    if (redisUri_ == uri) {
        return;
    }
    
    bool wasReading = readTimer_->isActive();
    if (wasReading) {
        stopReading();
    }
    
    disconnectFromRedis();
    redisUri_ = uri;
    
    if (wasReading && !redisUri_.isEmpty()) {
        startReading();
    }
}

void ImuWidget::connectToRedis()
{
    if (connected_ || redisUri_.isEmpty()) {
        return;
    }
    
    try {
        sw::redis::ConnectionOptions opts;
        opts.type = sw::redis::ConnectionType::TCP;
        
        // Parse URI (format: tcp://host:port)
        QString uri = redisUri_;
        if (uri.startsWith("tcp://")) {
            uri = uri.mid(6);  // Remove "tcp://"
        }
        
        QStringList parts = uri.split(':');
        if (parts.size() == 2) {
            opts.host = parts[0].toStdString();
            opts.port = parts[1].toInt();
        } else {
            opts.host = "127.0.0.1";
            opts.port = 6379;
        }
        
        opts.socket_timeout = std::chrono::milliseconds(100);
        
        redis_ = std::make_unique<sw::redis::Redis>(opts);
        
        // Test connection
        redis_->ping();
        
        connected_ = true;
        
        qDebug() << "[ImuWidget] Connected to Redis:" << redisUri_;
        
    } catch (const std::exception& e) {
        connected_ = false;
        
        QString error = QString("Redis connection error: %1").arg(e.what());
        qWarning() << "[ImuWidget]" << error;
        emit errorOccurred(error);
    }
}

void ImuWidget::disconnectFromRedis()
{
    redis_.reset();
    connected_ = false;
}

void ImuWidget::startReading()
{
    if (redisUri_.isEmpty()) {
        qWarning() << "[ImuWidget] Cannot start reading: Redis URI not set";
        return;
    }
    
    connectToRedis();
    
    if (connected_) {
        lastStreamId_ = "$";  // Read only new messages
        readTimer_->start();
        qDebug() << "[ImuWidget] Started reading IMU data";
    }
}

void ImuWidget::stopReading()
{
    readTimer_->stop();
    qDebug() << "[ImuWidget] Stopped reading IMU data";
}

void ImuWidget::readImuData()
{
    if (!connected_ || !redis_) {
        return;
    }
    
    try {
        // Define type for stream entries (same as RedisTelemetryClient)
        using StreamEntry = std::pair<std::string, std::vector<std::pair<std::string, std::string>>>;
        using StreamEntries = std::vector<std::pair<std::string, std::vector<StreamEntry>>>;
        
        // XREAD BLOCK 10 COUNT 1 STREAMS xdata:imu:0 <lastId>
        std::unordered_map<std::string, std::string> streams;
        streams["xdata:imu:0"] = lastStreamId_.toStdString();
        
        StreamEntries result;
        redis_->xread(streams.begin(), streams.end(),
                     std::chrono::milliseconds(10),
                     1LL,
                     std::back_inserter(result));

        if (result.empty()) {
            return;  // No new data
        }
        
        // Process all new entries
        for (const auto& stream_pair : result) {
            for (const auto& entry : stream_pair.second) {
                // Update last ID
                lastStreamId_ = QString::fromStdString(entry.first);
                
                // Extract IMU data from the entry
                ImuData imuData;
                bool hasData = false;
                
                for (const auto& field_pair : entry.second) {
                    QString field = QString::fromStdString(field_pair.first);
                    QString value = QString::fromStdString(field_pair.second);
                    
                    if (field == "euler_roll") {
                        imuData.roll = value.toDouble();
                        hasData = true;
                    } else if (field == "euler_pitch") {
                        imuData.pitch = value.toDouble();
                        hasData = true;
                    } else if (field == "euler_yaw") {
                        imuData.yaw = value.toDouble();
                        hasData = true;
                    }
                }
                
                if (hasData) {
                    imuData.timestamp = QDateTime::currentMSecsSinceEpoch();
                    currentData_ = imuData;
                    updateDisplay(imuData);
                    emit dataUpdated(imuData);
                }
            }
        }
        
    } catch (const sw::redis::TimeoutError&) {

        // Timeout is normal, just means no new data
        return;
    } catch (const std::exception& e) {
        qWarning() << "[ImuWidget] Error reading IMU data:" << e.what();
        
        // Try to reconnect
        connected_ = false;
        
        disconnectFromRedis();
        QTimer::singleShot(1000, this, &ImuWidget::connectToRedis);
    }
}

void ImuWidget::updateDisplay(const ImuData& imuData)
{
    rollLabel_->setText(QString("%1°").arg(imuData.roll, 0, 'f', 2));
    pitchLabel_->setText(QString("%1°").arg(imuData.pitch, 0, 'f', 2));
    yawLabel_->setText(QString("%1°").arg(imuData.yaw, 0, 'f', 2));
}

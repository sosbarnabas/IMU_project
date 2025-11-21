#pragma once

#include <QWidget>
#include <QString>
#include <QTimer>

#include "RedisTelemetryClient.h"

class QLabel;
class QVBoxLayout;

namespace sw {
namespace redis {
class Redis;
}
}

struct ImuData {
    double roll {0.0};
    double pitch {0.0};
    double yaw {0.0};
    qint64 timestamp {0};
};

class ImuWidget : public QWidget {
    Q_OBJECT
public:
    explicit ImuWidget(QWidget* parent = nullptr);
    void setTelemetryClient(RedisTelemetryClient* client);   // <—
    ~ImuWidget() override;

    void setRedisUri(const QString& uri);
    void startReading();
    void stopReading();

signals:
    void errorOccurred(const QString& message);
    void dataUpdated(const ImuData& data);

private slots:
    void readImuData();

private:
    void setupUI();
    void updateDisplay(const ImuData& data);
    void connectToRedis();
    void disconnectFromRedis();

    QString redisUri_;
    std::unique_ptr<sw::redis::Redis> redis_;
    QTimer* readTimer_;
    QString lastStreamId_;

    // UI components
    QLabel* titleLabel_;
    QLabel* rollLabel_;
    QLabel* pitchLabel_;
    QLabel* yawLabel_;
    QLabel* statusLabel_;
    
    ImuData currentData_;
    bool connected_{false};

    //Trigger redis refsh to include imu data to logging
    RedisTelemetryClient* telemetryClient_ = nullptr;         // <—
    bool firstImuDetected_ = false;                           // <—
};

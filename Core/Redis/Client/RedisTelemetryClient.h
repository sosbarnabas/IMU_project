#pragma once

#include <QObject>
#include <QThread>
#include <QJsonObject>
#include <QString>
#include <QPointer>
#include <QMetaType>
#include <QMap>

#include <memory>

struct TelemetrySample {
    bool enabled {false};
    int slot_idx {0};
    int cmd_cntr {0};
    int position {0};
    int torque {0};
    int n_tries {0};
    qint64 t_ns {0};
};

Q_DECLARE_METATYPE(TelemetrySample)

class RedisTelemetryClient : public QObject {
    Q_OBJECT
public:
    explicit RedisTelemetryClient(QObject *parent = nullptr);
    ~RedisTelemetryClient() override;

    void setRedisUri(const QString &uri);
    void setBlockMs(int blockMs);

    bool start();
    void stop();
    
    // Trigger refresh of addresses and environment from Redis
    void refreshFromRedis();

    signals:
    void envLoaded(const QJsonObject &env);
    void sampleReceived(int addr, const TelemetrySample &sample);
    void logReceived(const QJsonObject &record);
    void errorOccurred(const QString &message);
    void addressesUpdated(const QHash<int, QString> &addresses);

private slots:
    void handleWorkerFinished();

private:
    class Worker;

    void destroyWorker();

    QString m_uri;
    int m_blockMs {17};  // Reduced blocking time to prevent UI freezing
    bool m_running {false};

    QThread *m_thread {nullptr};
    Worker *m_worker {nullptr};
};
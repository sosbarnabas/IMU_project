#include "RedisTelemetryClient.h"
#include "RedisKeys.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QHash>
#include <QMap>

#include <sw/redis++/redis++.h>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cctype>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std::chrono_literals;

namespace
{
    constexpr auto kInitialBackoff = std::chrono::milliseconds{30};
    constexpr auto kMaxBackoff = std::chrono::seconds{5};

    inline bool isTruthy(const std::string& value)
    {
        return value == "1" || value == "true" || value == "True" || value == "TRUE";
    }

    inline qint64 parseInteger(const std::string& value, qint64 fallback)
    {
        qint64 result = fallback;
        const auto* begin = value.data();
        const auto* end = value.data() + value.size();
        auto [ptr, ec] = std::from_chars(begin, end, result);
        if (ec == std::errc()) return result;
        try { return std::stoll(value); }
        catch (...) { return fallback; }
    }

    inline int parseInt(const std::string& value, int fallback)
    {
        int result = fallback;
        const auto* begin = value.data();
        const auto* end = value.data() + value.size();
        auto [ptr, ec] = std::from_chars(begin, end, result);
        if (ec == std::errc()) return result;
        try { return std::stoi(value); }
        catch (...) { return fallback; }
    }

    inline QJsonValue toJsonValue(const std::string& value)
    {
        return QJsonValue(QString::fromStdString(value));
    }

    // Redis XREAD kimenet saját típusai (redis++ 1.3.15 kompatibilis)
    using StreamField = std::pair<std::string, std::string>;
    using StreamEntry = std::pair<std::string, std::vector<StreamField>>; // id, fields
    using StreamEntries = std::vector<std::pair<std::string, std::vector<StreamEntry>>>; // key -> entries

    TelemetrySample createSample(const StreamEntry& entry)
    {
        TelemetrySample sample;
        for (const auto& field : entry.second)
        {
            const auto& name = field.first;
            const auto& value = field.second;

            if (name == exo::redis::keys::FIELD_ENABLED)
            {
                sample.enabled = isTruthy(value);
            }
            else if (name == exo::redis::keys::FIELD_SLOT_INDEX)
            {
                sample.slot_idx = parseInt(value, sample.slot_idx);
            }
            else if (name == exo::redis::keys::FIELD_CMD_COUNTER)
            {
                sample.cmd_cntr = parseInt(value, sample.cmd_cntr);
            }
            else if (name == exo::redis::keys::FIELD_POSITION)
            {
                sample.position = parseInt(value, sample.position);
            }
            else if (name == exo::redis::keys::FIELD_TORQUE)
            {
                sample.torque = parseInt(value, sample.torque);
            }
            else if (name == exo::redis::keys::FIELD_RETRY_COUNT)
            {
                sample.n_tries = parseInt(value, sample.n_tries);
            }
            else if (name == exo::redis::keys::FIELD_TIMESTAMP)
            {
                sample.t_ns = parseInteger(value, sample.t_ns);
            }
        }
        return sample;
    }
} // namespace

class RedisTelemetryClient::Worker : public QObject
{
    Q_OBJECT

public:
    Worker(QString uri, int blockMs, QObject* parent = nullptr)
        : QObject(parent), m_stop(false), m_uri(std::move(uri)), m_blockMs(blockMs)
    {
    }

public slots:
    void startProcessing()
    {
        m_stop.store(false);
        auto backoff = kInitialBackoff;
        while (!m_stop.load(std::memory_order_relaxed))
        {
            try
            {
                if (!ensureClient()) break;

                emitEnvironment();
                refreshAddresses();
                backoff = kInitialBackoff;
                telemetryLoop();

                if (m_stop.load(std::memory_order_relaxed)) break;
            }
            catch (const sw::redis::Error& err)
            {
                emit errorOccurred(tr("Redis error: %1").arg(QString::fromUtf8(err.what())));
            }
            catch (const std::exception& ex)
            {
                emit errorOccurred(tr("Unexpected error: %1").arg(QString::fromUtf8(ex.what())));
            }

            resetConnection();
            if (m_stop.load(std::memory_order_relaxed)) break;

            std::this_thread::sleep_for(backoff);
            backoff = std::min(backoff * 2,
                               std::chrono::duration_cast<decltype(backoff)>(kMaxBackoff));
        }
        emit finished();
    }

    void requestStop() { m_stop.store(true, std::memory_order_relaxed); }
    void updateBlockMs(int block) { m_blockMs = std::max(1, block); }
    void updateUri(const QString& uri) { m_uri = uri; }

signals:
    void envLoaded(const QJsonObject& env);
    void sampleReceived(int addr, const TelemetrySample& sample);
    void logReceived(const QJsonObject& record);
    void errorOccurred(const QString& message);
    void addressesUpdated(const QHash<int, QString>& addresses);
    void finished();

private:
    struct StreamState
    {
        int addr{0};
        std::string lastId{"0-0"};
        QString name;
    };

    bool ensureClient()
    {
        if (m_uri.isEmpty())
        {
            emit errorOccurred(tr("Redis URI not configured."));
            return false;
        }
        try
        {
            m_client = std::make_unique<sw::redis::Redis>(m_uri.toStdString());
            m_client->ping();
        }
        catch (const std::exception& ex)
        {
            emit errorOccurred(tr("Failed to connect to Redis server: %1")
                .arg(QString::fromUtf8(ex.what())));
            throw;
        }
        return true;
    }

    void resetConnection()
    {
        m_streamStates.clear();
        m_client.reset();
    }

    void emitEnvironment()
    {
        if (!m_client) return;

        std::unordered_map<std::string, std::string> envMap;
        m_client->hgetall(exo::redis::keys::CONF_ENV, std::inserter(envMap, envMap.end()));

        QJsonObject env;
        for (const auto& entry : envMap)
            env.insert(QString::fromStdString(entry.first), toJsonValue(entry.second));

        emit envLoaded(env);
    }

    void refreshAddresses()
    {
        if (!m_client) return;

        std::unordered_map<std::string, std::string> mapping;
        m_client->hgetall(exo::redis::keys::RUN_ADDRS, std::inserter(mapping, mapping.end()));

        m_streamStates.clear();

        QHash<int, QString> addresses;

        for (const auto& entry : mapping)
        {
            const auto& spec = entry.second;
            const auto sep = spec.find('|');
            if (sep == std::string::npos)
            {
                emit errorOccurred(tr("Invalid run:addrs entry: %1").arg(QString::fromStdString(spec)));
                continue;
            }

            auto addrPart = spec.substr(0, sep);
            const auto addr = parseInt(addrPart, -1);
            if (addr < 0)
            {
                emit errorOccurred(tr("Invalid address in run:addrs value: %1")
                    .arg(QString::fromStdString(addrPart)));
                continue;
            }

            // Get the motor name from the entry and convert it to the format expected by the UI
            QString motorName = QString::fromStdString(entry.first);

            // Prefix with "MOTOR_" if it doesn't already start with it, and convert to uppercase
            if (!motorName.startsWith("MOTOR_", Qt::CaseInsensitive))
            {
                motorName = "MOTOR_" + motorName.toUpper();
            }
            else
            {
                motorName = motorName.toUpper();
            }

            std::string streamKey = exo::redis::keys::XDATA_PREFIX + addrPart;
            m_streamStates.emplace(std::move(streamKey), StreamState{addr, "0-0", motorName});
            addresses.insert(addr, motorName);
        }

        if (!addresses.isEmpty()) emit addressesUpdated(addresses);
    }

    void telemetryLoop()
    {
        if (!m_client) return;

        while (!m_stop.load(std::memory_order_relaxed))
        {
            if (m_streamStates.empty())
            {
                std::this_thread::sleep_for(500ms);
                refreshAddresses();
                continue;
            }

            // Limit the number of streams to process at once to prevent UI freezing
            constexpr size_t MAX_STREAMS_PER_BATCH = 3;

            std::vector<std::pair<std::string, std::string>> streams;
            streams.reserve(std::min(m_streamStates.size(), MAX_STREAMS_PER_BATCH));

            // Create a temporary vector of all stream keys for rotation
            std::vector<std::string> allStreamKeys;
            allStreamKeys.reserve(m_streamStates.size());
            for (const auto& kv : m_streamStates)
            {
                allStreamKeys.push_back(kv.first);
            }

            // Process streams in a round-robin fashion
            static size_t startIdx = 0;
            size_t streamsAdded = 0;

            for (size_t i = 0; i < allStreamKeys.size() && streamsAdded < MAX_STREAMS_PER_BATCH; ++i)
            {
                size_t idx = (startIdx + i) % allStreamKeys.size();
                const auto& key = allStreamKeys[idx];
                const auto& state = m_streamStates[key];
                streams.emplace_back(key, state.lastId);
                streamsAdded++;
            }

            // Update start index for next round
            startIdx = (startIdx + streamsAdded) % allStreamKeys.size();

            StreamEntries result;
            try
            {
                // Process only a subset of streams with reduced timeout
                m_client->xread(streams.begin(), streams.end(),
                                std::chrono::milliseconds{m_blockMs},
                                1LL,
                                std::back_inserter(result));
            }
            catch (const sw::redis::TimeoutError&)
            {
                continue;
            }

            if (result.empty()) continue;

            for (const auto& stream : result)
            {
                const auto it = m_streamStates.find(stream.first);
                if (it == m_streamStates.end()) continue;

                auto& state = it->second;
                for (const auto& entry : stream.second)
                {
                    state.lastId = entry.first; // id
                    TelemetrySample sample = createSample(entry);
                    emit sampleReceived(state.addr, sample);
                }
            }
        }
    }

    std::atomic_bool m_stop;
    QString m_uri;
    int m_blockMs;
    std::unique_ptr<sw::redis::Redis> m_client;
    std::unordered_map<std::string, StreamState> m_streamStates;
};

RedisTelemetryClient::RedisTelemetryClient(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<TelemetrySample>("TelemetrySample");
}

RedisTelemetryClient::~RedisTelemetryClient()
{
    stop();
}

void RedisTelemetryClient::setRedisUri(const QString& uri)
{
    m_uri = uri;
    if (m_worker)
    {
        QMetaObject::invokeMethod(m_worker, "updateUri", Qt::QueuedConnection, Q_ARG(QString, uri));
    }
}

void RedisTelemetryClient::setBlockMs(int blockMs)
{
    m_blockMs = std::max(1, blockMs);
    if (m_worker)
    {
        QMetaObject::invokeMethod(m_worker, "updateBlockMs", Qt::QueuedConnection, Q_ARG(int, m_blockMs));
    }
}

bool RedisTelemetryClient::start()
{
    if (m_running) return false;

    if (m_uri.isEmpty())
    {
        emit errorOccurred(tr("Redis URI not configured."));
        return false;
    }

    m_thread = new QThread();
    m_thread->setObjectName(QStringLiteral("RedisTelemetryWorker"));

    m_worker = new Worker(m_uri, m_blockMs);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &Worker::startProcessing);
    connect(m_worker, &Worker::envLoaded, this, &RedisTelemetryClient::envLoaded);
    connect(m_worker, &Worker::sampleReceived, this, &RedisTelemetryClient::sampleReceived);
    connect(m_worker, &Worker::logReceived, this, &RedisTelemetryClient::logReceived);
    connect(m_worker, &Worker::errorOccurred, this, &RedisTelemetryClient::errorOccurred);
    connect(m_worker, &Worker::addressesUpdated, this, &RedisTelemetryClient::addressesUpdated);
    connect(m_worker, &Worker::finished, this, &RedisTelemetryClient::handleWorkerFinished);
    connect(m_worker, &Worker::finished, m_thread, &QThread::quit);

    m_thread->start();
    m_running = true;
    return true;
}

void RedisTelemetryClient::stop()
{
    if (!m_running && !m_thread) return;

    if (m_worker)
    {
        // A worker végtelen ciklusban fut a QThread eseményhurok blokkolása mellett,
        // ezért a QueuedConnection nem tud lefutni. A requestStop() csak egy atomikus
        // flaget állít, így közvetlenül meghívhatjuk más szálról is biztonságosan.
        m_worker->requestStop();
    }

    if (m_thread)
    {
        m_thread->quit();
        m_thread->wait();
    }

    destroyWorker();
    m_running = false;
}

void RedisTelemetryClient::refreshFromRedis()
{
    qDebug() << "[RedisTelemetryClient] Manually triggering refresh from Redis";

    if (m_worker)
    {
        // Use queued connection to ensure it runs on the worker thread
        QMetaObject::invokeMethod(m_worker, "refreshEnv", Qt::QueuedConnection);
        QMetaObject::invokeMethod(m_worker, "refreshAddresses", Qt::QueuedConnection);
        qDebug() << "[RedisTelemetryClient] Refresh triggered successfully";
    }
    else
    {
        qWarning() << "[RedisTelemetryClient] Cannot refresh: worker not initialized";
    }
}

void RedisTelemetryClient::handleWorkerFinished()
{
    m_running = false;
    destroyWorker();
}

void RedisTelemetryClient::destroyWorker()
{
    if (m_worker)
    {
        if (QThread* thread = m_worker->thread();
            thread && thread->isRunning() && thread != QThread::currentThread())
        {
            m_worker->deleteLater();
        }
        else
        {
            delete m_worker;
        }
        m_worker = nullptr;
    }

    if (m_thread)
    {
        if (m_thread->isRunning())
        {
            m_thread->quit();
            m_thread->wait();
        }
        delete m_thread;
        m_thread = nullptr;
    }
}

#include "RedisTelemetryClient.moc"

#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <memory>

namespace exoskeleton::redis {

/**
 * @brief Manages Redis server in Docker container
 * 
 * This class handles starting, stopping, and monitoring a Redis server
 * running in a Docker container. It ensures proper cleanup on application exit.
 */
class RedisDockerManager : public QObject {
    Q_OBJECT

public:
    explicit RedisDockerManager(QObject* parent = nullptr);
    ~RedisDockerManager() override;

    /**
     * @brief Start Redis server in Docker container
     * @param containerName Name for the Docker container
     * @param port Port to expose Redis on (default: 6379)
     * @param imageName Docker image to use (default: redis:latest)
     * @return true if Redis started successfully, false otherwise
     */
    bool startRedis(const QString& containerName = "exogui-redis",
                   int port = 6379,
                   const QString& imageName = "redis:latest");

    /**
     * @brief Stop and remove Redis container
     * @return true if cleanup successful, false otherwise
     */
    bool stopRedis();

    /**
     * @brief Check if Docker daemon is running
     * @return true if Docker is available, false otherwise
     */
    bool isDockerRunning() const;

    /**
     * @brief Ensure Docker daemon is running
     * Checks if Docker daemon is available. Does not attempt to start Docker.
     * @return true if Docker is running, false otherwise
     */
    bool ensureDockerRunning();

    /**
     * @brief Check if Redis container is running
     * @return true if container is running, false otherwise
     */
    bool isRedisRunning() const;

    /**
     * @brief Get the Redis connection URI
     * @return Connection string (e.g., "tcp://127.0.0.1:6379")
     */
    QString getRedisUri() const;

    /**
     * @brief Wait for Redis to be ready to accept connections
     * @param timeoutMs Maximum time to wait in milliseconds
     * @return true if Redis is ready, false if timeout
     */
    bool waitForRedisReady(int timeoutMs = 10000);
signals:
    /**
     * @brief Emitted when Redis container status changes
     * @param running true if Redis is running, false otherwise
     */
    void redisStatusChanged(bool running);

    /**
     * @brief Emitted when an error occurs
     * @param message Error message
     */
    void errorOccurred(const QString& message);

    /**
     * @brief Emitted when Redis is ready to accept connections
     */
    void redisReady();

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void checkRedisReadiness();

private:
    // Helper functions
    int runDockerCommand(const QStringList& args) const; // Simple execute wrapper
    bool executeDockerCommand(const QStringList& args, QString* output = nullptr, bool waitForFinished = true);
    bool checkContainerExists(const QString& containerName) const;
    bool checkContainerRunning(const QString& containerName) const;
    bool stopContainer(const QString& containerName);
    bool removeContainer(const QString& containerName);
    bool pullDockerImage(const QString& imageName);
    bool startContainer();
    QString getDockerErrorString(QProcess::ProcessError error) const;

    // Member variables
    QString m_containerName;
    int m_port;
    QString m_imageName;
    QString m_redisUri;
    bool m_isRunning;
    QString m_dockerCommand;  // Path to docker executable
    
    std::unique_ptr<QProcess> m_currentProcess;
    QTimer* m_readinessCheckTimer;
    int m_readinessCheckAttempts;
    static constexpr int MAX_READINESS_ATTEMPTS = 30;
};

} // namespace exoskeleton::redis

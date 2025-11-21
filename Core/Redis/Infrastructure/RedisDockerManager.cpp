#include "RedisDockerManager.h"
#include <QDebug>
#include <QEventLoop>
#include <QThread>
#include <cstdio>   // for popen/pclose

// Windows uses _popen/_pclose
#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace exoskeleton::redis {

RedisDockerManager::RedisDockerManager(QObject* parent)
    : QObject(parent)
    , m_port(6379)
    , m_isRunning(false)
    , m_readinessCheckAttempts(0)
    , m_dockerCommand("docker")  // Default to PATH
{
    m_readinessCheckTimer = new QTimer(this);
    m_readinessCheckTimer->setInterval(500); // Check every 500ms
    connect(m_readinessCheckTimer, &QTimer::timeout, this, &RedisDockerManager::checkRedisReadiness);
}

RedisDockerManager::~RedisDockerManager()
{
    qDebug() << "[RedisDockerManager] Destructor called, cleaning up Redis container";
    stopRedis();
}

bool RedisDockerManager::isDockerRunning() const
{
    // Try to find docker executable
    // QProcess::execute returns -2 if process cannot be started
    // -1 if it crashes, otherwise the exit code of the process
    
#ifdef _WIN32
    // Check if docker is in Program Files or PATH
    QStringList possiblePaths = {
        "docker", // Try PATH first
        "C:/Program Files/Docker/Docker/resources/bin/docker.exe",
        "C:/Program Files/Docker/Docker/resources/docker.exe",
        "C:/Program Files (x86)/Docker/Docker/resources/bin/docker.exe"
    };
    
    for (const QString& path : possiblePaths) {
        int testResult = QProcess::execute(path, QStringList() << "--version");
        if (testResult != -2) { // -2 means process failed to start
            // Found working docker command, save it for future use
            const_cast<RedisDockerManager*>(this)->m_dockerCommand = path;
            qDebug() << "[RedisDockerManager] Found docker at:" << path;
            break;
        }
    }
#endif
    
    int exitCode = QProcess::execute(m_dockerCommand, QStringList() << "ps" << "-q");
    
    qDebug() << "[RedisDockerManager] Docker check (using:" << m_dockerCommand << ") exit code:" << exitCode;
    
    if (exitCode == 0) {
        qDebug() << "[RedisDockerManager] Docker is responsive";
        return true;
    }
    
    if (exitCode == -2) {
        qDebug() << "[RedisDockerManager] Docker executable not found";
        qDebug() << "[RedisDockerManager] Make sure Docker Desktop is installed";
    }
    
    qDebug() << "[RedisDockerManager] Docker not responsive";
    return false;
}

bool RedisDockerManager::ensureDockerRunning()
{
    qDebug() << "[RedisDockerManager] Checking if Docker is running...";
    
    if (isDockerRunning()) {
        qDebug() << "[RedisDockerManager] Docker is already running and responsive";
        return true;
    }

    qWarning() << "[RedisDockerManager] Docker is not running";
    emit errorOccurred("Docker is not running. Please start Docker Desktop and try again.");
    return false;
}

bool RedisDockerManager::startRedis(const QString& containerName, int port, const QString& imageName)
{
    qDebug() << "[RedisDockerManager] Starting Redis with container:" << containerName 
             << "port:" << port << "image:" << imageName;

    m_containerName = containerName;
    m_port = port;
    m_imageName = imageName;
    m_redisUri = QString("tcp://127.0.0.1:%1").arg(port);

    // Step 1: Ensure Docker is running
    if (!ensureDockerRunning()) {
        emit errorOccurred("Failed to start Docker. Please start Docker Desktop manually.");
        qWarning() << "[RedisDockerManager] Docker could not be started";
        return false;
    }
    qDebug() << "[RedisDockerManager] ✓ Docker is running";

    // Step 2: Remove existing container if it exists (clean slate approach)
    if (checkContainerExists(m_containerName)) {
        qDebug() << "[RedisDockerManager] Found existing container, removing it for clean start...";
        runDockerCommand(QStringList() << "rm" << "-f" << m_containerName);
        qDebug() << "[RedisDockerManager] ✓ Old container removed";
    }

    // Step 3: Start new Redis container
    qDebug() << "[RedisDockerManager] Starting new Redis container...";
    
    QStringList runArgs;
    runArgs << "run" << "--name" << m_containerName
            << "-d" << "-p" << QString("%1:6379").arg(m_port)
            << "--restart" << "unless-stopped"
            << m_imageName
            << "redis-server" << "--appendonly" << "yes";
    
    int result = runDockerCommand(runArgs);
    
    if (result != 0) {
        qWarning() << "[RedisDockerManager] Failed to start Redis container (exit code:" << result << ")";
        emit errorOccurred("Failed to start Redis container");
        return false;
    }

    m_isRunning = true;
    emit redisStatusChanged(true);
    qDebug() << "[RedisDockerManager] ✓ Redis container started successfully";
    qDebug() << "[RedisDockerManager] Redis URI:" << m_redisUri;

    // Step 4: Wait for Redis to be ready
    m_readinessCheckAttempts = 0;
    m_readinessCheckTimer->start();

    return true;
}

bool RedisDockerManager::stopRedis()
{
    if (m_containerName.isEmpty()) {
        qDebug() << "[RedisDockerManager] No container to stop";
        return true;
    }

    qDebug() << "[RedisDockerManager] Stopping Redis container:" << m_containerName;

    m_readinessCheckTimer->stop();

    // Stop and remove container
    runDockerCommand(QStringList() << "rm" << "-f" << m_containerName);

    m_isRunning = false;
    emit redisStatusChanged(false);
    qDebug() << "[RedisDockerManager] ✓ Redis cleanup completed";

    return true;
}

bool RedisDockerManager::isRedisRunning() const
{
    if (m_containerName.isEmpty()) {
        return false;
    }
    return checkContainerRunning(m_containerName);
}

QString RedisDockerManager::getRedisUri() const
{
    return m_redisUri;
}

bool RedisDockerManager::waitForRedisReady(int timeoutMs)
{
    qDebug() << "[RedisDockerManager] Waiting for Redis to be ready (timeout:" << timeoutMs << "ms)";
    
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    
    bool ready = false;
    
    connect(this, &RedisDockerManager::redisReady, &loop, [&loop, &ready]() {
        ready = true;
        loop.quit();
    });
    
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timeoutTimer.start(timeoutMs);
    loop.exec();
    
    return ready;
}

void RedisDockerManager::checkRedisReadiness()
{
    m_readinessCheckAttempts++;
    
    if (m_readinessCheckAttempts > MAX_READINESS_ATTEMPTS) {
        m_readinessCheckTimer->stop();
        qWarning() << "[RedisDockerManager] Redis failed to become ready after" 
                   << MAX_READINESS_ATTEMPTS << "attempts";
        emit errorOccurred("Redis failed to start within expected time");
        return;
    }

    // Try to ping Redis using docker exec
    int pingResult = runDockerCommand(
        QStringList() << "exec" << m_containerName << "redis-cli" << "ping");
    
    if (pingResult == 0) {
        m_readinessCheckTimer->stop();
        qDebug() << "[RedisDockerManager] ✓ Redis is ready and accepting connections";
        emit redisReady();
        return;
    }
    
    qDebug() << "[RedisDockerManager] Waiting for Redis... (attempt" 
             << m_readinessCheckAttempts << "/" << MAX_READINESS_ATTEMPTS << ")";
}

int RedisDockerManager::runDockerCommand(const QStringList& args) const
{
    return QProcess::execute(m_dockerCommand, args);
}

bool RedisDockerManager::executeDockerCommand(const QStringList& args, QString* output, bool waitForFinished)
{
    // For simple commands without output, use system()
    if (!output && waitForFinished) {
        QString cmd = "docker " + args.join(" ");
#ifdef _WIN32
        cmd += " > nul 2>&1";
#else
        cmd += " > /dev/null 2>&1";
#endif
        return std::system(cmd.toStdString().c_str()) == 0;
    }
    
    // For commands that need output, use QProcess
    QProcess process;
    process.start("docker", args);
    
    if (!process.waitForStarted(3000)) {
        qWarning() << "[RedisDockerManager] Failed to start docker command:" << args.join(" ");
        return false;
    }
    
    int timeout = 10000; // 10 seconds
    if (args.contains("pull")) {
        timeout = 300000; // 5 minutes for image pulling
    }
    
    if (!process.waitForFinished(timeout)) {
        qWarning() << "[RedisDockerManager] Docker command timed out:" << args.join(" ");
        process.kill();
        return false;
    }
    
    if (output) {
        *output = QString::fromUtf8(process.readAllStandardOutput());
    }
    
    return process.exitCode() == 0;
}

bool RedisDockerManager::checkContainerExists(const QString& containerName) const
{
    // Use popen() to check if container exists
    std::string cmd = "\"" + m_dockerCommand.toStdString() + "\" ps -a --filter name=^" + containerName.toStdString() + "$ --format {{.Names}}";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return false;
    }
    
    char buffer[128];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    
    // Trim whitespace
    result.erase(result.find_last_not_of(" \n\r\t") + 1);
    
    return result == containerName.toStdString();
}

bool RedisDockerManager::checkContainerRunning(const QString& containerName) const
{
    // Use popen() to check if container is running (not just exists)
    std::string cmd = "\"" + m_dockerCommand.toStdString() + "\" ps --filter name=^" + containerName.toStdString() + "$ --format {{.Names}}";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return false;
    }
    
    char buffer[128];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    
    // Trim whitespace
    result.erase(result.find_last_not_of(" \n\r\t") + 1);
    
    return result == containerName.toStdString();
}

bool RedisDockerManager::stopContainer(const QString& containerName)
{
    qDebug() << "[RedisDockerManager] Stopping container:" << containerName;
    return runDockerCommand(QStringList() << "stop" << containerName) == 0;
}

bool RedisDockerManager::removeContainer(const QString& containerName)
{
    qDebug() << "[RedisDockerManager] Removing container:" << containerName;
    return runDockerCommand(QStringList() << "rm" << containerName) == 0;
}

bool RedisDockerManager::pullDockerImage(const QString& imageName)
{
    qDebug() << "[RedisDockerManager] Pulling image:" << imageName << "(this may take a while...)";
    return runDockerCommand(QStringList() << "pull" << imageName) == 0;
}

bool RedisDockerManager::startContainer()
{
    // This is now handled in startRedis() directly
    return true;
}

void RedisDockerManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "[RedisDockerManager] Process finished with exit code:" << exitCode 
             << "status:" << exitStatus;
}

void RedisDockerManager::onProcessError(QProcess::ProcessError error)
{
    QString errorMsg = getDockerErrorString(error);
    qWarning() << "[RedisDockerManager] Process error:" << errorMsg;
    emit errorOccurred(errorMsg);
}

QString RedisDockerManager::getDockerErrorString(QProcess::ProcessError error) const
{
    switch (error) {
        case QProcess::FailedToStart:
            return "Failed to start Docker process. Is Docker installed?";
        case QProcess::Crashed:
            return "Docker process crashed";
        case QProcess::Timedout:
            return "Docker command timed out";
        case QProcess::WriteError:
            return "Failed to write to Docker process";
        case QProcess::ReadError:
            return "Failed to read from Docker process";
        case QProcess::UnknownError:
        default:
            return "Unknown Docker process error";
    }
}

} // namespace exoskeleton::redis

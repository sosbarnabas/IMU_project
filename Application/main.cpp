#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>
#include <QProgressDialog>
#include "MainWindow.h"
#include "StartupConfigDialog.h"
#include "RedisDockerManager.h"
#include <QDebug>
#include <memory>

int main(int argc, char *argv[]) {
    // High-DPI support
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("YourOrg");
    QCoreApplication::setApplicationName("YourApp");

    // Step 1: Start Redis Docker container
    qDebug() << "[main] Starting Redis Docker container...";
    
    auto redisManager = std::make_unique<exoskeleton::redis::RedisDockerManager>();
    
    // Show progress dialog while starting Redis
    QProgressDialog progressDialog("Starting Redis server in Docker...", "Cancel", 0, 0, nullptr);
    progressDialog.setWindowTitle("ExoGUI - Starting Services");
    progressDialog.setModal(true);
    progressDialog.setMinimumDuration(0);
    progressDialog.setValue(0);
    progressDialog.show();
    QApplication::processEvents();
    
    bool redisStarted = false;
    QObject::connect(redisManager.get(), &exoskeleton::redis::RedisDockerManager::redisReady,
                     [&redisStarted, &progressDialog]() {
                         redisStarted = true;
                         progressDialog.close();
                     });
    
    // Start Redis with default settings
    if (!redisManager->startRedis("exogui-redis", 6379, "redis:latest")) {
        QMessageBox::critical(nullptr, "Startup Error", 
            "Failed to start Redis server in Docker.\n\n"
            "Please ensure:\n"
            "1. Docker is installed and running\n"
            "2. You have network access to pull Docker images\n"
            "3. Port 6379 is available\n\n"
            "The application will now exit.");
        return 1;
    }
    
    // Wait for Redis to be ready (with timeout)
    if (!redisManager->waitForRedisReady(15000)) {
        progressDialog.close();
        QMessageBox::critical(nullptr, "Startup Error", 
            "Redis server started but failed to become ready within 15 seconds.\n\n"
            "The application will now exit.");
        return 1;
    }
    
    progressDialog.close();
    qDebug() << "[main] ✓ Redis is ready";

    // Step 2: Show startup configuration dialog
    StartupConfigDialog configDialog;
    
    // Pre-fill Redis URI from the started container
    configDialog.setRedisUri(redisManager->getRedisUri());
    
    if (configDialog.exec() != QDialog::Accepted) {
        qDebug() << "[main] Startup configuration cancelled, cleaning up and exiting";
        redisManager->stopRedis();
        return 0;
    }
    
    // Get configuration
    QString redisUri = configDialog.redisUri();
    bool uploadEnv = configDialog.shouldUploadEnv();
    QString envPath = configDialog.envFilePath();
    QHash<QString, bool> motorStates = configDialog.motorStates();
    
    qDebug() << "[main] Starting application with configuration:";
    qDebug() << "  Redis URI:" << redisUri;
    qDebug() << "  Upload .env:" << uploadEnv;
    
    // Step 3: Create and configure main window
    MainWindow w;
    w.applyStartupConfig(redisUri, uploadEnv, envPath, motorStates);
    
    w.resize(1600, 900);   // or w.showMaximized();
    w.show();

    // Step 4: Run application
    int exitCode = app.exec();
    
    // Step 5: Cleanup - stop and remove Redis container
    qDebug() << "[main] Application closing, cleaning up Redis container...";
    redisManager->stopRedis();
    qDebug() << "[main] ✓ Cleanup completed";

    return exitCode;
}

#pragma once

#include <QDialog>
#include <QString>
#include <QHash>
#include <memory>
#include <sw/redis++/redis++.h>

class QLineEdit;
class QPushButton;
class QCheckBox;
class QLabel;
class QVBoxLayout;

namespace sw { namespace redis { class Redis; } }

/**
 * @brief Startup configuration dialog shown before main application window
 * 
 * Allows user to configure:
 * - Redis connection URI
 * - .env file upload to Redis
 * - Which motor addresses should be enabled
 */
class StartupConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit StartupConfigDialog(QWidget* parent = nullptr);
    ~StartupConfigDialog() override = default;

    // Configuration results
    QString redisUri() const { return redisUri_; }
    bool shouldUploadEnv() const { return shouldUploadEnv_; }
    QString envFilePath() const { return envFilePath_; }
    QHash<QString, bool> motorStates() const { return motorStates_; }
    
    // Set Redis URI (called from main.cpp after starting Docker container)
    void setRedisUri(const QString& uri);

private slots:
    void onTestConnection();
    void onBrowseEnvFile();
    void onLoadCurrentConfig();
    void onAccept();

private:
    void setupUI();
    void applyAppStyle();
    bool testRedisConnection(const QString& uri);
    void loadMotorStatesFromRedis();

    // UI Components
    QLineEdit* redisUriEdit_ = nullptr;
    QPushButton* testConnectionBtn_ = nullptr;
    QLabel* connectionStatusLabel_ = nullptr;
    
    QCheckBox* uploadEnvCheckbox_ = nullptr;
    QLineEdit* envFileEdit_ = nullptr;
    QPushButton* browseEnvBtn_ = nullptr;
    
    QHash<QString, QCheckBox*> motorCheckboxes_;
    QPushButton* loadCurrentBtn_ = nullptr;
    
    QPushButton* okBtn_ = nullptr;
    QPushButton* cancelBtn_ = nullptr;

    // Configuration data
    QString redisUri_;
    bool shouldUploadEnv_ = false;
    QString envFilePath_;
    QHash<QString, bool> motorStates_;
    
    // Redis connection for testing
    std::unique_ptr<sw::redis::Redis> testRedis_;
};

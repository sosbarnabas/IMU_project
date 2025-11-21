#pragma once
#include <QMainWindow>
#include <QVector>
#include <QHash>
#include <memory>
#include "HomePage.h"
#include "SettingsPage.h"
#include "SpringPage.h"
#include "ArmSettingsPage.h"
#include "TelemetryController.h"

// Forward declarations for Redis types
namespace exoskeleton::settings {
    class Settings;
    class UserParams;
}

namespace exoskeleton::redis {
    class RedisDockerManager;
}

class QStackedWidget;
class TopBar;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent=nullptr);
    
    // Apply startup configuration
    void applyStartupConfig(const QString& redisUri, 
                           bool uploadEnv, 
                           const QString& envPath,
                           const QHash<QString, bool>& motorStates);
    
private:
    HomePage* home_ = nullptr;
    SpringPage* spring_ = nullptr;
    ArmSettingsPage* armSettings_ = nullptr;
    SettingsPage* settings_ = nullptr;
    TopBar* topBar_;
    QStackedWidget* pages_;
    int currentTabIndex_ = 0;
    void setupPages();
    void handleTabChange(int index);
    int settingsPageIndex() const;

private slots:
    void onSpringProfileApplied(const QVector<double>& profile);
    void handleAddFunctionRequest(const QString& motorName);
    void handleCreateFunctionRequest(const QString& motorName);
    void handleSlotSelectionRequest(const QString& motorName);
    void handleSettingsLoaded(const exoskeleton::settings::Settings& settings);
    void handleUserParamsLoaded(const exoskeleton::settings::UserParams& params);

private:
    TelemetryController* telemetry_ {nullptr};
    std::unique_ptr<exoskeleton::redis::RedisDockerManager> redisDockerManager_;
};

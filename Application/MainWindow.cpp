#include "MainWindow.h"
#include "TopBar.h"
#include "SlotWidget.h"
#include "ArmSettingsPage.h"
#include "RedisDockerManager.h"
#include <QStackedWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDebug>
#include "HomePage.h"
#include "RedisTelemetryClient.h"
#include "Settings.h"
#include "UserParams.h"
#include "RedisFacade.h"
#include "EnvLoader.h"
#include <sw/redis++/redis++.h>
#include <vector>

static constexpr auto BG = "#141414";

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // Initialize Redis Docker Manager first
    redisDockerManager_ = std::make_unique<exoskeleton::redis::RedisDockerManager>(this);
    
    // Connect Redis manager signals
    connect(redisDockerManager_.get(), &exoskeleton::redis::RedisDockerManager::redisReady,
            this, []() {
                qDebug() << "[MainWindow] Redis is ready for connections";
            });
    
    connect(redisDockerManager_.get(), &exoskeleton::redis::RedisDockerManager::errorOccurred,
            this, [this](const QString& error) {
                qWarning() << "[MainWindow] Redis Docker error:" << error;
                QMessageBox::warning(this, tr("Redis Error"), 
                    tr("Redis Docker error: %1\n\nThe application may not function correctly.").arg(error));
            });
    
    auto *central = new QWidget(this);
    auto *v = new QVBoxLayout(central);
    v->setContentsMargins(0,0,0,0);
    v->setSpacing(0);

    topBar_ = new TopBar(this);
    pages_  = new QStackedWidget(this);

    setupPages();  // Create Home page with 7 SlotWidgets

    v->addWidget(topBar_);
    v->addWidget(pages_, 1);
    setCentralWidget(central);

    setStyleSheet(QString("QMainWindow{background:%1;}").arg(BG));

    connect(topBar_, &TopBar::tabChanged, this, &MainWindow::handleTabChange);
    
    pages_->setCurrentIndex(3);
    currentTabIndex_ = pages_->currentIndex();
}

void MainWindow::setupPages() {
    home_     = new HomePage(this);
    spring_   = new SpringPage(this);
    armSettings_ = new ArmSettingsPage(this);
    settings_ = new SettingsPage(this);

    pages_->addWidget(home_);        // index 0 = Home
    pages_->addWidget(spring_);      // index 1 = Spring
    pages_->addWidget(armSettings_); // index 2 = Arm Settings
    pages_->addWidget(settings_);    // index 3 = Settings

    // Initialize telemetry controller first
    telemetry_ = new TelemetryController(home_, settings_, this);
    
    // Pass TelemetryController to ArmSettingsPage (not just RedisFacade pointer)
    if (armSettings_ && telemetry_) {
        armSettings_->setTelemetryController(telemetry_);
    }

    // Sync: Settings → Home
    connect(settings_, &SettingsPage::settingsSaved,
            home_,     &HomePage::setMotors);

    if (spring_) {
        connect(spring_, &SpringPage::springProfileApplied,
             this,    &MainWindow::onSpringProfileApplied);
    }

    // Initial state
    home_->setMotors(settings_->enabledNames());

    // telemetry_ already created above
    // TelemetryController initialization moved to top of setupPages()

    // Connect SpringPage to telemetry data
    if (spring_ && telemetry_) {
        connect(telemetry_, &TelemetryController::addressesUpdated,
                spring_, &SpringPage::setMotorAddresses);
        connect(telemetry_, &TelemetryController::redisUriChanged,
                spring_, &SpringPage::setRedisUri);
        
        // Set initial Redis URI (signal was emitted before connection was made)
        spring_->setRedisUri(telemetry_->redisUri());
        
        // Pass the SlotFunctionManager to SpringPage for caching uploaded functions
        if (telemetry_->functionManager()) {
            spring_->setFunctionManager(telemetry_->functionManager());
        }
    }

    // Connect HomePage SlotWidget button signals
    if (home_) {
        connect(home_, &HomePage::slotFunctionAddRequested,
                this, &MainWindow::handleAddFunctionRequest);
        connect(home_, &HomePage::slotFunctionCreateRequested,
                this, &MainWindow::handleCreateFunctionRequest);
        connect(home_, &HomePage::slotSelectionRequested,
                this, &MainWindow::handleSlotSelectionRequest);
    }
    
    // Connect to Redis settings signals for monitoring
    if (telemetry_) {
        connect(telemetry_, &TelemetryController::settingsLoaded,
                this, &MainWindow::handleSettingsLoaded);
        connect(telemetry_, &TelemetryController::userParamsLoaded,
                this, &MainWindow::handleUserParamsLoaded);
    }
}

void MainWindow::onSpringProfileApplied(const QVector<double>& profile) {
    if (!home_)
        return;

    const int slotCount = home_->slotCount();
    if (slotCount <= 0)
        return;

    for (int i = 0; i < slotCount; ++i) {
        if (auto* slot = home_->slotAt(i)) {
            slot->setGraph(profile);
        }
    }
}

int MainWindow::settingsPageIndex() const {
    return pages_->indexOf(settings_);
}

void MainWindow::handleTabChange(int index) {
    if (!pages_)
        return;

    if (index == currentTabIndex_)
        return;

    const int previousIndex = currentTabIndex_;
    const int settingsIndex = settingsPageIndex();

    if (previousIndex == settingsIndex && settings_ && settings_->isDirty()) {
        const auto response = QMessageBox::question(
            this,
            tr("Unsaved Changes"),
            tr("You have unsaved changes. Do you want to save them before leaving the Settings page?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);

        if (response == QMessageBox::Cancel) {
            if (topBar_)
                topBar_->setCurrentIndex(previousIndex);
            return;
        }
    }

    pages_->setCurrentIndex(index);
    currentTabIndex_ = index;
}

void MainWindow::handleAddFunctionRequest(const QString& motorName)
{
    qDebug() << "[MainWindow] Add function requested for motor:" << motorName;
    // Navigate to Spring page (index 1)
    if (pages_ && topBar_) {
        pages_->setCurrentIndex(1);
        topBar_->setCurrentIndex(1);
        currentTabIndex_ = 1;
    }
    // The user can then use the Upload button on the Spring page
}

void MainWindow::handleCreateFunctionRequest(const QString& motorName)
{
    qDebug() << "[MainWindow] Create function requested for motor:" << motorName;
    // Navigate to Spring page (index 1)
    if (pages_ && topBar_) {
        pages_->setCurrentIndex(1);
        topBar_->setCurrentIndex(1);
        currentTabIndex_ = 1;
    }
    // The Spring page editor is now available for creating a new function
}

void MainWindow::handleSlotSelectionRequest(const QString& motorName)
{
    qDebug() << "[MainWindow] Slot selection requested for motor:" << motorName;
    // TODO: Implement slot selection dialog
    // For now, just log the request
    QMessageBox::information(this, tr("Select Slot"),
                           tr("Slot selection for %1 is not yet implemented.\n"
                              "This feature will allow you to select and load a stored function.").arg(motorName));
}

void MainWindow::handleSettingsLoaded(const exoskeleton::settings::Settings& settings)
{
    qDebug() << "[MainWindow] Redis settings loaded:";
    qDebug() << "  - Multiport motors:" << settings.multiport_motors;
    qDebug() << "  - Mock motors:" << settings.mock_motors;
    qDebug() << "  - REST API port:" << settings.restapi_port;
    
    // Motor serials
    qDebug() << "  - Motor E_FLEX:" << QString::fromStdString(settings.motor_e_flex);
    qDebug() << "  - Motor E_EXT:" << QString::fromStdString(settings.motor_e_ext);
    qDebug() << "  - Motor S_FLEX:" << QString::fromStdString(settings.motor_s_flex);
    qDebug() << "  - Motor S_EXT:" << QString::fromStdString(settings.motor_s_ext);
    
    // These settings can be used to configure the application
    // For example, show/hide certain UI elements based on multiport_motors flag
}

void MainWindow::handleUserParamsLoaded(const exoskeleton::settings::UserParams& params)
{
    qDebug() << "[MainWindow] User parameters loaded:";
    qDebug() << "  - User ID:" << params.user_id;
    qDebug() << "  - Body weight:" << params.bodyweight << "kg";
    qDebug() << "  - Motor force:" << params.motorforce;
    qDebug() << "  - Assist level:" << params.assist << "%";
    qDebug() << "  - Upper arm:" << params.upper_arm << "mm";
    qDebug() << "  - Forearm:" << params.forearm << "mm";
    qDebug() << "  - Selected task:" << QString::fromStdString(params.selected_task);
    
    // These params can be displayed in the UI or used for calculations
    // For example, update a status bar or info panel with current user settings
}

void MainWindow::applyStartupConfig(const QString& redisUri, 
                                   bool uploadEnv, 
                                   const QString& envPath,
                                   const QHash<QString, bool>& motorStates)
{
    qDebug() << "[MainWindow] Applying startup configuration:";
    qDebug() << "  Redis URI:" << redisUri;
    qDebug() << "  Upload .env:" << uploadEnv;
    
    // 1. Upload .env file FIRST if requested (before initializing TelemetryController)
    if (uploadEnv && !envPath.isEmpty()) {
        qDebug() << "[MainWindow] Uploading .env file from:" << envPath;
        
        try {
            // Create Redis connection
            sw::redis::Redis redis(redisUri.toStdString());
            
            // Upload .env file using EnvLoader
            int keysUploaded = exoskeleton::redis::EnvLoader::upload_to_redis(
                redis, 
                envPath.toStdString(), 
                false  // Don't overwrite, only update missing keys
            );
            
            qDebug() << "[MainWindow] Successfully uploaded" << keysUploaded << "keys from .env to conf:env";
            
            // After uploading .env, create run:addrs based on selected motors (not all motors in conf:env!)
            qDebug() << "[MainWindow] Creating run:addrs from selected motors (not auto-initializing from conf:env)";
            
            // Build motor address mappings from selected motors
            QHash<QString, QString> motorNameMap;
            motorNameMap["MOTOR_E_FLEX"] = "e_flex";
            motorNameMap["MOTOR_E_EXT"] = "e_ext";
            motorNameMap["MOTOR_S_FLEX"] = "s_flex";
            motorNameMap["MOTOR_S_EXT"] = "s_ext";
            motorNameMap["MOTOR_S_ADD_PRON"] = "s_add_pron";
            motorNameMap["MOTOR_S_ABD"] = "s_abd";
            motorNameMap["MOTOR_S_ADD_SUP"] = "s_add_sup";
            
            int address = 0;
            std::vector<std::pair<std::string, std::string>> motorMappings;
            
            const QStringList motorOrder = {
                "MOTOR_E_FLEX", "MOTOR_E_EXT", "MOTOR_S_FLEX", "MOTOR_S_EXT",
                "MOTOR_S_ADD_PRON", "MOTOR_S_ABD", "MOTOR_S_ADD_SUP"
            };
            
            for (const QString& motorName : motorOrder) {
                bool isEnabled = motorStates.value(motorName, false);
                if (isEnabled) {
                    QString redisKey = motorNameMap.value(motorName, "");
                    if (!redisKey.isEmpty()) {
                        std::string value = std::to_string(address) + "|";
                        motorMappings.emplace_back(redisKey.toStdString(), value);
                        qDebug() << "[MainWindow]   Adding motor to run:addrs:" << redisKey << "=" << QString::fromStdString(value);
                        address++;
                    }
                }
            }
            
            if (!motorMappings.empty()) {
                redis.del("run:addrs");
                redis.hset("run:addrs", motorMappings.begin(), motorMappings.end());
                qDebug() << "[MainWindow] ✓ Created run:addrs with" << motorMappings.size() << "selected motors";
            }
            
        } catch (const std::exception& e) {
            qWarning() << "[MainWindow] Failed to upload .env file:" << e.what();
            QMessageBox::warning(this, tr("Configuration Error"),
                               tr("Failed to upload .env file: %1\n\nThe application will continue with existing configuration.")
                                   .arg(QString::fromStdString(e.what())));
        }
    } else {
        // If not uploading .env, manually create run:addrs from selected motors
        qDebug() << "[MainWindow] No .env upload requested, creating run:addrs manually from selected motors";
        qDebug() << "[MainWindow] Motor states received:" << motorStates.size() << "motors";
        
        // Debug: print all motor states
        for (auto it = motorStates.begin(); it != motorStates.end(); ++it) {
            qDebug() << "[MainWindow]   Motor:" << it.key() << "=" << (it.value() ? "ENABLED" : "disabled");
        }
        
        try {
            qDebug() << "[MainWindow] Connecting to Redis at:" << redisUri;
            sw::redis::Redis redis(redisUri.toStdString());
            qDebug() << "[MainWindow] Redis connection successful";
            
            // Build motor address mappings from selected motors
            // Motor name mapping: MOTOR_E_FLEX -> e_flex, etc.
            QHash<QString, QString> motorNameMap;
            motorNameMap["MOTOR_E_FLEX"] = "e_flex";
            motorNameMap["MOTOR_E_EXT"] = "e_ext";
            motorNameMap["MOTOR_S_FLEX"] = "s_flex";
            motorNameMap["MOTOR_S_EXT"] = "s_ext";
            motorNameMap["MOTOR_S_ADD_PRON"] = "s_add_pron";
            motorNameMap["MOTOR_S_ABD"] = "s_abd";
            motorNameMap["MOTOR_S_ADD_SUP"] = "s_add_sup";
            
            int address = 0;
            std::vector<std::pair<std::string, std::string>> motorMappings;
            
            // Process motors in order
            const QStringList motorOrder = {
                "MOTOR_E_FLEX", "MOTOR_E_EXT", "MOTOR_S_FLEX", "MOTOR_S_EXT",
                "MOTOR_S_ADD_PRON", "MOTOR_S_ABD", "MOTOR_S_ADD_SUP"
            };
            
            qDebug() << "[MainWindow] Processing motors in order...";
            for (const QString& motorName : motorOrder) {
                bool isEnabled = motorStates.value(motorName, false);
                qDebug() << "[MainWindow]   Checking" << motorName << ":" << (isEnabled ? "ENABLED" : "disabled");
                
                if (isEnabled) {
                    QString redisKey = motorNameMap.value(motorName, "");
                    if (!redisKey.isEmpty()) {
                        // Format: "address|serial_number" (serial number empty for now)
                        std::string value = std::to_string(address) + "|";
                        motorMappings.emplace_back(redisKey.toStdString(), value);
                        qDebug() << "[MainWindow]     Adding motor to run:addrs:" << redisKey << "=" << QString::fromStdString(value);
                        address++;
                    }
                }
            }
            
            qDebug() << "[MainWindow] Total motors to add:" << motorMappings.size();
            
            if (!motorMappings.empty()) {
                // Clear existing run:addrs and set new mappings
                qDebug() << "[MainWindow] Deleting existing run:addrs...";
                redis.del("run:addrs");
                qDebug() << "[MainWindow] Writing" << motorMappings.size() << "motors to run:addrs...";
                redis.hset("run:addrs", motorMappings.begin(), motorMappings.end());
                qDebug() << "[MainWindow] ✓ Successfully created run:addrs with" << motorMappings.size() << "motors";
            } else {
                qDebug() << "[MainWindow] ✗ No motors selected, run:addrs not created";
            }
            
        } catch (const std::exception& e) {
            qWarning() << "[MainWindow] ✗ Failed to create run:addrs:" << e.what();
        }
    }
    
    // 2. Set Redis URI in TelemetryController
    // Note: Always pass false for initializeFromEnv because we manually create run:addrs above
    if (telemetry_) {
        telemetry_->setRedisUri(redisUri, false);  // Don't auto-initialize run:addrs, we created it manually
        qDebug() << "[MainWindow] Redis URI set in TelemetryController";
    }
    
    // 3. Apply motor states (enable/disable motors) to UI
    if (settings_) {
        QStringList enabledMotors;
        for (auto it = motorStates.begin(); it != motorStates.end(); ++it) {
            if (it.value()) {
                enabledMotors.append(it.key());
            }
        }
        
        qDebug() << "[MainWindow] Setting enabled motors in UI:" << enabledMotors;
        
        // Update settings page
        settings_->setEnabledFromTelemetry(enabledMotors);
        
        // Update home page
        if (home_) {
            home_->setMotors(enabledMotors);
        }
    }
    
    qDebug() << "[MainWindow] Startup configuration applied successfully";
}

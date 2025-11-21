#include "StartupConfigDialog.h"
#include "EnvLoader.h"
#include "RedisKeys.h"
#include "../Widgets/UIUtilities/StyleHelper.h"
#include "../Widgets/UIUtilities/Constants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QScreen>
#include <QDebug>

#include <sw/redis++/redis++.h>
#include <unordered_map>
#include <iterator>

namespace {
    // All motor names in standard order
    const QStringList kAllMotors = {
        "MOTOR_E_FLEX", "MOTOR_E_EXT", "MOTOR_S_FLEX", "MOTOR_S_EXT",
        "MOTOR_S_ADD_PRON", "MOTOR_S_ABD", "MOTOR_S_ADD_SUP"
    };
}

StartupConfigDialog::StartupConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("ExoGUI - Startup Configuration"));
    setModal(true);
    
    // Default Redis URI
    redisUri_ = "tcp://127.0.0.1:6379";
    
    // Initialize motor states (all disabled by default)
    for (const QString& motor : kAllMotors) {
        motorStates_[motor] = false;
    }
    
    setupUI();
    applyAppStyle();
    
    // Set reasonable size
    resize(600, 700);
    
    // Center on screen
    if (QScreen* screen = QApplication::primaryScreen()) {
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }
}

void StartupConfigDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    
    // Title
    auto* titleLabel = new QLabel(tr("Application Startup Configuration"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 6);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    
    // ========== Redis Connection Section ==========
    auto* redisGroup = new QGroupBox(tr("Redis Connection"), this);
    auto* redisLayout = new QVBoxLayout(redisGroup);
    redisLayout->setSpacing(12);
    
    auto* uriLabel = new QLabel(tr("Redis URI:"), this);
    redisLayout->addWidget(uriLabel);
    
    auto* uriRow = new QHBoxLayout();
    redisUriEdit_ = new QLineEdit(redisUri_, this);
    redisUriEdit_->setPlaceholderText("tcp://127.0.0.1:6379");
    redisUriEdit_->setMinimumHeight(32);
    uriRow->addWidget(redisUriEdit_, 1);
    
    testConnectionBtn_ = new QPushButton(tr("Test Connection"), this);
    testConnectionBtn_->setMinimumHeight(32);
    uriRow->addWidget(testConnectionBtn_);
    redisLayout->addLayout(uriRow);
    
    connectionStatusLabel_ = new QLabel(tr("Not tested"), this);
    connectionStatusLabel_->setAlignment(Qt::AlignCenter);
    connectionStatusLabel_->setStyleSheet("padding: 4px; border-radius: 4px;");
    redisLayout->addWidget(connectionStatusLabel_);
    
    mainLayout->addWidget(redisGroup);
    
    // ========== Environment File Upload Section ==========
    auto* envGroup = new QGroupBox(tr("Environment Configuration"), this);
    auto* envLayout = new QVBoxLayout(envGroup);
    envLayout->setSpacing(12);
    
    uploadEnvCheckbox_ = new QCheckBox(tr("Upload .env file to Redis (conf:env)"), this);
    envLayout->addWidget(uploadEnvCheckbox_);
    
    auto* envFileRow = new QHBoxLayout();
    envFileEdit_ = new QLineEdit(".env", this);
    envFileEdit_->setPlaceholderText("Path to .env file");
    envFileEdit_->setMinimumHeight(32);
    envFileEdit_->setEnabled(false);
    envFileRow->addWidget(envFileEdit_, 1);
    
    browseEnvBtn_ = new QPushButton(tr("Browse..."), this);
    browseEnvBtn_->setMinimumHeight(32);
    browseEnvBtn_->setEnabled(false);
    envFileRow->addWidget(browseEnvBtn_);
    envLayout->addLayout(envFileRow);
    
    auto* envNote = new QLabel(tr("Note: Enable this if Redis conf:env is empty or needs updating"), this);
    envNote->setWordWrap(true);
    envNote->setStyleSheet("color: rgba(255, 255, 255, 150); font-size: 12px;");
    envLayout->addWidget(envNote);
    
    mainLayout->addWidget(envGroup);
    
    // ========== Motor Selection Section ==========
    auto* motorGroup = new QGroupBox(tr("Motor Configuration"), this);
    auto* motorLayout = new QVBoxLayout(motorGroup);
    motorLayout->setSpacing(12);
    
    auto* motorLabel = new QLabel(tr("Select motors to enable:"), this);
    motorLayout->addWidget(motorLabel);
    
    auto* motorGrid = new QGridLayout();
    motorGrid->setSpacing(8);
    
    for (int i = 0; i < kAllMotors.size(); ++i) {
        const QString& motorName = kAllMotors[i];
        auto* checkbox = new QCheckBox(motorName, this);
        checkbox->setChecked(false);
        motorCheckboxes_[motorName] = checkbox;
        motorGrid->addWidget(checkbox, i / 2, i % 2);
    }
    motorLayout->addLayout(motorGrid);
    
    loadCurrentBtn_ = new QPushButton(tr("Load Current Configuration from Redis"), this);
    loadCurrentBtn_->setMinimumHeight(36);
    motorLayout->addWidget(loadCurrentBtn_);
    
    auto* motorNote = new QLabel(tr("Note: Disabled motors will not receive telemetry data"), this);
    motorNote->setWordWrap(true);
    motorNote->setStyleSheet("color: rgba(255, 255, 255, 150); font-size: 12px;");
    motorLayout->addWidget(motorNote);
    
    mainLayout->addWidget(motorGroup);
    
    // ========== Action Buttons ==========
    mainLayout->addStretch(1);
    
    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(12);
    
    cancelBtn_ = new QPushButton(tr("Cancel"), this);
    cancelBtn_->setMinimumHeight(40);
    buttonRow->addWidget(cancelBtn_);
    
    buttonRow->addStretch(1);
    
    okBtn_ = new QPushButton(tr("Start Application"), this);
    okBtn_->setMinimumHeight(40);
    buttonRow->addWidget(okBtn_);
    
    mainLayout->addLayout(buttonRow);
    
    // ========== Connect Signals ==========
    connect(testConnectionBtn_, &QPushButton::clicked,
            this, &StartupConfigDialog::onTestConnection);
    connect(uploadEnvCheckbox_, &QCheckBox::toggled,
            this, [this](bool checked) {
                envFileEdit_->setEnabled(checked);
                browseEnvBtn_->setEnabled(checked);
            });
    connect(browseEnvBtn_, &QPushButton::clicked,
            this, &StartupConfigDialog::onBrowseEnvFile);
    connect(loadCurrentBtn_, &QPushButton::clicked,
            this, &StartupConfigDialog::onLoadCurrentConfig);
    connect(okBtn_, &QPushButton::clicked,
            this, &StartupConfigDialog::onAccept);
    connect(cancelBtn_, &QPushButton::clicked,
            this, &QDialog::reject);
}

void StartupConfigDialog::applyAppStyle()
{
    // Apply dialog style
    setStyleSheet(UI::StyleHelper::dialogStyle());
    
    // Apply specific widget styles
    if (redisUriEdit_) redisUriEdit_->setStyleSheet(UI::StyleHelper::lineEditStyle());
    if (envFileEdit_) envFileEdit_->setStyleSheet(UI::StyleHelper::lineEditStyle());
    
    // Apply checkbox style to all checkboxes
    for (auto* checkbox : motorCheckboxes_) {
        checkbox->setStyleSheet(UI::StyleHelper::checkBoxStyle());
    }
    if (uploadEnvCheckbox_) uploadEnvCheckbox_->setStyleSheet(UI::StyleHelper::checkBoxStyle());
    
    // Apply button styles
    const QString buttonStyle = UI::StyleHelper::pillButtonStyle(6);
    if (testConnectionBtn_) testConnectionBtn_->setStyleSheet(buttonStyle);
    if (browseEnvBtn_) browseEnvBtn_->setStyleSheet(buttonStyle);
    if (loadCurrentBtn_) loadCurrentBtn_->setStyleSheet(buttonStyle);
    if (cancelBtn_) cancelBtn_->setStyleSheet(buttonStyle);
    
    // Primary button (Start Application) gets special accent color
    if (okBtn_) {
        okBtn_->setStyleSheet(
            "QPushButton {"
            "  padding: 10px 24px;"
            "  border-radius: 6px;"
            "  font-size: 15px;"
            "  font-weight: bold;"
            "  color: #ffffff;"
            "  background: #008cff;"
            "  border: 2px solid #008cff;"
            "}"
            "QPushButton:hover {"
            "  background: #0099ff;"
            "  border: 2px solid #0099ff;"
            "}"
            "QPushButton:pressed {"
            "  background: #0070cc;"
            "}"
        );
    }
}

void StartupConfigDialog::onTestConnection()
{
    QString uri = redisUriEdit_->text().trimmed();
    if (uri.isEmpty()) {
        connectionStatusLabel_->setText(tr("⚠ Please enter a Redis URI"));
        connectionStatusLabel_->setStyleSheet(QString("color: %1; padding: 4px;").arg(UI::Colors::DANGER));
        return;
    }
    
    connectionStatusLabel_->setText(tr("Testing..."));
    connectionStatusLabel_->setStyleSheet("color: rgba(255, 255, 255, 150); padding: 4px;");
    QApplication::processEvents();
    
    if (testRedisConnection(uri)) {
        connectionStatusLabel_->setText(tr("✓ Connected successfully"));
        connectionStatusLabel_->setStyleSheet(QString("color: %1; padding: 4px; font-weight: bold;").arg(UI::Colors::SUCCESS));
        redisUri_ = uri;
    } else {
        connectionStatusLabel_->setText(tr("✗ Connection failed"));
        connectionStatusLabel_->setStyleSheet(QString("color: %1; padding: 4px; font-weight: bold;").arg(UI::Colors::DANGER));
    }
}

bool StartupConfigDialog::testRedisConnection(const QString& uri)
{
    try {
        testRedis_ = std::make_unique<sw::redis::Redis>(uri.toStdString());
        testRedis_->ping();
        qDebug() << "[StartupConfigDialog] Redis connection test successful:" << uri;
        return true;
    } catch (const std::exception& e) {
        qWarning() << "[StartupConfigDialog] Redis connection test failed:" << e.what();
        return false;
    }
}

void StartupConfigDialog::onBrowseEnvFile()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Select .env File"),
        ".",
        tr("Environment Files (*.env);;All Files (*)")
    );
    
    if (!fileName.isEmpty()) {
        envFileEdit_->setText(fileName);
    }
}

void StartupConfigDialog::onLoadCurrentConfig()
{
    QString uri = redisUriEdit_->text().trimmed();
    if (uri.isEmpty()) {
        QMessageBox::warning(this, tr("No Redis URI"),
                           tr("Please enter and test a Redis URI first."));
        return;
    }
    
    if (!testRedis_ || !testRedisConnection(uri)) {
        QMessageBox::warning(this, tr("Connection Failed"),
                           tr("Cannot connect to Redis. Please check your URI and try again."));
        return;
    }
    
    loadMotorStatesFromRedis();
}

void StartupConfigDialog::loadMotorStatesFromRedis()
{
    if (!testRedis_) {
        return;
    }
    
    try {
        // Load from run:addrs hash
        std::unordered_map<std::string, std::string> addrs;
        testRedis_->hgetall(exo::redis::keys::RUN_ADDRS, std::inserter(addrs, addrs.begin()));
        
        // Reset all to unchecked first
        for (auto* checkbox : motorCheckboxes_) {
            checkbox->setChecked(false);
        }
        
        // Mark enabled motors
        int enabledCount = 0;
        for (const auto& pair : addrs) {
            QString motorName = QString::fromStdString(pair.first).toUpper();
            
            // Normalize motor name (add MOTOR_ prefix if missing)
            if (!motorName.startsWith("MOTOR_")) {
                motorName = "MOTOR_" + motorName;
            }
            
            if (motorCheckboxes_.contains(motorName)) {
                motorCheckboxes_[motorName]->setChecked(true);
                motorStates_[motorName] = true;
                ++enabledCount;
            }
        }
        
        QMessageBox::information(this, tr("Configuration Loaded"),
                               tr("Loaded current configuration: %1 motors enabled").arg(enabledCount));
        
        qDebug() << "[StartupConfigDialog] Loaded" << enabledCount << "enabled motors from Redis";
        
    } catch (const std::exception& e) {
        qWarning() << "[StartupConfigDialog] Failed to load motor states from Redis:" << e.what();
        QMessageBox::warning(this, tr("Load Failed"),
                           tr("Failed to load configuration from Redis: %1")
                               .arg(QString::fromStdString(e.what())));
    }
}

void StartupConfigDialog::onAccept()
{
    // Validate Redis URI
    redisUri_ = redisUriEdit_->text().trimmed();
    if (redisUri_.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Configuration"),
                           tr("Please enter a Redis URI."));
        return;
    }
    
    // Collect motor states
    for (const QString& motorName : kAllMotors) {
        motorStates_[motorName] = motorCheckboxes_[motorName]->isChecked();
    }
    
    // Get env upload settings
    shouldUploadEnv_ = uploadEnvCheckbox_->isChecked();
    if (shouldUploadEnv_) {
        envFilePath_ = envFileEdit_->text().trimmed();
        if (envFilePath_.isEmpty()) {
            QMessageBox::warning(this, tr("Invalid Configuration"),
                               tr("Please specify a .env file path or disable env upload."));
            return;
        }
    }
    
    qDebug() << "[StartupConfigDialog] Configuration accepted:";
    qDebug() << "  Redis URI:" << redisUri_;
    qDebug() << "  Upload .env:" << shouldUploadEnv_;
    if (shouldUploadEnv_) {
        qDebug() << "  .env path:" << envFilePath_;
    }
    
    int enabledCount = 0;
    for (auto it = motorStates_.begin(); it != motorStates_.end(); ++it) {
        if (it.value()) {
            qDebug() << "  Motor enabled:" << it.key();
            ++enabledCount;
        }
    }
    qDebug() << "  Total enabled motors:" << enabledCount;
    
    accept();
}

void StartupConfigDialog::setRedisUri(const QString& uri)
{
    if (redisUriEdit_) {
        redisUriEdit_->setText(uri);
        qDebug() << "[StartupConfigDialog] Redis URI pre-filled:" << uri;
    }
}

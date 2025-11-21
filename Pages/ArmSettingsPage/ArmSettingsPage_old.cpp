#include "ArmSettingsPage.h"
#include "RedisFacade.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QIntValidator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QDebug>

// ArmSettingsData implementation
bool ArmSettingsData::isValid() const {
    return !name.trimmed().isEmpty() &&
           bodyweight > 0 && bodyweight <= 500 &&
           upper_arm_cuff > 0 && upper_arm_cuff <= 9999 &&
           upper_arm > 0 && upper_arm <= 9999 &&
           forearm_cuff > 0 && forearm_cuff <= 9999 &&
           forearm > 0 && forearm <= 9999 &&
           cuff > 0 && cuff <= 9999;
}

QString ArmSettingsData::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["bodyweight"] = bodyweight;
    obj["upper_arm_cuff"] = upper_arm_cuff;
    obj["upper_arm"] = upper_arm;
    obj["forearm_cuff"] = forearm_cuff;
    obj["forearm"] = forearm;
    obj["cuff"] = cuff;
    
    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact);
}

ArmSettingsData ArmSettingsData::fromJson(const QString& json) {
    ArmSettingsData data;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    
    if (!doc.isObject()) {
        return data;
    }
    
    QJsonObject obj = doc.object();
    data.name = obj["name"].toString();
    data.bodyweight = obj["bodyweight"].toInt();
    data.upper_arm_cuff = obj["upper_arm_cuff"].toInt();
    data.upper_arm = obj["upper_arm"].toInt();
    data.forearm_cuff = obj["forearm_cuff"].toInt();
    data.forearm = obj["forearm"].toInt();
    data.cuff = obj["cuff"].toInt();
    
    return data;
}

// ArmSettingsPage implementation
ArmSettingsPage::ArmSettingsPage(QWidget* parent)
    : QWidget(parent)
{
    initializeUI();
    connectSignals();
}

void ArmSettingsPage::initializeUI() {
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setContentsMargins(40, 40, 40, 40);
    mainLayout_->setSpacing(20);
    
    // Title
    auto* title = new QLabel("Arm Settings Configuration", this);
    QFont titleFont = title->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setStyleSheet("color: #FFFFFF; margin-bottom: 10px;");
    mainLayout_->addWidget(title);
    
    // Form layout
    setupFormLayout();
    
    // Status label
    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet("color: #4CAF50; font-size: 12px; margin-top: 5px;");
    statusLabel_->setVisible(false);
    mainLayout_->addWidget(statusLabel_);
    
    // Save button
    saveButton_ = new QPushButton("Save Settings", this);
    saveButton_->setEnabled(false);
    saveButton_->setMinimumHeight(40);
    saveButton_->setStyleSheet(
        "QPushButton {"
        "   background-color: #2196F3;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 4px;"
        "   font-size: 14px;"
        "   font-weight: bold;"
        "   padding: 10px 20px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #1976D2;"
        "}"
        "QPushButton:disabled {"
        "   background-color: #666666;"
        "   color: #999999;"
        "}"
    );
    mainLayout_->addWidget(saveButton_);
    
    mainLayout_->addStretch();
    
    // Set overall style
    setStyleSheet("QWidget { background-color: #1E1E1E; }");
}

void ArmSettingsPage::setupFormLayout() {
    formLayout_ = new QFormLayout();
    formLayout_->setSpacing(15);
    formLayout_->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    // Style for labels
    const QString labelStyle = "color: #CCCCCC; font-size: 13px; padding-right: 10px;";
    
    // Style for inputs
    const QString inputStyle = 
        "QLineEdit {"
        "   background-color: #2D2D2D;"
        "   color: #FFFFFF;"
        "   border: 1px solid #444444;"
        "   border-radius: 4px;"
        "   padding: 8px;"
        "   font-size: 13px;"
        "   min-width: 300px;"
        "}"
        "QLineEdit:focus {"
        "   border: 1px solid #2196F3;"
        "}";
    
    // Settings Name (text input)
    auto* nameLabel = new QLabel("Settings Name:", this);
    nameLabel->setStyleSheet(labelStyle);
    nameInput_ = new QLineEdit(this);
    nameInput_->setPlaceholderText("Enter a name for this setting...");
    nameInput_->setStyleSheet(inputStyle);
    formLayout_->addRow(nameLabel, nameInput_);
    
    // Body Weight (kg)
    auto* bodyweightLabel = new QLabel("Testtömeg (kg):", this);
    bodyweightLabel->setStyleSheet(labelStyle);
    bodyweightInput_ = createNumericInput(1, 500);
    bodyweightInput_->setStyleSheet(inputStyle);
    bodyweightInput_->setPlaceholderText("kg");
    formLayout_->addRow(bodyweightLabel, bodyweightInput_);
    
    // Upper Arm Cuff Distance (mm)
    auto* upperArmCuffLabel = new QLabel("Felkar mandzsetta távolság (mm):", this);
    upperArmCuffLabel->setStyleSheet(labelStyle);
    upperArmCuffInput_ = createNumericInput(1, 9999);
    upperArmCuffInput_->setStyleSheet(inputStyle);
    upperArmCuffInput_->setPlaceholderText("mm");
    formLayout_->addRow(upperArmCuffLabel, upperArmCuffInput_);
    
    // Upper Arm Length (mm)
    auto* upperArmLabel = new QLabel("Felkar hossz (mm):", this);
    upperArmLabel->setStyleSheet(labelStyle);
    upperArmInput_ = createNumericInput(1, 9999);
    upperArmInput_->setStyleSheet(inputStyle);
    upperArmInput_->setPlaceholderText("mm");
    formLayout_->addRow(upperArmLabel, upperArmInput_);
    
    // Forearm Cuff Distance (mm)
    auto* forearmCuffLabel = new QLabel("Alkar mandzsetta távolság (mm):", this);
    forearmCuffLabel->setStyleSheet(labelStyle);
    forearmCuffInput_ = createNumericInput(1, 9999);
    forearmCuffInput_->setStyleSheet(inputStyle);
    forearmCuffInput_->setPlaceholderText("mm");
    formLayout_->addRow(forearmCuffLabel, forearmCuffInput_);
    
    // Forearm Length (mm)
    auto* forearmLabel = new QLabel("Alkar hossz (mm):", this);
    forearmLabel->setStyleSheet(labelStyle);
    forearmInput_ = createNumericInput(1, 9999);
    forearmInput_->setStyleSheet(inputStyle);
    forearmInput_->setPlaceholderText("mm");
    formLayout_->addRow(forearmLabel, forearmInput_);
    
    // Cuff Distance (mm)
    auto* cuffLabel = new QLabel("Mandzsetta távolság (mm):", this);
    cuffLabel->setStyleSheet(labelStyle);
    cuffInput_ = createNumericInput(1, 9999);
    cuffInput_->setStyleSheet(inputStyle);
    cuffInput_->setPlaceholderText("mm");
    formLayout_->addRow(cuffLabel, cuffInput_);
    
    mainLayout_->addLayout(formLayout_);
}

void ArmSettingsPage::connectSignals() {
    // Enable save button when any field changes
    connect(nameInput_, &QLineEdit::textChanged, this, &ArmSettingsPage::updateSaveButtonState);
    connect(bodyweightInput_, &QLineEdit::textChanged, this, &ArmSettingsPage::updateSaveButtonState);
    connect(upperArmCuffInput_, &QLineEdit::textChanged, this, &ArmSettingsPage::updateSaveButtonState);
    connect(upperArmInput_, &QLineEdit::textChanged, this, &ArmSettingsPage::updateSaveButtonState);
    connect(forearmCuffInput_, &QLineEdit::textChanged, this, &ArmSettingsPage::updateSaveButtonState);
    connect(forearmInput_, &QLineEdit::textChanged, this, &ArmSettingsPage::updateSaveButtonState);
    connect(cuffInput_, &QLineEdit::textChanged, this, &ArmSettingsPage::updateSaveButtonState);
    
    // Save button click
    connect(saveButton_, &QPushButton::clicked, this, &ArmSettingsPage::onSaveClicked);
}

QLineEdit* ArmSettingsPage::createNumericInput(int minValue, int maxValue) {
    auto* input = new QLineEdit();
    auto* validator = new QIntValidator(minValue, maxValue, input);
    input->setValidator(validator);
    return input;
}

ArmSettingsData ArmSettingsPage::currentSettings() const {
    ArmSettingsData data;
    data.name = nameInput_->text().trimmed();
    data.bodyweight = bodyweightInput_->text().toInt();
    data.upper_arm_cuff = upperArmCuffInput_->text().toInt();
    data.upper_arm = upperArmInput_->text().toInt();
    data.forearm_cuff = forearmCuffInput_->text().toInt();
    data.forearm = forearmInput_->text().toInt();
    data.cuff = cuffInput_->text().toInt();
    return data;
}

void ArmSettingsPage::loadSettings(const ArmSettingsData& data) {
    nameInput_->setText(data.name);
    bodyweightInput_->setText(QString::number(data.bodyweight));
    upperArmCuffInput_->setText(QString::number(data.upper_arm_cuff));
    upperArmInput_->setText(QString::number(data.upper_arm));
    forearmCuffInput_->setText(QString::number(data.forearm_cuff));
    forearmInput_->setText(QString::number(data.forearm));
    cuffInput_->setText(QString::number(data.cuff));
}

void ArmSettingsPage::clearFields() {
    nameInput_->clear();
    bodyweightInput_->clear();
    upperArmCuffInput_->clear();
    upperArmInput_->clear();
    forearmCuffInput_->clear();
    forearmInput_->clear();
    cuffInput_->clear();
    statusLabel_->setVisible(false);
}

void ArmSettingsPage::setRedisFacade(exoskeleton::redis::Facade* facade) {
    redisFacade_ = facade;
}

void ArmSettingsPage::onSaveClicked() {
    ArmSettingsData data = currentSettings();
    
    if (!data.isValid()) {
        showStatus("Kérlek töltsd ki az összes mezőt érvényes értékekkel!", true);
        return;
    }
    
    saveToRedis(data);
    showStatus("Beállítások sikeresen elmentve: " + data.name, false);
    
    emit settingsSaved(data.name);
    emit settingsListChanged();
}

void ArmSettingsPage::saveToRedis(const ArmSettingsData& data) {
    if (!redisFacade_) {
        qWarning() << "[ArmSettingsPage] RedisFacade not set, cannot save to Redis";
        return;
    }
    
    try {
        // Save settings to Redis under key: armsettings:<name>
        QString key = "armsettings:" + data.name;
        redisFacade_->set_value(key.toStdString(), data.toJson().toStdString());
        
        // Add to list of available settings
        // We'll use a Redis SET to track all saved setting names
        redisFacade_->_redis.sadd("armsettings:list", data.name.toStdString());
        
        qDebug() << "[ArmSettingsPage] Saved settings:" << data.name << "to Redis";
    } catch (const std::exception& e) {
        qWarning() << "[ArmSettingsPage] Failed to save to Redis:" << e.what();
        showStatus("Hiba történt a mentés során!", true);
    }
}

void ArmSettingsPage::loadSettingsFromRedis(const QString& settingsName) {
    if (!redisFacade_ || settingsName.isEmpty()) {
        return;
    }
    
    try {
        QString key = "armsettings:" + settingsName;
        auto jsonStr = redisFacade_->_redis.get(key.toStdString());
        
        if (jsonStr) {
            ArmSettingsData data = ArmSettingsData::fromJson(QString::fromStdString(*jsonStr));
            loadSettings(data);
            showStatus("Beállítások betöltve: " + settingsName, false);
            qDebug() << "[ArmSettingsPage] Loaded settings:" << settingsName << "from Redis";
        } else {
            showStatus("A beállítás nem található: " + settingsName, true);
        }
    } catch (const std::exception& e) {
        qWarning() << "[ArmSettingsPage] Failed to load from Redis:" << e.what();
        showStatus("Hiba történt a betöltés során!", true);
    }
}

void ArmSettingsPage::updateSaveButtonState() {
    ArmSettingsData data = currentSettings();
    saveButton_->setEnabled(data.isValid());
}

void ArmSettingsPage::showStatus(const QString& message, bool isError) {
    statusLabel_->setText(message);
    statusLabel_->setStyleSheet(
        QString("color: %1; font-size: 12px; margin-top: 5px;")
        .arg(isError ? "#F44336" : "#4CAF50")
    );
    statusLabel_->setVisible(true);
}

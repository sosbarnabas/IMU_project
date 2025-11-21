#include "ArmSettingsPage.h"
#include "RedisFacade.h"
#include "UserParams.h"
#include "TelemetryController.h"
#include "../../Widgets/UIUtilities/StyleHelper.h"
#include "../../Widgets/UIUtilities/Constants.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QIntValidator>
#include <QMessageBox>
#include <QDebug>

using namespace exoskeleton::settings;

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
    auto* title = new QLabel("Arm Measurements Configuration", this);
    QFont titleFont = title->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    title->setFont(titleFont);
    mainLayout_->addWidget(title);
    
    // Description
    auto* desc = new QLabel("Configure user physical measurements for the exoskeleton", this);
    mainLayout_->addWidget(desc);
    
    // Form layout
    setupFormLayout();
    
    // Status label
    statusLabel_ = new QLabel(this);
    statusLabel_->setVisible(false);
    mainLayout_->addWidget(statusLabel_);
    
    // Buttons layout
    auto* buttonLayout = new QHBoxLayout();
    
    loadButton_ = new QPushButton("Load from Redis", this);
    loadButton_->setMinimumHeight(40);
    loadButton_->setStyleSheet(UI::StyleHelper::pillButtonStyle(4));
    buttonLayout->addWidget(loadButton_);
    
    saveButton_ = new QPushButton("Save to Redis", this);
    saveButton_->setEnabled(false);
    saveButton_->setMinimumHeight(40);
    saveButton_->setStyleSheet(UI::StyleHelper::pillButtonStyle(4));
    buttonLayout->addWidget(saveButton_);
    
    mainLayout_->addLayout(buttonLayout);
    mainLayout_->addStretch();
}

void ArmSettingsPage::setupFormLayout() {
    formLayout_ = new QFormLayout();
    formLayout_->setSpacing(UI::Dialog::GROUP_SPACING);
    formLayout_->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    const QString inputStyle = UI::StyleHelper::lineEditStyle();
    
    // User ID
    auto* userIdLabel = new QLabel("User ID:", this);
    userIdInput_ = createNumericInput(0, 999999);
    userIdInput_->setStyleSheet(inputStyle);
    userIdInput_->setPlaceholderText("User identifier");
    formLayout_->addRow(userIdLabel, userIdInput_);
    
    // Body Weight
    auto* bodyweightLabel = new QLabel("Testtömeg (kg):", this);
    bodyweightInput_ = createNumericInput(0, 500);
    bodyweightInput_->setStyleSheet(inputStyle);
    bodyweightInput_->setPlaceholderText("kg");
    formLayout_->addRow(bodyweightLabel, bodyweightInput_);
    
    // Upper Arm Length
    auto* upperArmLabel = new QLabel("Felkar hossz (mm):", this);
    upperArmInput_ = createNumericInput(1, 9999);
    upperArmInput_->setStyleSheet(inputStyle);
    upperArmInput_->setPlaceholderText("mm");
    formLayout_->addRow(upperArmLabel, upperArmInput_);
    
    // Upper Arm Cuff Distance
    auto* upperArmCuffLabel = new QLabel("Felkar mandzsetta távolság könyöktől (mm):", this);
    upperArmCuffInput_ = createNumericInput(0, 9999);
    upperArmCuffInput_->setStyleSheet(inputStyle);
    upperArmCuffInput_->setPlaceholderText("mm");
    formLayout_->addRow(upperArmCuffLabel, upperArmCuffInput_);
    
    // Forearm Length
    auto* forearmLabel = new QLabel("Alkar hossz (mm):", this);
    forearmInput_ = createNumericInput(1, 9999);
    forearmInput_->setStyleSheet(inputStyle);
    forearmInput_->setPlaceholderText("mm");
    formLayout_->addRow(forearmLabel, forearmInput_);
    
    // Forearm Cuff Distance
    auto* forearmCuffLabel = new QLabel("Alkar mandzsetta távolság csuklótól (mm):", this);
    forearmCuffInput_ = createNumericInput(0, 9999);
    forearmCuffInput_->setStyleSheet(inputStyle);
    forearmCuffInput_->setPlaceholderText("mm");
    formLayout_->addRow(forearmCuffLabel, forearmCuffInput_);
    
    // Cuff Distance
    auto* cuffLabel = new QLabel("Mandzseták közötti távolság (mm):", this);
    cuffInput_ = createNumericInput(0, 9999);
    cuffInput_->setStyleSheet(inputStyle);
    cuffInput_->setPlaceholderText("mm");
    formLayout_->addRow(cuffLabel, cuffInput_);
    
    mainLayout_->addLayout(formLayout_);
}

void ArmSettingsPage::connectSignals() {
    auto enableSave = [this]() { updateSaveButtonState(); };
    
    connect(userIdInput_, &QLineEdit::textChanged, this, enableSave);
    connect(bodyweightInput_, &QLineEdit::textChanged, this, enableSave);
    connect(upperArmInput_, &QLineEdit::textChanged, this, enableSave);
    connect(upperArmCuffInput_, &QLineEdit::textChanged, this, enableSave);
    connect(forearmInput_, &QLineEdit::textChanged, this, enableSave);
    connect(forearmCuffInput_, &QLineEdit::textChanged, this, enableSave);
    connect(cuffInput_, &QLineEdit::textChanged, this, enableSave);
    
    connect(saveButton_, &QPushButton::clicked, this, &ArmSettingsPage::onSaveClicked);
    connect(loadButton_, &QPushButton::clicked, this, &ArmSettingsPage::onLoadClicked);
}

QLineEdit* ArmSettingsPage::createNumericInput(int minValue, int maxValue) {
    auto* input = new QLineEdit();
    auto* validator = new QIntValidator(minValue, maxValue, input);
    input->setValidator(validator);
    return input;
}

void ArmSettingsPage::loadUserParams(const UserParams& params) {
    userIdInput_->setText(QString::number(params.user_id));
    bodyweightInput_->setText(QString::number(params.bodyweight));
    upperArmInput_->setText(QString::number(params.upper_arm));
    upperArmCuffInput_->setText(QString::number(params.upper_arm_cuff));
    forearmInput_->setText(QString::number(params.forearm));
    forearmCuffInput_->setText(QString::number(params.forearm_cuff));
    cuffInput_->setText(QString::number(params.cuff));
}

void ArmSettingsPage::clearFields() {
    userIdInput_->clear();
    bodyweightInput_->clear();
    upperArmInput_->clear();
    upperArmCuffInput_->clear();
    forearmInput_->clear();
    forearmCuffInput_->clear();
    cuffInput_->clear();
    statusLabel_->setVisible(false);
}

void ArmSettingsPage::setTelemetryController(TelemetryController* controller) {
    telemetryController_ = controller;
}

void ArmSettingsPage::onSaveClicked() {
    if (!telemetryController_ || !telemetryController_->redisFacade()) {
        showStatus("RedisFacade nincs beállítva!", true);
        return;
    }
    
    // Validate required fields
    if (bodyweightInput_->text().isEmpty() ||
        upperArmInput_->text().isEmpty() ||
        forearmInput_->text().isEmpty()) {
        showStatus("Kérlek töltsd ki a kötelező mezőket!", true);
        return;
    }
    
    saveUserParamsToRedis();
}

void ArmSettingsPage::onLoadClicked() {
    loadCurrentUserParams();
}

void ArmSettingsPage::saveUserParamsToRedis() {
    if (!telemetryController_ || !telemetryController_->redisFacade()) {
        showStatus("RedisFacade nincs beállítva!", true);
        return;
    }
    
    auto* redisFacade = telemetryController_->redisFacade();
    
    // Disable button during save
    saveButton_->setEnabled(false);
    showStatus("Mentés...", false);
    
    // Create UserParams from UI
    UserParams params;
    params.user_id = userIdInput_->text().toInt();
    params.bodyweight = bodyweightInput_->text().toInt();
    params.upper_arm = upperArmInput_->text().toInt();
    params.upper_arm_cuff = upperArmCuffInput_->text().toInt();
    params.forearm = forearmInput_->text().toInt();
    params.forearm_cuff = forearmCuffInput_->text().toInt();
    params.cuff = cuffInput_->text().toInt();
    params.motorforce_min = 0;
    params.motorforce_max = 127;
    params.motorforce = 50;
    params.assist = 50;
    params.selected_task = "";
    
    // Save to Redis
    try {
        redisFacade->set_user_params(params);
        showStatus("Méretek sikeresen elmentve!", false);
        emit userParamsUpdated();
    } catch (const std::exception& e) {
        showStatus(QString("Hiba: %1").arg(e.what()), true);
    }
    
    // Re-enable button
    updateSaveButtonState();
}

void ArmSettingsPage::loadCurrentUserParams() {
    if (!telemetryController_ || !telemetryController_->redisFacade()) {
        showStatus("RedisFacade nincs beállítva!", true);
        return;
    }
    
    try {
        UserParams params = telemetryController_->redisFacade()->load_user_params();
        loadUserParams(params);
        showStatus("Méretek betöltve", false);
    } catch (const std::exception& e) {
        showStatus("Nincs még mentett méret", true);
    }
}

void ArmSettingsPage::updateSaveButtonState() {
    bool hasData = !bodyweightInput_->text().isEmpty() ||
                   !upperArmInput_->text().isEmpty() ||
                   !forearmInput_->text().isEmpty();
    saveButton_->setEnabled(hasData);
}

void ArmSettingsPage::showStatus(const QString& message, bool isError) {
    statusLabel_->setText(message);
    statusLabel_->setStyleSheet(
        QString("color: %1; font-size: 12px; margin-top: 5px;")
        .arg(isError ? UI::Colors::DANGER : UI::Colors::SUCCESS)
    );
    statusLabel_->setVisible(true);
}

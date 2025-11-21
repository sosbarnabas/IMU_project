#include "SettingsPage.h"
#include "ControlRow.h"
#include "../../Widgets/UIUtilities/StyleHelper.h"
#include "../../Widgets/UIUtilities/Constants.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <algorithm>

// Static members (from SettingsSave)
SettingsState SettingsPage::storedState_{};
bool SettingsPage::hasStoredState_ = false;

namespace {
QStringList sortedList(QStringList list) {
    std::sort(list.begin(), list.end());
    return list;
}

bool statesEqual(const SettingsState& lhs, const SettingsState& rhs) {
    auto lhsEnabled = sortedList(lhs.enabled);
    auto rhsEnabled = sortedList(rhs.enabled);
    return lhsEnabled == rhsEnabled && lhs.texts == rhs.texts;
}
}

SettingsPage::SettingsPage(QWidget* parent) : QWidget(parent) {
    loadSettingsState();

    v_ = new QVBoxLayout(this);
    v_->setContentsMargins(20,20,20,20);
    v_->setSpacing(28);
    buildRows();

    // Create save button directly (inline SettingsSaveBar logic)
    saveButton_ = new QPushButton(tr("Save"), this);
    saveButton_->setVisible(false);
    saveButton_->setStyleSheet(UI::StyleHelper::pillButtonStyle(8));

    refreshButton_ = new QPushButton(tr("Konfiguráció újratöltése"), this);
    refreshButton_->setStyleSheet(UI::StyleHelper::pillButtonStyle(8));

    v_->addStretch(1);
    v_->addWidget(refreshButton_);
    v_->addSpacing(10);
    
    // Add save button with right alignment
    auto* saveLayout = new QHBoxLayout();
    saveLayout->addStretch(1);
    saveLayout->addWidget(saveButton_);
    v_->addLayout(saveLayout);

    connect(saveButton_, &QPushButton::clicked,
            this,        &SettingsPage::onSaveClicked);


    applySavedState();

    emit configChanged(savedState_.enabled);
    emit settingsSaved(savedState_.enabled);
}

void SettingsPage::buildRows() {
    const char* names[] = {
        "MOTOR_E_FLEX","MOTOR_E_EXT","MOTOR_S_FLEX","MOTOR_S_EXT",
                "MOTOR_S_ADD_PRON","MOTOR_S_ABD","MOTOR_S_ADD_SUP"
    };
    for (auto n : names) {
        auto* row = new ControlRow(this);
        row->setLabelText(n);
        row->setFixedSize(360,60);
        rows_ << row;
        v_->addWidget(row);
        connect(row, &ControlRow::switchToggled,
                this, &SettingsPage::onRowToggled);
        connect(row, &ControlRow::textChanged,
                this, &SettingsPage::onRowTextChanged);
    }
}

QStringList SettingsPage::enabledNames() const {
    return currentState().enabled;
}

void SettingsPage::loadSettingsState() {
    // Load from in-memory storage (inline SettingsSave logic)
    hasSavedState_ = hasStoredState_;
    savedState_ = storedState_;
}

void SettingsPage::applySavedState() {
    if (hasSavedState_)
        applyState(savedState_);

    savedState_ = currentState();
    evaluateDirtyState();
}

void SettingsPage::applyState(const SettingsState& state) {
    for (auto* row : rows_) {
        const QSignalBlocker blocker(row);
        const auto label = row->labelText();
        const bool shouldCheck = state.enabled.contains(label);
        row->setSwitchChecked(shouldCheck);
        row->setInputText(state.texts.value(label));
    }
}

SettingsState SettingsPage::currentState() const {
    SettingsState state;
    for (auto* row : rows_) {
        const auto label = row->labelText();
        if (row->isSwitchChecked())
            state.enabled << label;
        state.texts.insert(label, row->inputText());
    }
    return state;
}

void SettingsPage::updateSaveButton(const SettingsState& current) {
    updateSaveBar(dirty_, current.enabled.size());
}

void SettingsPage::updateSaveBar(bool dirty, int pendingCount) {
    if (!saveButton_)
        return;

    if (!dirty) {
        saveButton_->setVisible(false);
        saveButton_->setText(tr("Save"));
        return;
    }

    saveButton_->setVisible(true);
    saveButton_->setText(QStringLiteral("Save (%1 motors)").arg(pendingCount));
}

void SettingsPage::evaluateDirtyState() {
    const auto current = currentState();
    dirty_ = !statesEqual(current, savedState_);
    updateSaveButton(current);
}

void SettingsPage::persistState() {
    // Save to in-memory storage (inline SettingsSave logic)
    storedState_ = savedState_;
    hasStoredState_ = true;
}

void SettingsPage::onRowToggled(bool) {
    evaluateDirtyState();
}

void SettingsPage::onRowTextChanged(const QString&) {
    evaluateDirtyState();
}

void SettingsPage::onSaveClicked() {
    if (!dirty_)
        return;

    savedState_ = currentState();
    hasSavedState_ = true;
    persistState();
    evaluateDirtyState();

    emit configChanged(savedState_.enabled);
    emit settingsSaved(savedState_.enabled);
}

bool SettingsPage::isDirty() const {
    return dirty_;
}

void SettingsPage::saveChanges() {
    onSaveClicked();
}

void SettingsPage::discardChanges() {
    if (!dirty_)
        return;
    applyState(savedState_);
    evaluateDirtyState();
}
void SettingsPage::setEnabledFromTelemetry(const QStringList& names) {
    telemetryMotors_ = names;

    SettingsState state = currentState();
    QStringList filtered;

    for (auto* row : rows_) {
        const QString label = row->labelText();
        const bool inTelemetry = names.contains(label);
        const bool hasSerial = !state.texts.value(label).isEmpty();
        if (inTelemetry && hasSerial)
            filtered << label;
    }

    filtered.removeDuplicates();
    state.enabled = filtered;
    const bool changed = !statesEqual(state, savedState_);

    applyState(state);
    savedState_ = state;
    hasSavedState_ = true;
    evaluateDirtyState();

    if (changed) {
        emit configChanged(savedState_.enabled);
        emit settingsSaved(savedState_.enabled);
    }
}

void SettingsPage::setSerialsFromTelemetry(const QHash<QString, QString>& serials) {
    SettingsState state = currentState();
    QStringList enabled;

    for (auto* row : rows_) {
        const QString label = row->labelText();
        const QString serial = serials.value(label);
        state.texts.insert(label, serial);

        const bool hasSerial = !serial.isEmpty();
        const bool allowed = telemetryMotors_.isEmpty() || telemetryMotors_.contains(label);
        if (hasSerial && allowed)
            enabled << label;
    }

    enabled.removeDuplicates();
    state.enabled = enabled;
    const bool changed = !statesEqual(state, savedState_);

    applyState(state);
    savedState_ = state;
    hasSavedState_ = true;
    evaluateDirtyState();

    if (changed) {
        emit configChanged(savedState_.enabled);
        emit settingsSaved(savedState_.enabled);
    }
}

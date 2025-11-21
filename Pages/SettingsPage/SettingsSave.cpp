#include "SettingsSave.h"

SettingsState SettingsSave::storedState_{};
bool SettingsSave::hasStoredState_ = false;

SettingsSave::SettingsSave() = default;

SettingsState SettingsSave::load(bool* hasSavedState) const {
    if (hasSavedState)
        *hasSavedState = hasStoredState_;
    return storedState_;
}

void SettingsSave::save(const SettingsState& state) {
    storedState_ = state;
    hasStoredState_ = true;
}

void SettingsSave::reset() {
    storedState_ = SettingsState{};
    hasStoredState_ = false;
}

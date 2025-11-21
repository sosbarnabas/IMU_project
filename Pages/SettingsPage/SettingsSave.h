#pragma once

#include <QStringList>
#include <QMap>

struct SettingsState {
    QStringList enabled;
    QMap<QString, QString> texts;
};

class SettingsSave {
public:
    SettingsSave();

    SettingsState load(bool* hasSavedState) const;
    void save(const SettingsState& state);
    void reset();

private:
    static SettingsState storedState_;
    static bool hasStoredState_;
};

#pragma once
#include <QWidget>
#include <QStringList>
#include <QList>
#include <QHash>
#include <QMap>

// Simple state struct (from SettingsSave)
struct SettingsState {
    QStringList enabled;
    QMap<QString, QString> texts;
};

class QVBoxLayout;
class ControlRow;
class QPushButton;

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(QWidget* parent=nullptr);
    QStringList enabledNames() const;
    bool isDirty() const;
    void saveChanges();
    void discardChanges();
    void setEnabledFromTelemetry(const QStringList& names);
    void setSerialsFromTelemetry(const QHash<QString, QString>& serials);

signals:
    void configChanged(const QStringList& names);
    void settingsSaved(const QStringList& names);

private slots:
    void onRowToggled(bool checked);
    void onRowTextChanged(const QString& text);
    void onSaveClicked();

private:
    void buildRows();
    void loadSettingsState();
    void applySavedState();
    void applyState(const SettingsState& state);
    SettingsState currentState() const;
    void updateSaveButton(const SettingsState& current);
    void evaluateDirtyState();
    void persistState();
    void updateSaveBar(bool dirty, int pendingCount);

    QVBoxLayout* v_{};
    QList<ControlRow*> rows_;
    QPushButton* saveButton_{nullptr};
    SettingsState savedState_;
    QPushButton* refreshButton_{nullptr};
    bool dirty_{false};
    bool hasSavedState_{false};
    QStringList telemetryMotors_;
    
    // In-memory state storage (from SettingsSave)
    static SettingsState storedState_;
    static bool hasStoredState_;
};

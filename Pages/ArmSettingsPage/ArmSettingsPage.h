#pragma once

#include <QWidget>

class QLineEdit;
class QPushButton;
class QLabel;
class QVBoxLayout;
class QFormLayout;
class TelemetryController;

namespace exoskeleton::redis {
    class Facade;
}

namespace exoskeleton::settings {
    class UserParams;
}

/**
 * @brief Page for configuring user parameters including arm measurements.
 * 
 * This page allows users to configure the physical measurements and parameters
 * stored in Redis under conf:user key, following the UserParams structure.
 */
class ArmSettingsPage : public QWidget {
    Q_OBJECT
    
public:
    explicit ArmSettingsPage(QWidget* parent = nullptr);
    
    void setTelemetryController(TelemetryController* controller);
    void loadCurrentUserParams();
    void clearFields();
    
signals:
    void userParamsUpdated();
    
private slots:
    void onSaveClicked();
    void onLoadClicked();
    void updateSaveButtonState();
    
private:
    void initializeUI();
    void setupFormLayout();
    void connectSignals();
    void showStatus(const QString& message, bool isError);
    
    QLineEdit* createNumericInput(int minValue, int maxValue);
    void loadUserParams(const exoskeleton::settings::UserParams& params);
    void saveUserParamsToRedis();
    
    // UI Components
    QVBoxLayout* mainLayout_;
    QFormLayout* formLayout_;
    QPushButton* saveButton_;
    QPushButton* loadButton_;
    QLabel* statusLabel_;
    
    // Input fields for user physical measurements only
    QLineEdit* userIdInput_;
    QLineEdit* bodyweightInput_;
    QLineEdit* upperArmInput_;
    QLineEdit* upperArmCuffInput_;
    QLineEdit* forearmInput_;
    QLineEdit* forearmCuffInput_;
    QLineEdit* cuffInput_;
    
    TelemetryController* telemetryController_ = nullptr;
};

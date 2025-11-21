#pragma once

#include <QWidget>
#include <QHash>
#include <QString>
#include <memory>
#include <QDateTime>

namespace exo::redis { class CommandClient; }

class QComboBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QMenu;
class QAction;
class QTimer;
class QCheckBox;
class MotorDataLogger;

class ActionButtons : public QWidget {
    Q_OBJECT
public:
    explicit ActionButtons(QWidget* parent = nullptr);
    ~ActionButtons() override;

    void setRedisUri(const QString& uri);
    void setMotorAddresses(const QHash<int, QString>& addresses);
    void setSelectedAddress(int address);

public slots:
    void logMotorData(const QString& motorName, double timestamp, double position, double torque);

signals:
    void commandSent(const QString& command, int address);
    void commandFailed(const QString& command, int address, const QString& error);

private:
    enum class StatusType {
        Info,
        Success,
        Error
    };

    void buildUi();
    void createButtons();
    QPushButton* createButton(const QString& text, const char* slot);
    void layoutButton(QWidget* button, int row, int column);
    void refreshMotorCombo();

    bool sendCommand(const QString& command, const QString& parameters = QString());
    bool sendCommandWithParameters(const QString& command, const QString& parameters);

    int currentAddress() const;
    QString currentMotorName() const;

    void showStatusMessage(const QString& text, StatusType type);
    void clearStatusMessage();
    void checkPendingResponses();

    struct PendingCommand {
        QString command;
        int address;
        QString uniqueId;
        QDateTime sentTime;
    };

private slots:
    void handleStatus();
    void handleConnect();
    void handleDisconnect();
    void handleEnable();
    void handleDisable();
    void handleZero();
    void handleOffset();
    void handleRead();
    void handleFunctionsMenu();
    void handleFunctionAction(QAction* action);
    void handleLogSnapshot();
    void handleStartLogging();
    void handleStopLogging();

private:
    QComboBox* motorCombo_ {nullptr};
    QCheckBox* applyToAllCheckbox_ {nullptr};
    QLabel* statusLabel_ {nullptr};
    QGridLayout* grid_ {nullptr};
    QPushButton* functionsButton_ {nullptr};
    QPushButton* startLogButton_ {nullptr};
    QPushButton* stopLogButton_ {nullptr};
    QMenu* functionsMenu_ {nullptr};
    QTimer* statusTimer_ {nullptr};
    QTimer* responseCheckTimer_ {nullptr};

    std::unique_ptr<exo::redis::CommandClient> commandClient_;
    std::unique_ptr<MotorDataLogger> dataLogger_;
    QHash<int, QString> addresses_;
    QList<PendingCommand> pendingCommands_;
};
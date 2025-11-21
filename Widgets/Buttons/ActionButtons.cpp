#include "ActionButtons.h"
#include "MotorDataLogger.h"
#include "CommandClient.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QGridLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>
#include <QPoint>
#include <QList>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDir>
#include <QThread>

#include <algorithm>

namespace {
    const QString checkboxStyle =
        "QCheckBox {"
    "  color: #E6E6E6;"
    "  font-size: 12px;"
    "  spacing: 5px;"
    "}"
    "QCheckBox::indicator {"
    "  width: 30px;"
    "  height: 30px;"
    "  border-radius: 10px;"
    "  background-color: transparent;"
    "  border: 2px solid #ffffff;"  // Adding a white border
    "}"
    "QCheckBox::indicator:unchecked {"
    "  background-color: transparent;"
    "}"
    "QCheckBox::indicator:checked {"
    "  background-color: #ffffff;"
    "  image: none;"
    "}";

    constexpr auto kButtonStyle =
        "QPushButton {"
        "  padding: 10px 20px;"
        "  text-transform: uppercase;"
        "  border-radius: 8px;"
        "  font-size: 17px;"
        "  font-weight: 500;"
        "  color: rgba(255, 255, 255, 128);"
        "  background: transparent;"
        "  border: 1px solid rgba(255, 255, 255, 128);"
        "  transition: 0.5s;"
        "}"
        "QPushButton:hover {"
        "  color: #ffffff;"
        "  background: #008cff;"
        "  border: 1px solid #008cff;"
        "  font-weight: bold;"
        "}"
        "QPushButton:pressed {"
        "  background: #2DA8FF;"
        "}"
        "QPushButton:focus {"
        "  outline: none;"
        "}";

constexpr auto kStatusInfoColor    = "#BDBDBD";
constexpr auto kStatusSuccessColor = "#27AE60";
constexpr auto kStatusErrorColor   = "#EB5757";

} // namespace

ActionButtons::ActionButtons(QWidget* parent)
    : QWidget(parent)
    , commandClient_(std::make_unique<exo::redis::CommandClient>(QString()))
    , dataLogger_(std::make_unique<MotorDataLogger>(this))
{
    setAttribute(Qt::WA_StyledBackground, true);
    grid_ = new QGridLayout();
    createButtons();
    buildUi();

    // Setup response check timer (check every 500ms)
    responseCheckTimer_ = new QTimer(this);
    responseCheckTimer_->setInterval(500);
    connect(responseCheckTimer_, &QTimer::timeout, this, &ActionButtons::checkPendingResponses);
    responseCheckTimer_->start();
    
    // Connect logger signals
    connect(dataLogger_.get(), &MotorDataLogger::statusMessage, 
            this, [this](const QString& msg) {
        showStatusMessage(msg, StatusType::Success);
    });
    
    connect(dataLogger_.get(), &MotorDataLogger::loggingError,
            this, [this](const QString& error) {
        showStatusMessage(error, StatusType::Error);
    });
    
    connect(dataLogger_.get(), &MotorDataLogger::loggingStarted,
            this, [this](const QString&) {
        if (startLogButton_) startLogButton_->setEnabled(false);
        if (stopLogButton_) stopLogButton_->setEnabled(true);
    });
    
    connect(dataLogger_.get(), &MotorDataLogger::loggingStopped,
            this, [this](const QString&, int) {
        if (startLogButton_) startLogButton_->setEnabled(true);
        if (stopLogButton_) stopLogButton_->setEnabled(false);
    });
}

ActionButtons::~ActionButtons() = default;

void ActionButtons::setRedisUri(const QString& uri)
{
    if (!commandClient_) {
        return;
    }

    if (commandClient_->uri() == uri) {
        return;
    }

    commandClient_->setUri(uri);
    showStatusMessage(tr("Redis connection updated."), StatusType::Info);
}

void ActionButtons::setMotorAddresses(const QHash<int, QString>& addresses)
{
    addresses_ = addresses;
    refreshMotorCombo();
}

void ActionButtons::setSelectedAddress(int address)
{
    if (!motorCombo_)
        return;

    for (int i = 0; i < motorCombo_->count(); ++i) {
        if (motorCombo_->itemData(i).toInt() == address) {
            motorCombo_->setCurrentIndex(i);
            return;
        }
    }
}

void ActionButtons::buildUi()
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    //auto* title = new QLabel(tr("Motor parancsok"), this);
    //title->setStyleSheet(QStringLiteral("color:#E6E6E6;font-weight:600;font-size:16px;"));

    motorCombo_ = new QComboBox(this);
    motorCombo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    motorCombo_->setMinimumWidth(200);
    motorCombo_->setStyleSheet("QComboBox{background:#1F1E23;color:#E6E6E6;border:1px solid #2C2B31;"
                               "border-radius:6px;padding:6px 10px;}"
                               "QComboBox:hover{border-color:#3B3A41;}"
                               "QComboBox QAbstractItemView{background:#1F1E23;color:#E6E6E6;}");


    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setHorizontalSpacing(8);
    grid_->setVerticalSpacing(8);


    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    statusLabel_->setStyleSheet(QStringLiteral("color:%1;font-size:12px;").arg(kStatusInfoColor));

    statusTimer_ = new QTimer(this);
    statusTimer_->setSingleShot(true);
    connect(statusTimer_, &QTimer::timeout, this, &ActionButtons::clearStatusMessage);

    applyToAllCheckbox_ = new QCheckBox(tr("Apply to all motors"), this);
    applyToAllCheckbox_->setStyleSheet(checkboxStyle);
    grid_->addWidget(motorCombo_, 0, 3);
    grid_->addWidget(applyToAllCheckbox_,1,3);
    grid_->addWidget(statusLabel_, 2, 3);

    layout->addLayout(grid_);

}

void ActionButtons::createButtons()
{
    layoutButton(createButton(tr("Status"), SLOT(handleStatus())), 0, 0);
    layoutButton(createButton(tr("Connect"), SLOT(handleConnect())), 0, 1);
    layoutButton(createButton(tr("Disconnect"), SLOT(handleDisconnect())), 0, 2);

    layoutButton(createButton(tr("Enable"), SLOT(handleEnable())), 1, 0);
    layoutButton(createButton(tr("Disable"), SLOT(handleDisable())), 1, 1);
    layoutButton(createButton(tr("Zero"), SLOT(handleZero())), 1, 2);

    layoutButton(createButton(tr("Offset"), SLOT(handleOffset())), 2, 0);
    layoutButton(createButton(tr("Read"), SLOT(handleRead())), 2, 1);

    functionsButton_ = createButton(tr("Functions"), SLOT(handleFunctionsMenu()));
    layoutButton(functionsButton_, 2, 2);
    
    // Logging buttons
    auto* logSnapshotBtn = createButton(tr("Log Snapshot"), SLOT(handleLogSnapshot()));
    layoutButton(logSnapshotBtn, 3, 0);
    
    startLogButton_ = createButton(tr("Start Log"), SLOT(handleStartLogging()));
    layoutButton(startLogButton_, 3, 1);
    
    stopLogButton_ = createButton(tr("Stop Log"), SLOT(handleStopLogging()));
    stopLogButton_->setEnabled(false);  // Disabled until logging starts
    layoutButton(stopLogButton_, 3, 2);

    functionsMenu_ = new QMenu(this);
    auto* fnGet    = functionsMenu_->addAction(tr("List (fn_get)"));
    auto* fnSelect = functionsMenu_->addAction(tr("Select (fn_select)"));
    auto* fnUpload = functionsMenu_->addAction(tr("Upload (fn_upload)"));

    connect(functionsMenu_, &QMenu::triggered, this, &ActionButtons::handleFunctionAction);

    fnGet->setData(QStringLiteral("fn_get"));
    fnSelect->setData(QStringLiteral("fn_select"));
    fnUpload->setData(QStringLiteral("fn_upload"));
}

QPushButton* ActionButtons::createButton(const QString& text, const char* slot)
{
    auto* button = new QPushButton(text, this);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::StrongFocus);
    button->setStyleSheet(QString::fromLatin1(kButtonStyle));
    if (slot)
        connect(button, SIGNAL(clicked()), this, slot);
    return button;
}

void ActionButtons::layoutButton(QWidget* button, int row, int column)
{
    if (!grid_ || !button)
        return;
    grid_->addWidget(button, row, column);
}

void ActionButtons::refreshMotorCombo()
{
    if (!motorCombo_)
        return;

    QVariant previousData;
    if (motorCombo_->currentIndex() >= 0)
        previousData = motorCombo_->currentData();

    motorCombo_->blockSignals(true);
    motorCombo_->clear();

    QList<int> keys = addresses_.keys();
    std::sort(keys.begin(), keys.end());

    for (int addr : keys) {
        const QString name = addresses_.value(addr);
        const QString display = name.isEmpty()
            ? tr("Address %1").arg(addr)
            : tr("%1 (%2)").arg(name, QString::number(addr));
        motorCombo_->addItem(display, addr);
    }

    if (motorCombo_->count() > 0) {
        int index = 0;
        if (previousData.isValid()) {
            const int previousAddress = previousData.toInt();
            for (int i = 0; i < motorCombo_->count(); ++i) {
                if (motorCombo_->itemData(i).toInt() == previousAddress) {
                    index = i;
                    break;
                }
            }
        }
        motorCombo_->setCurrentIndex(index);
    } else {
        motorCombo_->setCurrentIndex(-1);
    }

    motorCombo_->blockSignals(false);
}

bool ActionButtons::sendCommand(const QString& command, const QString& parameters)
{
    if (!commandClient_) {
        showStatusMessage(tr("Command client not initialized."), StatusType::Error);
        return false;
    }

    // Check if "apply to all" is enabled
    const bool applyToAll = applyToAllCheckbox_ && applyToAllCheckbox_->isChecked();

    if (applyToAll) {
        // Send command to all motors
        if (addresses_.isEmpty()) {
            showStatusMessage(tr("No motor addresses available."), StatusType::Error);
            return false;
        }

        int successCount = 0;
        int failCount = 0;
        QString lastError;

        QList<int> sortedAddresses = addresses_.keys();
        std::sort(sortedAddresses.begin(), sortedAddresses.end());

        for (int address : sortedAddresses) {
            const auto result = commandClient_->sendCommand(command, address, parameters);
            if (result.success) {
                ++successCount;
                emit commandSent(command, address);
                
                // Track command for response checking
                if (!result.uniqueId.isEmpty()) {
                    pendingCommands_.append({command, address, result.uniqueId, QDateTime::currentDateTime()});
                    qDebug() << "[ActionButtons] Tracking command:" << command << "address:" << address << "uniqueId:" << result.uniqueId;
                }
            } else {
                ++failCount;
                lastError = result.errorMessage;
                emit commandFailed(command, address, result.errorMessage);
            }
        }

        if (failCount == 0) {
            showStatusMessage(tr("Command %1 sent to all %2 motors successfully")
                                .arg(command, QString::number(successCount)), StatusType::Success);
            return true;
        } else if (successCount > 0) {
            showStatusMessage(tr("Command %1: %2 succeeded, %3 failed. Last error: %4")
                                .arg(command, QString::number(successCount), 
                                     QString::number(failCount), lastError), StatusType::Error);
            return false;
        } else {
            showStatusMessage(tr("Command %1 failed for all motors. Error: %2")
                                .arg(command, lastError), StatusType::Error);
            return false;
        }
    } else {
        // Send command to selected motor only
        const int address = currentAddress();
        if (address < 0) {
            showStatusMessage(tr("No motor address selected."), StatusType::Error);
            return false;
        }

        const auto result = commandClient_->sendCommand(command, address, parameters);
        
        if (!result.success) {
            const QString error = tr("Redis error: %1").arg(result.errorMessage);
            showStatusMessage(error, StatusType::Error);
            emit commandFailed(command, address, error);
            return false;
        }

        // Track command for response checking
        if (!result.uniqueId.isEmpty()) {
            pendingCommands_.append({command, address, result.uniqueId, QDateTime::currentDateTime()});
            qDebug() << "[ActionButtons] Tracking command:" << command << "address:" << address << "uniqueId:" << result.uniqueId;
        }

        const QString targetName = currentMotorName();
        const QString message = targetName.isEmpty()
            ? tr("Command sent: %1 (address: %2)").arg(command, QString::number(address))
            : tr("Command sent: %1 → %2").arg(command, targetName);

        showStatusMessage(message, StatusType::Success);
        emit commandSent(command, address);
        return true;
    }
}

bool ActionButtons::sendCommandWithParameters(const QString& command, const QString& parameters)
{
    const QString trimmed = parameters.trimmed();
    if (trimmed.isEmpty()) {
        showStatusMessage(tr("The %1 command requires parameters.").arg(command), StatusType::Error);
        return false;
    }
    return sendCommand(command, trimmed);
}

int ActionButtons::currentAddress() const
{
    if (!motorCombo_ || motorCombo_->currentIndex() < 0)
        return 0;
    return motorCombo_->currentData().toInt();
}

QString ActionButtons::currentMotorName() const
{
    const int address = currentAddress();
    return addresses_.value(address);
}

void ActionButtons::showStatusMessage(const QString& text, StatusType type)
{
    if (!statusLabel_)
        return;

    QString color;
    switch (type) {
    case StatusType::Success:
        color = QString::fromLatin1(kStatusSuccessColor);
        break;
    case StatusType::Error:
        color = QString::fromLatin1(kStatusErrorColor);
        break;
    case StatusType::Info:
    default:
        color = QString::fromLatin1(kStatusInfoColor);
        break;
    }

    statusLabel_->setStyleSheet(QStringLiteral("color:%1;font-size:12px;").arg(color));
    statusLabel_->setText(text);

    if (statusTimer_)
        statusTimer_->start(5000);
}

void ActionButtons::clearStatusMessage()
{
    if (!statusLabel_)
        return;

    statusLabel_->clear();
    statusLabel_->setStyleSheet(QStringLiteral("color:%1;font-size:12px;").arg(kStatusInfoColor));
}

void ActionButtons::handleStatus()
{
    sendCommand(QStringLiteral("status"));
}

void ActionButtons::handleConnect()
{
    sendCommand(QStringLiteral("connect"));
}

void ActionButtons::handleDisconnect()
{
    sendCommand(QStringLiteral("disconnect"));
}

void ActionButtons::handleEnable()
{
    sendCommand(QStringLiteral("enable"));
}

void ActionButtons::handleDisable()
{
    sendCommand(QStringLiteral("disable"));
}

void ActionButtons::handleZero()
{
    sendCommand(QStringLiteral("zero"));
}

void ActionButtons::handleOffset()
{
    bool ok = false;
    const int value = QInputDialog::getInt(this,
                                           tr("Offset"),
                                           tr("Enter offset value:"),
                                           0,
                                           -1000000,
                                           1000000,
                                           1,
                                           &ok);
    if (!ok)
        return;

    sendCommand(QStringLiteral("offset"), QString::number(value));
}

void ActionButtons::handleRead()
{
    sendCommand(QStringLiteral("read"));
}

void ActionButtons::handleFunctionsMenu()
{
    if (!functionsMenu_ || !functionsButton_)
        return;

    const QPoint globalPos = functionsButton_->mapToGlobal(QPoint(0, functionsButton_->height()));
    functionsMenu_->exec(globalPos);
}

void ActionButtons::handleFunctionAction(QAction* action)
{
    if (!action)
        return;

    const QString command = action->data().toString();
    if (command.isEmpty())
        return;

    if (command == QLatin1String("fn_get")) {
        sendCommand(command);
        return;
    }

    if (command == QLatin1String("fn_select")) {
        bool ok = false;
        const int slot = QInputDialog::getInt(this,
                                              tr("Select Function"),
                                              tr("Slot number:"),
                                              0,
                                              0,
                                              1000,
                                              1,
                                              &ok);
        if (!ok)
            return;
        sendCommand(command, QString::number(slot));
        return;
    }

    if (command == QLatin1String("fn_upload")) {
        bool ok = false;
        const QString parameters = QInputDialog::getMultiLineText(
            this,
            tr("Upload Function"),
            tr("Enter parameters (slot number first, then values):"),
            QString(),
            &ok);
        if (!ok || parameters.trimmed().isEmpty())
            return;
        sendCommandWithParameters(command, parameters);
    }
}

void ActionButtons::checkPendingResponses()
{
    if (pendingCommands_.isEmpty()) {
        return;
    }

    QDateTime now = QDateTime::currentDateTime();
    QMutableListIterator<PendingCommand> it(pendingCommands_);
    
    while (it.hasNext()) {
        const PendingCommand& pending = it.next();
        
        // Timeout after 5 seconds
        if (pending.sentTime.msecsTo(now) > 5000) {
            qWarning() << "[ActionButtons] Command timeout:" << pending.command << "address:" << pending.address;
            it.remove();
            continue;
        }
        
        // Check for response
        auto response = commandClient_->checkCommandResponse(pending.command, pending.address, pending.uniqueId);
        
        if (response.found) {
            if (response.success) {
                QString msg = tr("✓ %1 (addr %2): %3")
                    .arg(pending.command)
                    .arg(pending.address)
                    .arg(response.value.isEmpty() ? tr("OK") : response.value);
                showStatusMessage(msg, StatusType::Success);
                qDebug() << "[ActionButtons] Command succeeded:" << pending.command << "value:" << response.value;
            } else {
                QString msg = tr("✗ %1 (addr %2): %3")
                    .arg(pending.command)
                    .arg(pending.address)
                    .arg(response.value);
                showStatusMessage(msg, StatusType::Error);
                qWarning() << "[ActionButtons] Command failed:" << pending.command << "error:" << response.value;
            }
            it.remove();
        }
    }
}

void ActionButtons::handleLogSnapshot()
{
    if (dataLogger_) {
        dataLogger_->logManualSnapshot();
    }
}

void ActionButtons::handleStartLogging()
{
    if (dataLogger_) {
        dataLogger_->startContinuousLogging();
    }
}

void ActionButtons::handleStopLogging()
{
    if (dataLogger_) {
        dataLogger_->stopContinuousLogging();
    }
}

void ActionButtons::logMotorData(const QString& motorName, double timestamp, double position, double torque)
{
    if (dataLogger_) {
        dataLogger_->updateMotorData(motorName, timestamp, position, torque);
    }
}

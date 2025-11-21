#include "SpringPage.h"

#include "FunctionEditorWidget.h"
#include "CommandClient.h"
#include "SlotFunctionManager.h"
#include "../../Widgets/UIUtilities/StyleHelper.h"
#include "../../Widgets/UIUtilities/Constants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QFont>
#include <QDir>
#include <QPoint>
#include <QSizePolicy>
#include <QInputDialog>
#include <QtMath>
#include <QDebug>
#include <QApplication>

SpringPage::SpringPage(QWidget *parent)
    : QWidget(parent)
    , commandClient_(std::make_unique<exo::redis::CommandClient>(QString()))
{
    setAttribute(Qt::WA_StyledBackground, true);
    buildLayout();
}

void SpringPage::buildLayout() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 16);
    layout->setSpacing(16);

    auto *title = new QLabel(tr("Spring Editor"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title, 0, Qt::AlignLeft);

    editor_ = new FunctionEditorWidget(this);
    editor_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(editor_, 1);

    connect(editor_, &FunctionEditorWidget::arraySaved,
            this, &SpringPage::springProfileApplied);
    connect(editor_, &FunctionEditorWidget::uploadRequested,
            this, [this](int slot, int motorAddress, const QVector<double>& values) {
                qDebug() << "[SpringPage] Upload requested for motor" << motorAddress << "slot" << slot;
                
                // Apply the function locally (display in slot widgets)
                emit springProfileApplied(values);
                
                // If motorAddress is -1, upload to all motors
                if (motorAddress == -1) {
                    QList<int> addresses = motorAddresses_.keys();
                    std::sort(addresses.begin(), addresses.end());
                    
                    int successCount = 0;
                    int failCount = 0;
                    
                    for (int addr : addresses) {
                        if (uploadFunctionToMotor(addr, slot, values)) {
                            ++successCount;
                        } else {
                            ++failCount;
                        }
                    }
                    
                    if (failCount == 0) {
                        QMessageBox::information(this, tr("Upload Complete"),
                                               tr("Spring function uploaded to all %1 motors successfully in slot %2.")
                                                   .arg(successCount).arg(slot));
                    } else {
                        QMessageBox::warning(this, tr("Upload Completed With Errors"),
                                           tr("Upload results: %1 succeeded, %2 failed.")
                                               .arg(successCount).arg(failCount));
                    }
                } else {
                    // Single motor
                    if (uploadFunctionToMotor(motorAddress, slot, values)) {
                        QMessageBox::information(this, tr("Upload Complete"),
                                               tr("Spring function uploaded to motor %1 in slot %2.")
                                                   .arg(motorAddress).arg(slot));
                    } else {
                        QMessageBox::warning(this, tr("Upload Failed"),
                                           tr("Failed to upload function to motor %1.")
                                               .arg(motorAddress));
                    }
                }
            });
    
    connect(editor_, &FunctionEditorWidget::loadRequested,
            this, &SpringPage::handleLoadRequested);
    connect(editor_, &FunctionEditorWidget::saveRequested,
            this, &SpringPage::handleSaveRequested);
    connect(editor_, &FunctionEditorWidget::resetRequested,
            this, &SpringPage::handleResetRequested);
}

void SpringPage::handleLoadRequested() {
    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          tr("Load Spring Profile"),
                                                          QDir::homePath(),
                                                          tr("Spring Profiles (*.json);;All Files (*)"));
    if (fileName.isEmpty())
        return;

    loadFromFile(fileName);
}

void SpringPage::handleSaveRequested() {
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Save Spring Profile"),
                                                    QDir::homePath(),
                                                    tr("Spring Profiles (*.json);;All Files (*)"));
    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(".json", Qt::CaseInsensitive))
        fileName.append(".json");

    saveToFile(fileName);
}

void SpringPage::handleResetRequested() {
    if (editor_)
        editor_->resetToDefault();
}

bool SpringPage::loadFromFile(const QString &path) {
    if (path.isEmpty())
        return false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Load Failed"),
                             tr("Could not open %1 for reading.").arg(path));
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        QMessageBox::warning(this, tr("Invalid File"),
                             tr("The selected file does not contain a valid spring profile. (%1)").arg(parseError.errorString()));
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonValue pointsValue = root.value(QStringLiteral("points"));
    if (!pointsValue.isArray()) {
        QMessageBox::warning(this, tr("Invalid File"),
                             tr("The spring profile is missing a valid 'points' array."));
        return false;
    }

    const QJsonArray pointsArray = pointsValue.toArray();
    QVector<QPoint> points;
    points.reserve(pointsArray.size());

    for (const QJsonValue &value : pointsArray) {
        if (!value.isObject())
            continue;
        const QJsonObject obj = value.toObject();
        if (!obj.contains(QStringLiteral("x")) || !obj.contains(QStringLiteral("y")))
            continue;
        const QJsonValue xVal = obj.value(QStringLiteral("x"));
        const QJsonValue yVal = obj.value(QStringLiteral("y"));
        if (!xVal.isDouble() || !yVal.isDouble())
            continue;
        const int x = qRound(xVal.toDouble());
        const int y = qRound(yVal.toDouble());
        points.append(QPoint(x, y));
    }

    if (points.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid File"),
                             tr("No valid control points were found in the selected file."));
        return false;
    }

    if (editor_)
        editor_->setControlPoints(points);

    return true;
}

bool SpringPage::saveToFile(const QString &path) {
    if (path.isEmpty())
        return false;

    if (!editor_)
        return false;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Save Failed"),
                             tr("Could not open %1 for writing.").arg(path));
        return false;
    }

    QJsonArray pointsArray;
    const auto points = editor_->controlPoints();

    for (const QPoint &pt : points) {
        QJsonObject obj;
        obj.insert(QStringLiteral("x"), pt.x());
        obj.insert(QStringLiteral("y"), pt.y());
        pointsArray.append(obj);
    }

    QJsonObject root;
    root.insert(QStringLiteral("points"), pointsArray);

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    return true;
}

void SpringPage::setRedisUri(const QString& uri)
{
    qDebug() << "[SpringPage] setRedisUri called with URI:" << uri;
    if (commandClient_) {
        commandClient_->setUri(uri);
        qDebug() << "[SpringPage] Redis URI set successfully in CommandClient";
    } else {
        qWarning() << "[SpringPage] CommandClient is null, cannot set Redis URI";
    }
}

void SpringPage::setMotorAddresses(const QHash<int, QString>& addresses)
{
    qDebug() << "[SpringPage] setMotorAddresses called with" << addresses.size() << "addresses";
    motorAddresses_ = addresses;
    
    // Update the FunctionEditorWidget with available motors
    if (editor_) {
        editor_->setAvailableMotors(addresses);
        qDebug() << "[SpringPage] Updated FunctionEditorWidget with motor addresses";
    }
}

void SpringPage::setFunctionManager(SlotFunctionManager* manager)
{
    qDebug() << "[SpringPage] setFunctionManager called";
    functionManager_ = manager;
}

bool SpringPage::uploadFunctionToMotor(int address, int slot, const QVector<double>& values)
{
    qDebug() << "[SpringPage] uploadFunctionToMotor: address=" << address << "slot=" << slot << "values.size=" << values.size();
    
    if (!commandClient_) {
        qWarning() << "[SpringPage] CommandClient is null";
        return false;
    }

    if (!commandClient_->ensureConnection()) {
        qWarning() << "[SpringPage] Failed to connect to Redis";
        QMessageBox::warning(this, tr("Redis Error"),
                           tr("Failed to connect to Redis. Check your connection settings."));
        return false;
    }

    // Validate values
    if (values.size() != 360) {
        qWarning() << "[SpringPage] Invalid values size:" << values.size() << "(expected 360)";
        return false;
    }

    // Build parameters: slot number followed by 360 values
    QString parameters = "["+QString::number(slot);
    for (const double& value : values) {
        const int intValue = qRound(value);
        // Validate range (0-127 for typical motor functions)
        if (intValue < 0 || intValue > 127) {
            qWarning() << "[SpringPage] Value out of range at index:" << parameters.split(' ').size() - 1 
                      << "value:" << intValue;
        }
        parameters += "," + QString::number(intValue);
    }
        parameters += "]";
    qDebug() << "[SpringPage] Sending fn_upload command with" << (parameters.split(' ').size() - 1) << "values";
    const auto result = commandClient_->sendCommand("fn_upload", address, parameters);
    
    if (!result.success) {
        qWarning() << "[SpringPage] fn_upload failed:" << result.errorMessage;
        QMessageBox::warning(this, tr("Upload Failed"),
                           tr("Failed to upload function to motor %1: %2")
                               .arg(address).arg(result.errorMessage));
        return false;
    }

    qDebug() << "[SpringPage] fn_upload succeeded for address:" << address;
    
    // Cache the uploaded function so it can be retrieved when the slot is selected
    if (functionManager_) {
        functionManager_->cacheFunction(address, slot, values);
        qDebug() << "[SpringPage] Cached uploaded function for address:" << address << "slot:" << slot;
    } else {
        qDebug() << "[SpringPage] Warning: functionManager_ is null, function not cached";
    }
    
    return true;
}

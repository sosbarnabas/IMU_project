#pragma once

#include <QWidget>
#include <QVector>
#include <QHash>

class QPushButton;
class QLabel;
class FunctionEditorWidget;
class SlotFunctionManager;

namespace exo::redis { 
    class CommandClient; 
    struct CommandResult;
}

class SpringPage : public QWidget {
    Q_OBJECT
public:
    explicit SpringPage(QWidget *parent = nullptr);

    void setRedisUri(const QString& uri);
    void setMotorAddresses(const QHash<int, QString>& addresses);
    void setFunctionManager(SlotFunctionManager* manager);

signals:
    void springProfileApplied(const QVector<double> &values);

private slots:
    void handleLoadRequested();
    void handleSaveRequested();
    void handleResetRequested();

private:
    void buildLayout();
    bool loadFromFile(const QString &path);
    bool saveToFile(const QString &path);
    bool uploadFunctionToMotor(int address, int slot, const QVector<double>& values);

    FunctionEditorWidget *editor_ = nullptr;

    std::unique_ptr<exo::redis::CommandClient> commandClient_;
    QHash<int, QString> motorAddresses_;
    SlotFunctionManager* functionManager_ = nullptr;
};

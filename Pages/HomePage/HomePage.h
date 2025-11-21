#pragma once
#include <QWidget>
#include <QStringList>
#include <QVector>
#include "SlotWidget.h"
#include <QHash>
#include <QTimer>

class QHBoxLayout;
class QVBoxLayout;
class SlotWidget;
class ActionButtons;
class LivePlotWidget;
class ImuWidget;
class QCheckBox;

class HomePage : public QWidget {
    Q_OBJECT
public:
    explicit HomePage(QWidget* parent=nullptr);

signals:
    void slotFunctionAddRequested(const QString& motorName);
    void slotFunctionCreateRequested(const QString& motorName);
    void slotSelectionRequested(const QString& motorName);

public slots:
    void setMotorAddresses(const QHash<int, QString>& addresses);
    void setCommandRedisUri(const QString& uri);
    void setMotors(const QStringList& names);
    void updateMotorData(const QString& motorName, double timestamp, double position, double torque = 0.0);
    void onMotorEnabled(const QString& motorName, bool enabled);

public:
    SlotWidget* slotAt(int index) const;
    int slotCount() const;
    SlotWidget* slotForMotor(const QString& name) const;
    LivePlotWidget* livePlot() const { return livePlot_; }

private:
    QHBoxLayout* h_{};
    QWidget* rightPanel_{};
    QVBoxLayout* right_{};
    void clearRight();
    ActionButtons* actions_{};
    QVector<SlotWidget*> slots_{};
    QHash<QString, SlotWidget*> slotByName_{};
    
    // LivePlot components
    LivePlotWidget* livePlot_{};
    QCheckBox* plotToggle_{};
    QWidget* plotContainer_{};
    
    // IMU widget
    ImuWidget* imuWidget_{};
    
    // Resize optimization
    QTimer* resizeTimer_{};
    int lastCalculatedHeight_{0};
    
private slots:
    void performDelayedResize();
    
protected:
    void resizeEvent(QResizeEvent* e) override;
};

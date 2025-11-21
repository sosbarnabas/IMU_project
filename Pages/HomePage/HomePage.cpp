#include "HomePage.h"
#include "SlotWidget.h"
#include "LivePlotWidget.h"
#include "ImuWidget.h"
#include "MotorColorScheme.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QTimer>
#include "ActionButtons.h"
HomePage::HomePage(QWidget* parent) : QWidget(parent) {
    h_ = new QHBoxLayout(this);
    h_->setContentsMargins(0,0,0,0);
    h_->setSpacing(0);

    // Left panel with plot and buttons
    auto* left = new QWidget;
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(20, 20, 20, 20);
    leftLayout->setSpacing(20);

    // Create LivePlot section at the top of left panel
    plotContainer_ = new QWidget(left);
    auto* plotLayout = new QVBoxLayout(plotContainer_);
    plotLayout->setContentsMargins(0, 0, 0, 0);
    plotLayout->setSpacing(5);
    
    // Global plot toggle
    plotToggle_ = new QCheckBox("Show Live Plot", plotContainer_);
    plotToggle_->setChecked(true);
    
    livePlot_ = new LivePlotWidget(plotContainer_);
    livePlot_->setTimeWindow(10.0);  // 10 seconds window
    livePlot_->setUpdateRate(17);    // 20 FPS for smooth performance
    
    plotLayout->addWidget(plotToggle_);
    plotLayout->addWidget(livePlot_, 0);  // Give plot stretch factor to fill space
    
    // Connect plot toggle
    connect(plotToggle_, &QCheckBox::toggled, livePlot_, &LivePlotWidget::setGlobalVisible);
    
    // Add plot container to left layout - it will take most of the space
    leftLayout->addWidget(plotContainer_, 1);  // Stretch factor 1
    
    // IMU widget in the middle (fixed height)
    imuWidget_ = new ImuWidget(left);
    leftLayout->addWidget(imuWidget_, 0);
    
    // Action buttons at the bottom
    actions_ = new ActionButtons(left);
    leftLayout->addWidget(actions_, 0, Qt::AlignLeft | Qt::AlignBottom);
    
    h_->addWidget(left, 3);

    // Right panel with slot widgets
    rightPanel_ = new QWidget(this);
    right_ = new QVBoxLayout(rightPanel_);
    right_->setContentsMargins(20,20,20,20);
    right_->setSpacing(5);
    right_->addStretch(1);

    h_->addWidget(rightPanel_,1);
    
    // Setup resize throttling timer
    resizeTimer_ = new QTimer(this);
    resizeTimer_->setSingleShot(true);
    resizeTimer_->setInterval(100);  // 100ms debounce
    connect(resizeTimer_, &QTimer::timeout, this, &HomePage::performDelayedResize);
}

void HomePage::setMotorAddresses(const QHash<int, QString>& addresses)
{
    if (actions_)
        actions_->setMotorAddresses(addresses);
}

void HomePage::setCommandRedisUri(const QString& uri)
{
    if (actions_)
        actions_->setRedisUri(uri);
    
    if (imuWidget_) {
        imuWidget_->setRedisUri(uri);

        imuWidget_->startReading();
    }
}

void HomePage::clearRight() {
    // Delete all items except the final stretch
    while (right_->count() > 1) {
        auto* item = right_->takeAt(0);
        if (auto* w = item->widget()) { 
            w->deleteLater();
        }
        delete item;
    }
    slots_.clear();
    slotByName_.clear();
    
    // Clear motors from live plot
    if (livePlot_) {
        livePlot_->clearMotors();
    }
}

void HomePage::setMotors(const QStringList& names) {
    clearRight();

    QStringList normalizedNames;
    normalizedNames.reserve(names.size());
    for (const auto& name : names) {
        const auto normalized = name.trimmed().toUpper();
        if (!normalized.isEmpty() && !normalizedNames.contains(normalized)) {
            normalizedNames.append(normalized);
        }
    }

    // All motor names in order
    const QStringList allMotors = {
        "MOTOR_E_FLEX", "MOTOR_E_EXT", "MOTOR_S_FLEX", "MOTOR_S_EXT",
        "MOTOR_S_ADD_PRON", "MOTOR_S_ABD", "MOTOR_S_ADD_SUP"
    };

    // Calculate the fixed height based on current screen dimensions (always for all 7 slots)
    const int TOTAL_SLOTS = 7;
    QMargins m = right_->contentsMargins();
    const int spacing = right_->spacing();
    int avail = rightPanel_->height() - m.top() - m.bottom();
    int fixedHeight = (avail - spacing * (TOTAL_SLOTS - 1)) / TOTAL_SLOTS;
    fixedHeight = qMax(60, fixedHeight);

    // Only add visible widgets - gaps will close automatically
    const auto addSlot = [&](const QString& motorName) {
        const QString normalized = motorName.trimmed().toUpper();
        auto* slot = new SlotWidget(rightPanel_);
        slot->setTitle(normalized);
        // Don't set slotId here - it will be set by telemetry data (active slot indicator)
        slot->setFixedHeight(fixedHeight);  // Fixed size, never changes
        slot->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

        // Set motor as active by default (will be updated by telemetry)
        slot->setMotorActive(true);
        
        // Set motor color from color scheme
        QColor motorColor = MotorColorScheme::colorForMotor(normalized);
        slot->setMotorColor(motorColor);
        
        // Add motor to live plot with the same color
        if (livePlot_) {
            livePlot_->addMotor(normalized, motorColor);
        }
        
        // Insert before final stretch
        right_->insertWidget(right_->count() - 1, slot);
        slots_.append(slot);
        slotByName_.insert(normalized, slot);

        // Connect SlotWidget button signals
        connect(slot, &SlotWidget::addFunctionRequested, this, [this, normalized]() {
            emit slotFunctionAddRequested(normalized);
        });
        connect(slot, &SlotWidget::createFunctionRequested, this, [this, normalized]() {
            emit slotFunctionCreateRequested(normalized);
        });
        connect(slot, &SlotWidget::selectSlotRequested, this, [this, normalized]() {
            emit slotSelectionRequested(normalized);
        });
        
        // Connect plot visibility toggle
        connect(slot, &SlotWidget::plotVisibilityChanged, this, [this, normalized](bool visible) {
            if (livePlot_) {
                livePlot_->setMotorVisible(normalized, visible);
            }
        });
    };
    for (const auto& motorName : allMotors) {
        if (normalizedNames.contains(motorName)) {
            addSlot(motorName);
        }
    }

    for (const auto& name : normalizedNames) {
        if (!allMotors.contains(name)) {
            addSlot(name);
        }
    }
}
SlotWidget* HomePage::slotAt(int index) const {
    if (index < 0 || index >= slots_.size())
        return nullptr;
    return slots_.at(index);
}

int HomePage::slotCount() const {
    return slots_.size();
}
SlotWidget* HomePage::slotForMotor(const QString& name) const {
    const auto key = name.trimmed().toUpper();
    const auto it = slotByName_.find(key);
    if (it != slotByName_.end()) {
        return it.value();
    }
    return nullptr;
}

void HomePage::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    
    // Throttle resize calculations to avoid excessive updates
    resizeTimer_->start();
}

void HomePage::performDelayedResize() {
    // Count actual slot widgets (exclude stretch)
    int widgetCount = 0;
    for (int i = 0; i < right_->count() - 1; ++i) {
        if (right_->itemAt(i)->widget()) {
            widgetCount++;
        }
    }

    // Calculate widget heights as if always 7 motors are visible
    // This ensures consistent widget sizes regardless of how many are actually visible
    if (widgetCount > 0) {
        const int TOTAL_SLOTS = 7; // Always calculate based on 7 slots
        QMargins m = right_->contentsMargins();
        const int spacing = right_->spacing();

        int avail = rightPanel_->height() - m.top() - m.bottom();
        
        // Calculate height based on 7 widgets, not actual count
        int hEach = (avail - spacing * (TOTAL_SLOTS - 1)) / TOTAL_SLOTS;
        hEach = qMax(60, hEach);  // Minimum height to ensure widgets aren't too small

        // Only update if height changed significantly (avoid micro-adjustments)
        if (qAbs(hEach - lastCalculatedHeight_) > 5) {
            lastCalculatedHeight_ = hEach;
            
            // Apply the calculated height to all slot widgets
            for (int i = 0; i < right_->count() - 1; ++i) {
                if (auto* w = right_->itemAt(i)->widget()) {
                    w->setFixedHeight(hEach);
                }
            }
        }
    }
}

void HomePage::updateMotorData(const QString& motorName, double timestamp, double position, double torque) {
    if (livePlot_) {
        livePlot_->addDataPoint(motorName, timestamp, position);
    }
    
    // Forward telemetry data to action buttons for logging
    if (actions_) {
        actions_->logMotorData(motorName, timestamp, position, torque);
    }
}

void HomePage::onMotorEnabled(const QString& motorName, bool enabled) {
    // When a motor is disabled in Settings, remove it from the live plot
    if (livePlot_) {
        if (enabled) {
            // Re-add motor with its color
            QColor motorColor = MotorColorScheme::colorForMotor(motorName);
            livePlot_->addMotor(motorName, motorColor);
        } else {
            livePlot_->removeMotor(motorName);
        }
    }
}

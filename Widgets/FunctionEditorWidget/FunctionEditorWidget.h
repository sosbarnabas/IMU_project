#pragma once

#include <QWidget>
#include <QPoint>
#include <QVector>
#include <QHash>
#include <memory>

class QCustomPlot;
class QSpinBox;
class QPushButton;
class QLabel;
class QComboBox;
class QButtonGroup;

class FunctionEditorWidget : public QWidget {
    Q_OBJECT

public:
    explicit FunctionEditorWidget(QWidget* parent = nullptr);
    ~FunctionEditorWidget() override = default;

    // Data management
    QVector<double> exportToArray360() const;
    void setControlPoints(const QVector<QPoint>& points);
    void resetToDefault();

    const QVector<QPoint>& controlPoints() const { return points_; }
    
    // Motor management
    void setAvailableMotors(const QHash<int, QString>& motors);
    int selectedSlot() const;
    int selectedMotorAddress() const;

signals:
    void arraySaved(const QVector<double>& values);
    void uploadRequested(int slot, int motorAddress, const QVector<double>& values);
    void loadRequested();
    void saveRequested();
    void resetRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void onAddPoint();
    void onDeletePoint();
    void onRoundAll();
    void onSaveArray();
    void onUploadToMotor();
    void onXChanged(int value);
    void onYChanged(int value);
    void onSlotButtonClicked(int slot);

private:
    // Constants
    static constexpr int MIN_X = 0;
    static constexpr int MAX_X = 360;
    static constexpr int MIN_Y = -127;
    static constexpr int MAX_Y = 127;
    static constexpr int POINT_RADIUS = 6;
    static constexpr double SELECTION_RADIUS = 12.0;

    // Initialization
    void initializeUI();
    void createPlot();
    void createControls();
    void connectSignals();

    QWidget* createControlPanel();

    // Point management
    void addPointAt(const QPoint& point);
    void removeSelectedPoint();
    void updateSelectedPoint(const QPoint& newPos);
    int findPointNear(const QPointF& pixelPos) const;
    QPoint clampPoint(const QPoint& point) const;
    void sortPoints();

    // UI updates
    void refreshUI();
    void updatePlot();
    void updateControls();
    void updateSelection();
    void setSelectedIndex(int index);

    // Mouse handling
    void handleMousePress(QMouseEvent *event);
    void handleMouseMove(QMouseEvent *event);
    void handleMouseRelease(QMouseEvent *event);
    void handleMouseDoubleClick(QMouseEvent *event);

    // Utilities
    QPointF pixelToCoord(const QPointF& pixel) const;
    void showPointTooltip(const QPoint& point, const QPoint& globalPos);
    void editPointDialog(int index);
    bool canMoveX(int index) const;
    bool canDelete(int index) const;

    // Data
    QVector<QPoint> points_;
    int selectedIndex_ = 0;

    // Drag state
    struct DragState {
        bool active = false;
        int index = -1;
    } dragState_;

    // UI elements
    QCustomPlot* plot_ = nullptr;
    QSpinBox* spinX_ = nullptr;
    QSpinBox* spinY_ = nullptr;
    QPushButton* addBtn_ = nullptr;
    QPushButton* deleteBtn_ = nullptr;
    QPushButton* roundBtn_ = nullptr;
    QPushButton* saveBtn_ = nullptr;
    QPushButton* uploadBtn_ = nullptr;
    QPushButton* loadBtn_ = nullptr;
    QPushButton* saveFileBtn_ = nullptr;
    QPushButton* resetBtn_ = nullptr;
    QLabel* startLabel_ = nullptr;
    QLabel* endLabel_ = nullptr;
    QButtonGroup* slotButtonGroup_ = nullptr;
    QComboBox* motorCombo_ = nullptr;
    
    // Motor/Slot selection
    QHash<int, QString> availableMotors_;
    int selectedSlot_ = 1;

    // Signal blocking
    bool blockSignals_ = false;
};

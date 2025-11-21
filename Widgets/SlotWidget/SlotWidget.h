#pragma once
#include <QWidget>
#include <memory>
#include <array>
#include <QString>

class QLabel;
class QPushButton;
class QCustomPlot;
class QGridLayout;
class QVBoxLayout;
class ToggleSwitch;

class SlotWidget : public QWidget {
    Q_OBJECT

public:
    explicit SlotWidget(QWidget* parent = nullptr);
    ~SlotWidget() override = default;

    void setValues(int position, int torque);
    void setSlotId(int id);
    void setTitle(const QString& title);
    void setGraph(const QVector<double>& values);
    void setMotorActive(bool active);
    void setFunctionEmpty(bool isEmpty);
    void setMotorColor(const QColor& color);
    QString title() const;
    int slotId() const;
    QColor motorColor() const;

signals:
    void addFunctionRequested();
    void createFunctionRequested();
    void selectSlotRequested();
    void slotChanged(int newSlotId);
    void plotVisibilityChanged(bool visible);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    // Layout structure
    struct LayoutComponents {
        QGridLayout* grid = nullptr;
        QVBoxLayout* centerBox = nullptr;
    };

    // UI elements grouped by function
    struct StatusWidgets {
        ToggleSwitch* toggle = nullptr;
        QLabel* enabledPill = nullptr;
        QWidget* colorIndicator = nullptr;
    };

    struct InfoWidgets {
        QLabel* slotId = nullptr;
        QLabel* slotLabel = nullptr;
        QLabel* positionValue = nullptr;
        QLabel* positionLabel = nullptr;
        QLabel* torqueValue = nullptr;
        QLabel* torqueLabel = nullptr;
    };

    struct ActionWidgets {
        QLabel* titleChip = nullptr;
        std::array<QPushButton*, 3> buttons{};
    };

    LayoutComponents layout_;
    StatusWidgets status_;
    InfoWidgets info_;
    ActionWidgets actions_;
    QCustomPlot* plot_ = nullptr;

    bool motorActive_ {false};
    bool functionEmpty_ {true};
    int currentSlotId_ {0};
    int lastPosition_ {0};
    int lastTorque_ {0};
    QColor motorColor_ {Qt::gray};
    int enabledPillRadius_ {14};
    int enabledPillPadH_ {12};
    int enabledPillPadV_ {4};
    QString cachedEnabledStyle_;
    QString currentTitle_;

    void initializeUI();
    void createStatusColumn();
    void createInfoColumn();
    void createActionColumn();
    void createPlotColumn();
    void configurePlot();

    void applyScaling(double widthRatio, double heightRatio);
    void scaleLayout(double widthRatio, double heightRatio);
    void scaleWidgets(double widthRatio, double heightRatio);
    void scalePlot(double widthRatio, double heightRatio);

    void applyEnabledPillStyle();

    static QPushButton* createStyledButton(const QString& text, QWidget* parent);
    static QWidget* createVerticalContainer(QLabel* value, QLabel* label, QWidget* parent);
};

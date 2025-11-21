#pragma once

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QColor>
#include <QTimer>

class QCustomPlot;
class QCPGraph;

class LivePlotWidget : public QWidget {
    Q_OBJECT

public:
    explicit LivePlotWidget(QWidget* parent = nullptr);
    ~LivePlotWidget() override = default;

    // Motor management
    void addMotor(const QString& motorName, const QColor& color);
    void removeMotor(const QString& motorName);
    void clearMotors();

    // Data update (optimized with batching)
    void addDataPoint(const QString& motorName, double timestamp, double position);
    
    // Visibility control
    void setMotorVisible(const QString& motorName, bool visible);
    void setGlobalVisible(bool visible);
    
    // Color management
    QColor motorColor(const QString& motorName) const;
    
    // Configuration
    void setTimeWindow(double seconds);
    void setYAxisRange(double min, double max);
    void setAutoScaleY(bool enabled);
    void setUpdateRate(int msec);  // Control replot frequency

signals:
    void plotClicked();
    void motorGraphClicked(const QString& motorName);

private slots:
    void performScheduledReplot();

private:
    void setupPlot();
    void configureAxes();
    void updatePlotRange();
    void scheduleReplot();
    
    QCustomPlot* plot_ {nullptr};
    QTimer* replotTimer_ {nullptr};
    
    // Motor data storage
    struct MotorData {
        QCPGraph* graph {nullptr};
        QColor color;
        QVector<double> timeData;
        QVector<double> positionData;
        bool visible {true};
        bool dataChanged {false};  // Track if data needs replot
    };
    
    QMap<QString, MotorData> motors_;
    
    // Plot configuration
    double timeWindow_ {10.0};  // seconds
    bool autoScaleY_ {true};
    double yMin_ {0.0};
    double yMax_ {10000.0};
    int updateRateMsec_ {50};  // 20 FPS default
    bool replotScheduled_ {false};
    
    static constexpr int MAX_DATA_POINTS = 10000;
};

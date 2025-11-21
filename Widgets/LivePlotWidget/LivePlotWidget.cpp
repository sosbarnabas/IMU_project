#include "LivePlotWidget.h"
#include "qcustomplot.h"
#include <QVBoxLayout>
#include <QDateTime>
#include <QTimer>

LivePlotWidget::LivePlotWidget(QWidget* parent)
    : QWidget(parent)
{
    setupPlot();
    
    // Setup replot timer for optimized rendering
    replotTimer_ = new QTimer(this);
    replotTimer_->setSingleShot(true);
    connect(replotTimer_, &QTimer::timeout, this, &LivePlotWidget::performScheduledReplot);
}

void LivePlotWidget::setupPlot()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    
    plot_ = new QCustomPlot(this);
    layout->addWidget(plot_);
    
    // Set dark theme background
    plot_->setBackground(QColor("#1E1E1E"));
    plot_->axisRect()->setBackground(QColor("#252525"));
    
    // Performance optimizations
    plot_->setNoAntialiasingOnDrag(true);  // Disable AA during drag for performance
    plot_->setOpenGl(false);  // CPU rendering is often faster for simple plots
    
    configureAxes();
    
    // Enable interactions
    plot_->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    plot_->axisRect()->setRangeDrag(Qt::Horizontal);
    plot_->axisRect()->setRangeZoom(Qt::Horizontal);
    
    // Set initial size
    plot_->setMinimumHeight(300);
}

void LivePlotWidget::configureAxes()
{
    // X axis (time)
    plot_->xAxis->setLabel("Time (s)");
    plot_->xAxis->setLabelColor(QColor("#CCCCCC"));
    plot_->xAxis->setTickLabelColor(QColor("#AAAAAA"));
    plot_->xAxis->setBasePen(QPen(QColor("#555555"), 1));
    plot_->xAxis->setTickPen(QPen(QColor("#555555"), 1));
    plot_->xAxis->setSubTickPen(QPen(QColor("#444444"), 1));
    plot_->xAxis->grid()->setPen(QPen(QColor("#333333"), 1, Qt::DotLine));
    
    // Y axis (position)
    plot_->yAxis->setLabel("Position");
    plot_->yAxis->setLabelColor(QColor("#CCCCCC"));
    plot_->yAxis->setTickLabelColor(QColor("#AAAAAA"));
    plot_->yAxis->setBasePen(QPen(QColor("#555555"), 1));
    plot_->yAxis->setTickPen(QPen(QColor("#555555"), 1));
    plot_->yAxis->setSubTickPen(QPen(QColor("#444444"), 1));
    plot_->yAxis->grid()->setPen(QPen(QColor("#333333"), 1, Qt::DotLine));
    
    // Set initial ranges
    plot_->xAxis->setRange(0, timeWindow_);
    plot_->yAxis->setRange(yMin_, yMax_);
}

void LivePlotWidget::addMotor(const QString& motorName, const QColor& color)
{
    if (motors_.contains(motorName)) {
        return;  // Motor already exists
    }
    
    MotorData data;
    data.color = color;
    data.graph = plot_->addGraph();
    data.visible = true;
    data.dataChanged = false;
    
    // Configure graph appearance
    QPen pen(color, 2);
    data.graph->setPen(pen);
    data.graph->setName(motorName);
    data.graph->setLineStyle(QCPGraph::lsLine);
    data.graph->setVisible(true);
    
    // Performance: Reserve space for data
    data.timeData.reserve(1000);
    data.positionData.reserve(1000);
    
    motors_.insert(motorName, data);
    
    scheduleReplot();
}

void LivePlotWidget::removeMotor(const QString& motorName)
{
    auto it = motors_.find(motorName);
    if (it == motors_.end()) {
        return;
    }
    
    if (it->graph) {
        plot_->removeGraph(it->graph);
    }
    
    motors_.erase(it);
    scheduleReplot();
}

void LivePlotWidget::clearMotors()
{
    plot_->clearGraphs();
    motors_.clear();
    scheduleReplot();
}

void LivePlotWidget::addDataPoint(const QString& motorName, double timestamp, double position)
{
    auto it = motors_.find(motorName);
    if (it == motors_.end()) {
        return;  // Motor not registered
    }
    
    MotorData& data = it.value();
    
    // Add data point
    data.timeData.append(timestamp);
    data.positionData.append(position);
    
    // Limit data points to prevent memory issues
    if (data.timeData.size() > MAX_DATA_POINTS) {
        const int removeCount = data.timeData.size() - MAX_DATA_POINTS;
        data.timeData.remove(0, removeCount);
        data.positionData.remove(0, removeCount);
    }
    
    // Mark data as changed but don't update graph immediately
    data.dataChanged = true;
    
    // Schedule a replot instead of reploting immediately
    scheduleReplot();
}

void LivePlotWidget::setMotorVisible(const QString& motorName, bool visible)
{
    auto it = motors_.find(motorName);
    if (it == motors_.end()) {
        return;
    }
    
    if (it->visible == visible) {
        return;  // No change
    }
    
    it->visible = visible;
    if (it->graph) {
        it->graph->setVisible(visible);
    }
    
    scheduleReplot();
}

void LivePlotWidget::setGlobalVisible(bool visible)
{
    setVisible(visible);
}

QColor LivePlotWidget::motorColor(const QString& motorName) const
{
    auto it = motors_.find(motorName);
    if (it != motors_.end()) {
        return it->color;
    }
    return QColor();
}

void LivePlotWidget::setTimeWindow(double seconds)
{
    if (timeWindow_ == seconds) {
        return;  // No change
    }
    
    timeWindow_ = seconds;
    updatePlotRange();
    scheduleReplot();
}

void LivePlotWidget::setYAxisRange(double min, double max)
{
    autoScaleY_ = false;
    yMin_ = min;
    yMax_ = max;
    plot_->yAxis->setRange(yMin_, yMax_);
    scheduleReplot();
}

void LivePlotWidget::setAutoScaleY(bool enabled)
{
    autoScaleY_ = enabled;
    if (enabled) {
        updatePlotRange();
        scheduleReplot();
    }
}

void LivePlotWidget::setUpdateRate(int msec)
{
    updateRateMsec_ = qMax(16, msec);  // Minimum 16ms (60 FPS)
}

void LivePlotWidget::scheduleReplot()
{
    if (replotScheduled_) {
        return;  // Already scheduled
    }
    
    replotScheduled_ = true;
    replotTimer_->start(updateRateMsec_);
}

void LivePlotWidget::performScheduledReplot()
{
    replotScheduled_ = false;
    
    // Update all graphs that have changed data
    bool anyDataChanged = false;
    for (auto& data : motors_) {
        if (data.dataChanged && data.graph) {
            data.graph->setData(data.timeData, data.positionData);
            data.dataChanged = false;
            anyDataChanged = true;
        }
    }
    
    if (anyDataChanged) {
        updatePlotRange();
    }
    
    plot_->replot();
}

void LivePlotWidget::updatePlotRange()
{
    if (motors_.isEmpty()) {
        return;
    }
    
    // Find the latest timestamp across all motors
    double maxTime = 0.0;
    for (const auto& data : motors_) {
        if (!data.timeData.isEmpty()) {
            maxTime = qMax(maxTime, data.timeData.last());
        }
    }
    
    // Set X range to show last timeWindow_ seconds
    double minTime = qMax(0.0, maxTime - timeWindow_);
    plot_->xAxis->setRange(minTime, maxTime);
    
    // Auto-scale Y axis if enabled
    if (autoScaleY_) {
        double minY = std::numeric_limits<double>::max();
        double maxY = std::numeric_limits<double>::lowest();
        bool hasData = false;
        
        for (const auto& data : motors_) {
            if (!data.visible || data.timeData.isEmpty()) {
                continue;
            }
            
            // Only consider points within the visible time range
            for (int i = 0; i < data.timeData.size(); ++i) {
                if (data.timeData[i] >= minTime && data.timeData[i] <= maxTime) {
                    minY = qMin(minY, data.positionData[i]);
                    maxY = qMax(maxY, data.positionData[i]);
                    hasData = true;
                }
            }
        }
        
        if (hasData) {
            // Add 5% padding
            double padding = (maxY - minY) * 0.05;
            plot_->yAxis->setRange(minY - padding, maxY + padding);
        }
    }
}

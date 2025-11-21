#include "SlotWidget.h"
#include "ToggleSwitch.h"
#include "Constants.h"
#include "StyleHelper.h"
#include "qcustomplot.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <array>

using namespace UI;

// Simple color indicator widget
class ColorIndicator : public QWidget {
public:
    explicit ColorIndicator(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(16, 16);
    }
    
    void setColor(const QColor& color) {
        color_ = color;
        update();
    }
    
    QColor color() const { return color_; }
    
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color_);
        painter.drawEllipse(rect());
    }
    
private:
    QColor color_ {Qt::gray};
};

SlotWidget::SlotWidget(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);
    initializeUI();
    setMotorActive(false);
}

void SlotWidget::initializeUI() {
    layout_.grid = new QGridLayout(this);
    layout_.grid->setContentsMargins(Layout::MARGIN, Layout::MARGIN, Layout::MARGIN, Layout::MARGIN);
    layout_.grid->setHorizontalSpacing(Layout::SPACING_H);
    layout_.grid->setVerticalSpacing(Layout::SPACING_V);

    createStatusColumn();
    createInfoColumn();
    createActionColumn();
    createPlotColumn();

    configurePlot();

    // Configure column stretching for center alignment
    for (int i = 0; i < 2; ++i) {
        layout_.grid->setColumnStretch(i, 0);
    }
    layout_.grid->setColumnStretch(2, 1);
    layout_.grid->setColumnStretch(3, 1);
}

void SlotWidget::createStatusColumn() {
    status_.toggle = new ToggleSwitch(this);
    status_.toggle->setChecked(true);  // Default: plot line visible
    status_.enabledPill = new QLabel("ENABLED", this);
    status_.enabledPill->setAlignment(Qt::AlignCenter);

    info_.slotId = new QLabel("-", this);
    info_.slotLabel = new QLabel("Active Slot", this);
    
    // Create color indicator
    status_.colorIndicator = new ColorIndicator(this);

    auto* slotContainer = createVerticalContainer(info_.slotId, info_.slotLabel, this);
    
    // Create a horizontal layout for slotContainer and color indicator
    auto* slotRow = new QWidget(this);
    auto* slotRowLayout = new QHBoxLayout(slotRow);
    slotRowLayout->setContentsMargins(0, 0, 0, 0);
    slotRowLayout->setSpacing(8);
    slotRowLayout->addWidget(slotContainer);
    //slotRowLayout->addWidget(status_.colorIndicator, 0, Qt::AlignVCenter);
    slotRowLayout->addStretch();

    layout_.grid->addWidget(status_.toggle, 0, 0, 1, 1, Qt::AlignLeft | Qt::AlignTop);
    layout_.grid->addWidget(status_.enabledPill, 0, 1, 1, 1, Qt::AlignLeft | Qt::AlignTop);
    layout_.grid->addWidget(slotRow, 1, 0, 1, 1, Qt::AlignLeft);
    layout_.grid->addWidget(status_.colorIndicator, 2, 0, 1, 1, Qt::AlignLeft);
    
    // Connect toggle to emit signal for plot visibility (NOT motor enable/disable)
    connect(status_.toggle, &ToggleSwitch::toggled, this, &SlotWidget::plotVisibilityChanged);
}

void SlotWidget::createInfoColumn() {
    info_.positionValue = new QLabel("7523", this);
    info_.positionLabel = new QLabel("Position", this);
    info_.torqueValue = new QLabel("45", this);
    info_.torqueLabel = new QLabel("Torque", this);

    auto* posContainer = createVerticalContainer(info_.positionValue, info_.positionLabel, this);
    auto* torqueContainer = createVerticalContainer(info_.torqueValue, info_.torqueLabel, this);

    layout_.grid->addWidget(posContainer, 1, 1, 1, 1);
    layout_.grid->addWidget(torqueContainer, 2, 1, 1, 1);
}

void SlotWidget::createActionColumn() {
    auto* centerHost = new QWidget(this);
    centerHost->setStyleSheet("background:transparent;");

    layout_.centerBox = new QVBoxLayout(centerHost);
    layout_.centerBox->setContentsMargins(0, 0, 0, 0);
    layout_.centerBox->setSpacing(16);

    actions_.titleChip = new QLabel("E_Flex", this);
    actions_.titleChip->setAlignment(Qt::AlignCenter);

    const std::array<const char*, 3> buttonTexts = {
        "ADD FUNCTION", "CREATE FUNCTION", "SELECT SLOT"
    };

    layout_.centerBox->addWidget(actions_.titleChip, 0, Qt::AlignHCenter);
    layout_.centerBox->addSpacing(2);

    for (size_t i = 0; i < buttonTexts.size(); ++i) {
        actions_.buttons[i] = createStyledButton(buttonTexts[i], this);
        layout_.centerBox->addWidget(actions_.buttons[i], 0, Qt::AlignHCenter);
    }

    // Connect button signals
    if (actions_.buttons[0]) {
        connect(actions_.buttons[0], &QPushButton::clicked, this, &SlotWidget::addFunctionRequested);
    }
    if (actions_.buttons[1]) {
        connect(actions_.buttons[1], &QPushButton::clicked, this, &SlotWidget::createFunctionRequested);
    }
    if (actions_.buttons[2]) {
        connect(actions_.buttons[2], &QPushButton::clicked, this, &SlotWidget::selectSlotRequested);
    }

    layout_.centerBox->addStretch();
    layout_.grid->addWidget(centerHost, 0, 2, 4, 1);
}

void SlotWidget::createPlotColumn() {
    plot_ = new QCustomPlot(this);

    auto* plotContainer = new QWidget(this);
    plotContainer->setStyleSheet("background:transparent;");

    auto* vbox = new QVBoxLayout(plotContainer);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->addStretch();
    vbox->addWidget(plot_, 0, Qt::AlignTop | Qt::AlignRight);
    vbox->addStretch();

    layout_.grid->addWidget(plotContainer, 0, 3, 4, 1);
}

void SlotWidget::configurePlot() {
    // Set transparent background
    plot_->setBackground(Qt::transparent);
    plot_->axisRect()->setBackground(Qt::transparent);

    const QColor lineColor(Colors::CHART_LINE);

    // Configure axes - only bottom and left visible
    auto configureAxis = [&](QCPAxis* axis) {
        axis->setVisible(true);
        axis->setBasePen(QPen(lineColor, 1));
        axis->setTickPen(Qt::NoPen);
        axis->setSubTickPen(Qt::NoPen);
        axis->setTickLabels(false);
        axis->setTicks(false);
        axis->grid()->setVisible(false);
    };

    configureAxis(plot_->xAxis);
    configureAxis(plot_->yAxis);

    plot_->xAxis2->setVisible(false);
    plot_->yAxis2->setVisible(false);

    // Set margins
    plot_->plotLayout()->setMargins(QMargins(0, 0, 0, 0));
    plot_->axisRect()->setAutoMargins(QCP::msNone);
    const QMargins plotMargins(20, 20, 10, 10);
    plot_->axisRect()->setMinimumMargins(plotMargins);
    plot_->axisRect()->setMargins(plotMargins);

    // Set ranges
    plot_->xAxis->setRange(-10, 360);
    plot_->yAxis->setRange(-10, 127);

    // Add data
    auto* graph = plot_->addGraph();
    const QVector<double> x{0, 60, 60, 260, 260, 360};
    const QVector<double> y{80, 80, 80, 20, 20, 20};
    graph->setData(x, y);

    QPen linePen(lineColor);
    linePen.setColor("#2DA8FF");
    linePen.setWidth(2);
    graph->setPen(linePen);
    graph->setLineStyle(QCPGraph::lsLine);

    plot_->setNotAntialiasedElements(QCP::aeNone);
    plot_->replot();
}

void SlotWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(QColor(Colors::CARD_BG));
    painter.setPen(QPen(QColor(Colors::CARD_BORDER), 1));
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), Layout::CARD_RADIUS, Layout::CARD_RADIUS);
}

void SlotWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);

    const double widthRatio = static_cast<double>(width()) / Layout::BASE_WIDTH;
    const double heightRatio = static_cast<double>(height()) / Layout::BASE_HEIGHT;

    applyScaling(widthRatio, heightRatio);
}

void SlotWidget::applyScaling(double widthRatio, double heightRatio) {
    scaleLayout(widthRatio, heightRatio);
    scaleWidgets(widthRatio, heightRatio);
    scalePlot(widthRatio, heightRatio);
}

void SlotWidget::scaleLayout(double widthRatio, double heightRatio) {
    const int margin = StyleHelper::scaled(Layout::MARGIN, widthRatio);
    const int vMargin = StyleHelper::scaled(Layout::MARGIN, heightRatio);
    layout_.grid->setContentsMargins(margin, vMargin, margin, vMargin);
    layout_.grid->setHorizontalSpacing(StyleHelper::scaled(Layout::SPACING_H, widthRatio));
    layout_.grid->setVerticalSpacing(StyleHelper::scaled(Layout::SPACING_V, heightRatio));

    if (layout_.centerBox) {
        layout_.centerBox->setSpacing(StyleHelper::scaled(16, heightRatio));
    }
}

void SlotWidget::scaleWidgets(double widthRatio, double heightRatio) {
    // Scale toggle
    status_.toggle->setFixedSize(
        StyleHelper::scaled(Widgets::TOGGLE_WIDTH, widthRatio),
        StyleHelper::scaled(Widgets::TOGGLE_HEIGHT, heightRatio)
        );

    // Scale enabled pill
    enabledPillRadius_ = StyleHelper::scaled(14, heightRatio);
    enabledPillPadH_ = StyleHelper::scaled(12, widthRatio);
    enabledPillPadV_ = StyleHelper::scaled(4, heightRatio);
    StyleHelper::setScaledFont(status_.enabledPill, 10, heightRatio);
    status_.enabledPill->setMinimumWidth(StyleHelper::scaled(140, widthRatio));
    status_.enabledPill->setFixedHeight(StyleHelper::scaled(28, heightRatio));
    applyEnabledPillStyle();
    // Scale info labels
    auto styleInfoLabel = [&](QLabel* valueLabel, QLabel* textLabel) {
        StyleHelper::setScaledFont(valueLabel, 35, heightRatio, true);
        valueLabel->setStyleSheet(QString("color: %1; background: transparent;").arg(Colors::TEXT));

        StyleHelper::setScaledFont(textLabel, 8, heightRatio, false);
        textLabel->setStyleSheet(QString("color: %1; background: transparent;").arg(Colors::TEXT_MUTED));
    };

    styleInfoLabel(info_.slotId, info_.slotLabel);
    styleInfoLabel(info_.positionValue, info_.positionLabel);
    styleInfoLabel(info_.torqueValue, info_.torqueLabel);

    // Scale title chip
    const int pillRadius = StyleHelper::scaled(14, heightRatio);
    actions_.titleChip->setStyleSheet(
        StyleHelper::labelChipStyle(pillRadius,
                                    StyleHelper::scaled(18, widthRatio),
                                    StyleHelper::scaled(4, heightRatio))
        );
    StyleHelper::setScaledFont(actions_.titleChip, 12, heightRatio, true);

    // Scale buttons
    const int buttonWidth = StyleHelper::scaled(Widgets::BUTTON_WIDTH, widthRatio);
    const int buttonHeight = StyleHelper::scaled(Widgets::BUTTON_HEIGHT, heightRatio);
    const int buttonRadius = StyleHelper::scaled(18, heightRatio);

    for (auto* button : actions_.buttons) {
        button->setFixedSize(buttonWidth, buttonHeight);
        button->setStyleSheet(StyleHelper::pillButtonStyle(buttonRadius));
        StyleHelper::setScaledFont(button, 10, heightRatio, true);
    }
}

void SlotWidget::scalePlot(double widthRatio, double heightRatio) {
    const int plotWidth = StyleHelper::scaled(Widgets::PLOT_WIDTH, widthRatio);
    const int plotHeight = StyleHelper::scaled(Widgets::PLOT_HEIGHT, heightRatio);

    plot_->setMinimumSize(plotWidth, plotHeight);
    plot_->setMaximumSize(plotWidth, plotHeight);

    // Scale axis fonts
    QFont tickFont("Inter", std::max(7, StyleHelper::scaled(9, heightRatio)));
    plot_->xAxis->setTickLabelFont(tickFont);
    plot_->yAxis->setTickLabelFont(tickFont);

    // Scale line widths
    const qreal axisWidth = std::max(1.0, 1.0 * heightRatio);
    QPen axisPen(Qt::black);
    axisPen.setWidthF(axisWidth);

    plot_->xAxis->setBasePen(axisPen);
    plot_->yAxis->setBasePen(axisPen);
    plot_->xAxis->setTickPen(axisPen);
    plot_->yAxis->setTickPen(axisPen);

    if (plot_->graphCount() > 0) {
        QPen linePen = plot_->graph(0)->pen();
        linePen.setWidthF(std::max(1.0, 2.0 * heightRatio));
        plot_->graph(0)->setPen(linePen);
    }

    plot_->replot();
}

void SlotWidget::setValues(int position, int torque) {
    // Avoid unnecessary updates if values haven't changed
    if (lastPosition_ == position && lastTorque_ == torque) {
        return;
    }
    
    lastPosition_ = position;
    lastTorque_ = torque;
    
    info_.positionValue->setText(QString::number(position));
    info_.torqueValue->setText(QString::number(torque));
}

void SlotWidget::setSlotId(int id) {
    if (currentSlotId_ != id) {
        currentSlotId_ = id;
        info_.slotId->setText(QString::number(id));
        emit slotChanged(id);
    }
}

void SlotWidget::setTitle(const QString& title) {
    // Avoid unnecessary updates
    if (currentTitle_ == title) {
        return;
    }
    
    currentTitle_ = title;
    actions_.titleChip->setText(title);
}

void SlotWidget::setGraph(const QVector<double>& values) {
    const QColor lineColor(Colors::CHART_LINE);
    if (!plot_)
        return;

    plot_->clearGraphs();

    if (values.isEmpty()) {
        functionEmpty_ = true;
        // Show empty state - just axes, no graph
        plot_->replot();
        return;
    }

    functionEmpty_ = false;
    auto* graph = plot_->addGraph();
    QVector<double> x(values.size());
    for (int i = 0; i < values.size(); ++i) {
        x[i] = i;
    }
    graph->setData(x, values);

    QPen linePen(lineColor);
    linePen.setColor("#2DA8FF");
    linePen.setWidth(2);
    graph->setPen(linePen);
    graph->setLineStyle(QCPGraph::lsLine);

    plot_->setNotAntialiasedElements(QCP::aeNone);
    plot_->replot();
}

void SlotWidget::setFunctionEmpty(bool isEmpty) {
    functionEmpty_ = isEmpty;
    if (isEmpty) {
        // Clear the plot and show empty state
        setGraph(QVector<double>());
    }
    
    // Update button states based on function existence
    // "Add Function" and "Create Function" are always enabled
    // "Select Slot" is only useful when functions exist, but can still be enabled
    // We'll keep all buttons enabled for now, as even without a function,
    // users can still add/create functions or select slots
}

QPushButton* SlotWidget::createStyledButton(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setCursor(Qt::PointingHandCursor);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return button;
}

QWidget* SlotWidget::createVerticalContainer(QLabel* value, QLabel* label, QWidget* parent) {
    auto* container = new QWidget(parent);
    container->setStyleSheet("background:transparent;");

    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(value, 0, Qt::AlignLeft);
    layout->addWidget(label, 0, Qt::AlignLeft);

    return container;
}

QString SlotWidget::title() const {
    return actions_.titleChip ? actions_.titleChip->text() : QString();
}

int SlotWidget::slotId() const {
    return currentSlotId_;
}

void SlotWidget::setMotorActive(bool active) {
    motorActive_ = active;
    // NOTE: Toggle switch is NOT connected to motor enabled/disabled state
    // It only controls plot line visibility
    if (status_.enabledPill) {
        status_.enabledPill->setText(active ? tr("ENABLED") : tr("DISABLED"));
    }
    applyEnabledPillStyle();
}

void SlotWidget::applyEnabledPillStyle() {
    if (!status_.enabledPill) {
        return;
    }

    const QString color = motorActive_ ? Colors::SUCCESS : Colors::DANGER;
    const QString newStyle = QStringLiteral(
        "QLabel { background: %1; color: white; border-radius: %2px; padding: %3px %4px; }")
                                            .arg(color)
                                            .arg(enabledPillRadius_)
                                            .arg(enabledPillPadV_)
                                            .arg(enabledPillPadH_);
    
    // Cache and avoid redundant stylesheet updates
    if (cachedEnabledStyle_ != newStyle) {
        cachedEnabledStyle_ = newStyle;
        status_.enabledPill->setStyleSheet(newStyle);
    }
}

void SlotWidget::setMotorColor(const QColor& color) {
    motorColor_ = color;
    if (status_.colorIndicator) {
        static_cast<ColorIndicator*>(status_.colorIndicator)->setColor(color);
    }
}

QColor SlotWidget::motorColor() const {
    return motorColor_;
}

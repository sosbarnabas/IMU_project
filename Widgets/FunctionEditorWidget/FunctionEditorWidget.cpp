#include "FunctionEditorWidget.h"
#include "qcustomplot.h"
#include "../UIUtilities/Constants.h"

#include <QBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QToolTip>
#include <QComboBox>
#include <QButtonGroup>
#include <QMessageBox>
#include <QtMath>
#include <algorithm>
#include <cmath>

FunctionEditorWidget::FunctionEditorWidget(QWidget* parent)
    : QWidget(parent)
    , points_{{MIN_X, 0}, {MAX_X, 0}} {

    initializeUI();
    connectSignals();
    refreshUI();
}

// =============================================================================
// Initialization
// =============================================================================

void FunctionEditorWidget::initializeUI() {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(16);

    createPlot();
    createControls();

    layout->addWidget(plot_, 3);
    layout->addWidget(createControlPanel(), 1);
}

void FunctionEditorWidget::createPlot() {
    plot_ = new QCustomPlot(this);
    plot_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    plot_->setMouseTracking(true);
    plot_->setInteractions(QCP::iNone);

    // Dark theme
    const QColor bgColor("#1E1E1E");
    const QColor gridColor("#333333");
    const QColor axisColor("#555555");
    const QColor lineColor("#2DA8FF");

    plot_->setBackground(bgColor);
    plot_->axisRect()->setBackground(bgColor);
    plot_->legend->setVisible(false);

    // Setup axes
    plot_->xAxis->setRange(MIN_X, MAX_X);
    plot_->yAxis->setRange(MIN_Y, MAX_Y);

    // Axis styling
    QPen axisPen(axisColor);
    plot_->xAxis->setBasePen(axisPen);
    plot_->yAxis->setBasePen(axisPen);
    plot_->xAxis->setTickPen(axisPen);
    plot_->yAxis->setTickPen(axisPen);
    plot_->xAxis->setSubTickPen(axisPen);
    plot_->yAxis->setSubTickPen(axisPen);
    plot_->xAxis->setTickLabelColor(Qt::lightGray);
    plot_->yAxis->setTickLabelColor(Qt::lightGray);

    // Grid
    QPen gridPen(gridColor);
    plot_->xAxis->grid()->setPen(gridPen);
    plot_->yAxis->grid()->setPen(gridPen);
    plot_->xAxis->grid()->setZeroLinePen(QPen(axisColor));
    plot_->yAxis->grid()->setZeroLinePen(QPen(axisColor));
    plot_->xAxis->grid()->setSubGridVisible(false);
    plot_->yAxis->grid()->setSubGridVisible(false);

    // Custom ticks
    auto xTicker = QSharedPointer<QCPAxisTickerText>::create();
    xTicker->addTick(0, "0");
    xTicker->addTick(UI::SpringFunction::POINTS_COUNT / 2, QString::number(UI::SpringFunction::POINTS_COUNT / 2));
    xTicker->addTick(UI::SpringFunction::POINTS_COUNT, QString::number(UI::SpringFunction::POINTS_COUNT));
    plot_->xAxis->setTicker(xTicker);

    auto yTicker = QSharedPointer<QCPAxisTickerText>::create();
    yTicker->addTick(-127, "-127");
    yTicker->addTick(0, "0");
    yTicker->addTick(127, "127");
    plot_->yAxis->setTicker(yTicker);

    // Create graph for the line and normal points
    plot_->addGraph();
    auto* mainGraph = plot_->graph(0);
    mainGraph->setPen(QPen(lineColor, 2));
    mainGraph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc,
                                           QPen(lineColor),
                                           QBrush(Qt::NoBrush),
                                           POINT_RADIUS));

    // Create a second graph for the selected point only (will be on top)
    plot_->addGraph();
    auto* selectionGraph = plot_->graph(1);
    selectionGraph->setPen(Qt::NoPen); // Nincs összekötő vonal a kijelölt pontokhoz
    selectionGraph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc,
                                           QPen(QColor(255, 220, 0)), // sárga keret
                                           QBrush(QColor(255, 220, 0)), // sárga kitöltés
                                           POINT_RADIUS));
}

void FunctionEditorWidget::createControls() {
    spinX_ = new QSpinBox;
    spinX_->setRange(MIN_X, MAX_X);
    spinX_->setAccelerated(true);
    spinX_->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    spinX_->setKeyboardTracking(false);

    spinY_ = new QSpinBox;
    spinY_->setRange(MIN_Y, MAX_Y);
    spinY_->setAccelerated(true);
    spinY_->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    spinY_->setKeyboardTracking(false);

    addBtn_ = new QPushButton(tr("Add Point"));
    deleteBtn_ = new QPushButton(tr("Delete Point"));


    for (auto* btn : {addBtn_, deleteBtn_}) {
        btn->setCursor(Qt::PointingHandCursor);
    }

}

QWidget* FunctionEditorWidget::createControlPanel() {
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // Title
    auto* title = new QLabel(tr("Function Editor"));
    QFont font = title->font();
    font.setBold(true);
    font.setPointSize(font.pointSize() + 2);
    title->setFont(font);
    title->setStyleSheet("color:#f0f0f0;");
    layout->addWidget(title);

    // Coordinate controls
    auto* formWidget = new QWidget;
    auto* formLayout = new QFormLayout(formWidget);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->addRow(tr("X"), spinX_);
    formLayout->addRow(tr("Y"), spinY_);
    layout->addWidget(formWidget);

    // Buttons
    layout->addWidget(addBtn_);
    layout->addWidget(deleteBtn_);
    layout->addWidget(roundBtn_);
    layout->addWidget(saveBtn_);

    // File operations
    auto* fileLabel = new QLabel("File Operations:", this);
    fileLabel->setStyleSheet("font-weight: bold; margin-top: 15px;");
    layout->addWidget(fileLabel);
    
    loadBtn_ = new QPushButton("Load from File", this);
    loadBtn_->setMinimumHeight(40);
    layout->addWidget(loadBtn_);
    
    saveFileBtn_ = new QPushButton("Save to File", this);
    saveFileBtn_->setMinimumHeight(40);
    layout->addWidget(saveFileBtn_);
    
    resetBtn_ = new QPushButton("Reset", this);
    resetBtn_->setMinimumHeight(40);
    layout->addWidget(resetBtn_);

    // Slot selector (0-7 buttons)
    auto* slotLabel = new QLabel("Target Slot:", this);
    slotLabel->setStyleSheet("font-weight: bold; margin-top: 15px;");
    layout->addWidget(slotLabel);
    
    slotButtonGroup_ = new QButtonGroup(this);
    slotButtonGroup_->setExclusive(true);
    
    auto* slotLayout = new QGridLayout();
    slotLayout->setSpacing(5);
    
    for (int i = 0; i < 8; ++i) {
        QPushButton* btn = new QPushButton(QString::number(i), this);
        btn->setCheckable(true);
        btn->setMinimumSize(40, 40);
        btn->setMaximumSize(40, 40);
        btn->setStyleSheet(
            "QPushButton {"
            "   border: 2px solid #cccccc;"
            "   border-radius: 4px;"
            "   background-color: white;"
            "   color: black;"
            "}"
            "QPushButton:checked {"
            "   background-color: #2196F3;"
            "   color: black;"
            "   border-color: #2196F3;"
            "}"
            "QPushButton:hover {"
            "   border-color: #2196F3;"
            "}"
        );
        
        slotButtonGroup_->addButton(btn, i);
        slotLayout->addWidget(btn, i / 4, i % 4);
        
        if (i == 1) {
            btn->setChecked(true);
        }
    }
    layout->addLayout(slotLayout);
    
    // Motor selector
    auto* motorLabel = new QLabel("Target Motor:", this);
    motorLabel->setStyleSheet("font-weight: bold; margin-top: 10px;");
    layout->addWidget(motorLabel);
    
    motorCombo_ = new QComboBox(this);
    motorCombo_->setMinimumHeight(40);
    layout->addWidget(motorCombo_);

    // Upload to Motor button
    uploadBtn_ = new QPushButton("Upload to Motor", this);
    uploadBtn_->setMinimumHeight(50);
    uploadBtn_->setStyleSheet(
        "QPushButton {"
        "   background-color: #4CAF50;"
        "   color: white;"
        "   font-weight: bold;"
        "   border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #45a049;"
        "}"
    );
    layout->addWidget(uploadBtn_);

    layout->addStretch();

    return panel;
}

void FunctionEditorWidget::connectSignals() {
    // Plot events
    connect(plot_, &QCustomPlot::mousePress,
            this, &FunctionEditorWidget::handleMousePress);
    connect(plot_, &QCustomPlot::mouseMove,
            this, &FunctionEditorWidget::handleMouseMove);
    connect(plot_, &QCustomPlot::mouseRelease,
            this, &FunctionEditorWidget::handleMouseRelease);
    connect(plot_, &QCustomPlot::mouseDoubleClick,
            this, &FunctionEditorWidget::handleMouseDoubleClick);

    // Control events
    connect(addBtn_, &QPushButton::clicked,
            this, &FunctionEditorWidget::onAddPoint);
    connect(deleteBtn_, &QPushButton::clicked,
            this, &FunctionEditorWidget::onDeletePoint);
    connect(roundBtn_, &QPushButton::clicked,
            this, &FunctionEditorWidget::onRoundAll);
    connect(saveBtn_, &QPushButton::clicked,
            this, &FunctionEditorWidget::onSaveArray);
    connect(loadBtn_, &QPushButton::clicked,
            this, &FunctionEditorWidget::loadRequested);
    connect(saveFileBtn_, &QPushButton::clicked,
            this, &FunctionEditorWidget::saveRequested);
    connect(resetBtn_, &QPushButton::clicked,
            this, &FunctionEditorWidget::resetRequested);
    connect(uploadBtn_, &QPushButton::clicked,
            this, &FunctionEditorWidget::onUploadToMotor);
    connect(slotButtonGroup_, &QButtonGroup::idClicked,
              this, &FunctionEditorWidget::onSlotButtonClicked);
    connect(spinX_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &FunctionEditorWidget::onXChanged);
    connect(spinY_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &FunctionEditorWidget::onYChanged);
}

// =============================================================================
// Public API
// =============================================================================

QVector<double> FunctionEditorWidget::exportToArray360() const {
    QVector<double> result(MAX_X);

    for (int x = 0; x < MAX_X; ++x) {
        // Find surrounding points
        auto it = std::lower_bound(points_.begin(), points_.end(), QPoint(x, 0),
                                   [](const QPoint& a, const QPoint& b) {
                                       return a.x() < b.x();
                                   });

        if (it == points_.begin()) {
            result[x] = points_.first().y();
        } else if (it == points_.end()) {
            result[x] = points_.last().y();
        } else {
            // Linear interpolation
            const auto& p2 = *it;
            const auto& p1 = *(it - 1);

            if (p2.x() == p1.x()) {
                result[x] = p1.y();
            } else {
                const double t = double(x - p1.x()) / (p2.x() - p1.x());
                result[x] = qRound(p1.y() + t * (p2.y() - p1.y()));
            }
        }
    }

    return result;
}

void FunctionEditorWidget::setControlPoints(const QVector<QPoint>& points) {
    // Sanitize and sort input points
    QMap<int, int> uniquePoints;
    for (const auto& p : points) {
        uniquePoints[qBound(MIN_X, p.x(), MAX_X)] = qBound(MIN_Y, p.y(), MAX_Y);
    }

    // Ensure endpoints exist
    if (!uniquePoints.contains(MIN_X)) uniquePoints[MIN_X] = 0;
    if (!uniquePoints.contains(MAX_X)) uniquePoints[MAX_X] = 0;

    // Convert to vector
    points_.clear();
    points_.reserve(uniquePoints.size());
    for (auto it = uniquePoints.begin(); it != uniquePoints.end(); ++it) {
        points_.append(QPoint(it.key(), it.value()));
    }

    // Ensure endpoints are locked
    points_.first().setX(MIN_X);
    points_.last().setX(MAX_X);

    setSelectedIndex(0);
    dragState_ = {};
    refreshUI();
}

void FunctionEditorWidget::resetToDefault() {
    setControlPoints({{MIN_X, 0}, {MAX_X, 0}});
}

// =============================================================================
// Point Management
// =============================================================================

void FunctionEditorWidget::addPointAt(const QPoint& point) {
    const QPoint clamped = clampPoint(point);

    // Check if point already exists at this X
    auto it = std::find_if(points_.begin(), points_.end(),
                           [&](const QPoint& p) { return p.x() == clamped.x(); });

    if (it != points_.end()) {
        // Update existing point
        it->setY(clamped.y());
        setSelectedIndex(std::distance(points_.begin(), it));
    } else {
        // Insert new point
        auto insertIt = std::lower_bound(points_.begin(), points_.end(), clamped,
                                         [](const QPoint& a, const QPoint& b) {
                                             return a.x() < b.x();
                                         });
        const int index = std::distance(points_.begin(), insertIt);
        points_.insert(insertIt, clamped);
        setSelectedIndex(index);
    }

    refreshUI();
}

void FunctionEditorWidget::removeSelectedPoint() {
    if (!canDelete(selectedIndex_)) return;

    points_.removeAt(selectedIndex_);
    setSelectedIndex(qBound(0, selectedIndex_ - 1, points_.size() - 1));
    refreshUI();
}

void FunctionEditorWidget::updateSelectedPoint(const QPoint& newPos) {
    if (selectedIndex_ < 0 || selectedIndex_ >= points_.size()) return;

    QPoint clamped = clampPoint(newPos);

    // Constrain X movement for internal points
    if (canMoveX(selectedIndex_)) {
        const int minX = points_[selectedIndex_ - 1].x() + 1;
        const int maxX = points_[selectedIndex_ + 1].x() - 1;
        clamped.setX(qBound(minX, clamped.x(), maxX));
    } else {
        clamped.setX(points_[selectedIndex_].x());
    }

    points_[selectedIndex_] = clamped;
}

QPoint FunctionEditorWidget::clampPoint(const QPoint& point) const {
    return {qBound(MIN_X, point.x(), MAX_X),
            qBound(MIN_Y, point.y(), MAX_Y)};
}

void FunctionEditorWidget::sortPoints() {
    if (selectedIndex_ < 0) return;

    const QPoint selected = points_[selectedIndex_];
    std::sort(points_.begin(), points_.end(),
              [](const QPoint& a, const QPoint& b) { return a.x() < b.x(); });

    // Maintain selection
    auto it = std::find(points_.begin(), points_.end(), selected);
    if (it != points_.end()) {
        selectedIndex_ = std::distance(points_.begin(), it);
    }

    // Ensure endpoints
    points_.first().setX(MIN_X);
    points_.last().setX(MAX_X);
}

// =============================================================================
// UI Updates
// =============================================================================

void FunctionEditorWidget::refreshUI() {
    // Végezzük el a frissítéseket
    updatePlot();
    updateControls();
    updateSelection(); // Ez frissíti a kijelölt pontot
}

void FunctionEditorWidget::updatePlot() {
    if (!plot_) return;

    // Get both graphs
    auto* mainGraph = plot_->graph(0);
    if (!mainGraph) return;

    // Prepare data for main graph
    QVector<double> xs, ys;
    xs.reserve(points_.size());
    ys.reserve(points_.size());

    for (const auto& p : points_) {
        xs.append(p.x());
        ys.append(p.y());
    }

    // Update main graph with all points
    mainGraph->setData(xs, ys, true);

}

void FunctionEditorWidget::updateControls() {
    blockSignals_ = true;

    const bool hasSelection = selectedIndex_ >= 0 && selectedIndex_ < points_.size();

    if (hasSelection) {
        const auto& point = points_[selectedIndex_];
        spinX_->setValue(point.x());
        spinY_->setValue(point.y());
        spinX_->setEnabled(canMoveX(selectedIndex_));
        spinY_->setEnabled(true);
    } else {
        spinX_->setEnabled(false);
        spinY_->setEnabled(false);
    }

    deleteBtn_->setEnabled(hasSelection && canDelete(selectedIndex_));

    blockSignals_ = false;
}

void FunctionEditorWidget::updateSelection() {
    if (!plot_) return;

    // Get the selection graph
    auto* selectionGraph = plot_->graph(1);
    if (!selectionGraph) return;

    // Clear previous selection
    selectionGraph->data()->clear();

    // If we have a valid selection, add just that point to the selection graph
    if (selectedIndex_ >= 0 && selectedIndex_ < points_.size()) {
        const QPoint& selectedPoint = points_[selectedIndex_];
        selectionGraph->addData(selectedPoint.x(), selectedPoint.y());
    }

    // Refresh the plot
    plot_->replot();
}

void FunctionEditorWidget::setSelectedIndex(int index) {
    selectedIndex_ = qBound(-1, index, points_.size() - 1);
}

// =============================================================================
// Mouse Handling
// =============================================================================

void FunctionEditorWidget::handleMousePress(QMouseEvent* event) {
    if (!event) return;

    const int index = findPointNear(event->position());

    if (event->button() == Qt::LeftButton && index >= 0) {
        setSelectedIndex(index);
        dragState_ = {true, index};
        setFocus();
        refreshUI();
    } else if (event->button() == Qt::RightButton && index >= 0) {
        setSelectedIndex(index);
        refreshUI();

        QMenu menu(this);
        auto* editAction = menu.addAction(tr("Edit"));
        auto* deleteAction = menu.addAction(tr("Delete"));
        deleteAction->setEnabled(canDelete(index));

        const auto* chosen = menu.exec(event->globalPosition().toPoint());
        if (chosen == editAction) {
            editPointDialog(index);
        } else if (chosen == deleteAction) {
            removeSelectedPoint();
        }
    }
}

void FunctionEditorWidget::handleMouseMove(QMouseEvent* event) {
    if (!event) return;

    // Tooltip
    const int hoverIndex = findPointNear(event->position());
    if (hoverIndex >= 0) {
        const auto& pt = points_[hoverIndex];
        showPointTooltip(pt, event->globalPosition().toPoint());
    } else {
        QToolTip::hideText();
    }

    // Dragging
    if (dragState_.active && dragState_.index >= 0) {
        const QPointF coord = pixelToCoord(event->position());
        QPoint newPos(qRound(coord.x()), qRound(coord.y()));

        // Update point temporarily
        updateSelectedPoint(newPos);

        // Frissítsük a kiválasztott pontot is a sárga graph-ban
        if (plot_ && plot_->graph(1)) {
            auto* selectionGraph = plot_->graph(1);
            selectionGraph->data()->clear();
            selectionGraph->addData(points_[selectedIndex_].x(), points_[selectedIndex_].y());
        }

        // Frissítsük a fő grafikon adatait is, hogy a vonal is mozogjon
        if (plot_ && plot_->graph(0)) {
            auto* mainGraph = plot_->graph(0);
            QVector<double> xs, ys;
            xs.reserve(points_.size());
            ys.reserve(points_.size());

            for (const auto& p : points_) {
                xs.append(p.x());
                ys.append(p.y());
            }

            mainGraph->setData(xs, ys, true);
        }

        // Újrarajzoljuk a plotot azonnal a változások megjelenítéséhez
        if (plot_) {
            plot_->replot();
        }

        updateControls();
    }
}

void FunctionEditorWidget::handleMouseRelease(QMouseEvent* event) {
    Q_UNUSED(event)

    if (dragState_.active) {
        sortPoints();
        dragState_ = {};
        refreshUI();
    }
}

void FunctionEditorWidget::handleMouseDoubleClick(QMouseEvent* event) {
    if (!event || findPointNear(event->position()) >= 0) return;

    const QPointF coord = pixelToCoord(event->position());
    const int x = qRound(coord.x());

    // Don't add at endpoints
    if (x <= MIN_X || x >= MAX_X) return;

    // Check if point exists
    if (std::any_of(points_.begin(), points_.end(),
                    [x](const QPoint& p) { return p.x() == x; })) return;

    // Find insertion position and interpolate Y
    auto it = std::lower_bound(points_.begin(), points_.end(), QPoint(x, 0),
                               [](const QPoint& a, const QPoint& b) {
                                   return a.x() < b.x();
                               });

    if (it != points_.begin() && it != points_.end()) {
        const auto& left = *(it - 1);
        const auto& right = *it;
        const double t = double(x - left.x()) / (right.x() - left.x());
        const int y = qRound(left.y() + t * (right.y() - left.y()));
        addPointAt({x, y});
    }
}

// =============================================================================
// Utilities
// =============================================================================

int FunctionEditorWidget::findPointNear(const QPointF& pixelPos) const {
    if (!plot_) return -1;

    int closest = -1;
    double minDist = SELECTION_RADIUS;

    for (int i = 0; i < points_.size(); ++i) {
        const double px = plot_->xAxis->coordToPixel(points_[i].x());
        const double py = plot_->yAxis->coordToPixel(points_[i].y());
        const double dist = std::hypot(px - pixelPos.x(), py - pixelPos.y());

        if (dist < minDist) {
            minDist = dist;
            closest = i;
        }
    }

    return closest;
}

QPointF FunctionEditorWidget::pixelToCoord(const QPointF& pixel) const {
    if (!plot_) return {};
    return {plot_->xAxis->pixelToCoord(pixel.x()),
            plot_->yAxis->pixelToCoord(pixel.y())};
}

void FunctionEditorWidget::showPointTooltip(const QPoint& point, const QPoint& globalPos) {
    QToolTip::showText(globalPos, tr("X=%1, Y=%2").arg(point.x()).arg(point.y()), plot_);
}

bool FunctionEditorWidget::canMoveX(int index) const {
    return index > 0 && index < points_.size() - 1;
}

bool FunctionEditorWidget::canDelete(int index) const {
    return canMoveX(index);  // Same logic: can't delete endpoints
}

void FunctionEditorWidget::editPointDialog(int index) {
    if (index < 0 || index >= points_.size()) return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Edit Point"));
    dialog.setModal(true);

    auto* layout = new QFormLayout(&dialog);

    QSpinBox xSpin;
    xSpin.setRange(MIN_X, MAX_X);
    xSpin.setValue(points_[index].x());
    xSpin.setEnabled(canMoveX(index));

    QSpinBox ySpin;
    ySpin.setRange(MIN_Y, MAX_Y);
    ySpin.setValue(points_[index].y());

    layout->addRow(tr("X"), &xSpin);
    layout->addRow(tr("Y"), &ySpin);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(&buttons);

    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QPoint newPos(xSpin.value(), ySpin.value());
        points_[index] = clampPoint(newPos);

        if (canMoveX(index)) {
            const int minX = points_[index - 1].x() + 1;
            const int maxX = points_[index + 1].x() - 1;
            points_[index].setX(qBound(minX, newPos.x(), maxX));
        }

        sortPoints();
        refreshUI();
    }
}

// =============================================================================
// Event Handlers
// =============================================================================

void FunctionEditorWidget::keyPressEvent(QKeyEvent* event) {
    if (!event || selectedIndex_ < 0 || selectedIndex_ >= points_.size()) {
        QWidget::keyPressEvent(event);
        return;
    }

    QPoint current = points_[selectedIndex_];
    bool handled = false;

    switch (event->key()) {
    case Qt::Key_Up:
        current.setY(qMin(MAX_Y, current.y() + 1));
        handled = true;
        break;
    case Qt::Key_Down:
        current.setY(qMax(MIN_Y, current.y() - 1));
        handled = true;
        break;
    case Qt::Key_Left:
        if (canMoveX(selectedIndex_)) {
            const int minX = points_[selectedIndex_ - 1].x() + 1;
            current.setX(qMax(minX, current.x() - 1));
            handled = true;
        }
        break;
    case Qt::Key_Right:
        if (canMoveX(selectedIndex_)) {
            const int maxX = points_[selectedIndex_ + 1].x() - 1;
            current.setX(qMin(maxX, current.x() + 1));
            handled = true;
        }
        break;
    }

    if (handled) {
        updateSelectedPoint(current);
        sortPoints();
        refreshUI();
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void FunctionEditorWidget::leaveEvent(QEvent* event) {
    QToolTip::hideText();
    QWidget::leaveEvent(event);
}

// =============================================================================
// Slots
// =============================================================================

void FunctionEditorWidget::onAddPoint() {
    if (spinX_ && spinY_) {
        spinX_->interpretText();
        spinY_->interpretText();
        addPointAt({spinX_->value()+10, spinY_->value()});
    }
}

void FunctionEditorWidget::onDeletePoint() {
    removeSelectedPoint();
}

void FunctionEditorWidget::onRoundAll() {
    for (auto& pt : points_) {
        pt.setY(qRound(static_cast<qreal>(pt.y())));
    }
    refreshUI();
}

void FunctionEditorWidget::onSaveArray() {
    emit arraySaved(exportToArray360());
}

void FunctionEditorWidget::onXChanged(int value) {
    if (blockSignals_ || !canMoveX(selectedIndex_)) return;

    const int minX = points_[selectedIndex_ - 1].x() + 1;
    const int maxX = points_[selectedIndex_ + 1].x() - 1;
    const int clamped = qBound(minX, value, maxX);

    if (clamped != value) {
        blockSignals_ = true;
        spinX_->setValue(clamped);
        blockSignals_ = false;
    }

    points_[selectedIndex_].setX(clamped);
    sortPoints();
    updatePlot();
    updateSelection();
}

void FunctionEditorWidget::onYChanged(int value) {
    if (blockSignals_ || selectedIndex_ < 0 || selectedIndex_ >= points_.size()) return;

    const int clamped = qBound(MIN_Y, value, MAX_Y);
    if (clamped != value) {
        blockSignals_ = true;
        spinY_->setValue(clamped);
        blockSignals_ = false;
    }

    points_[selectedIndex_].setY(clamped);
    updatePlot();
    updateSelection();
}

void FunctionEditorWidget::onSlotButtonClicked(int slot) {
    selectedSlot_ = slot;
}

void FunctionEditorWidget::onUploadToMotor() {
    if (motorCombo_->count() == 0) {
        QMessageBox::warning(this, "No Motor Selected",
                            "Please select a target motor first.");
        return;
    }

    int motorAddr = motorCombo_->currentData().toInt();
    int slot = slotButtonGroup_->checkedId();
    QString motorName = motorCombo_->currentText();

    QString confirmMsg;
    if (motorAddr == -1) {
        // All motors selected
        confirmMsg = QString("Are you sure you want to upload this function to ALL MOTORS in slot %1?")
                             .arg(slot);
    } else {
        // Single motor selected
        confirmMsg = QString("Are you sure you want to upload this function to motor %1 (address %2) slot %3?")
                             .arg(motorName)
                             .arg(motorAddr)
                             .arg(slot);
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Upload", confirmMsg,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QVector<double> values = exportToArray360();
        emit uploadRequested(slot, motorAddr, values);
    }
}

// =============================================================================
// Motor Management
// =============================================================================

void FunctionEditorWidget::setAvailableMotors(const QHash<int, QString>& motors) {
    availableMotors_ = motors;
    
    motorCombo_->clear();
    
    // Add "All Motors" option with address -1
    motorCombo_->addItem("All Motors", -1);
    
    // Add separator (visual only, using disabled item)
    motorCombo_->insertSeparator(1);
    
    // Add individual motors
    for (auto it = motors.constBegin(); it != motors.constEnd(); ++it) {
        motorCombo_->addItem(it.value(), it.key());
    }
}

int FunctionEditorWidget::selectedSlot() const {
    return slotButtonGroup_->checkedId();
}

int FunctionEditorWidget::selectedMotorAddress() const {
    return motorCombo_->currentData().toInt();
}

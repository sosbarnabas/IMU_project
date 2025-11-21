#include "ToggleSwitch.h"
#include "Constants.h"
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>

ToggleSwitch::ToggleSwitch(QWidget* parent)
    : QWidget(parent) {
    setCursor(Qt::PointingHandCursor);
    setFixedSize(64, 32); // Default size
}

void ToggleSwitch::setChecked(bool checked) {
    if (checked_ != checked) {
        checked_ = checked;
        update();
        emit toggled(checked_);
    }
}

void ToggleSwitch::mousePressEvent(QMouseEvent*) {
    setChecked(!checked_);
}

void ToggleSwitch::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int radius = height() / 2;
    drawBackground(painter, radius);
    drawHandle(painter, radius);
}

void ToggleSwitch::drawBackground(QPainter& painter, int radius) const {
    painter.setPen(Qt::NoPen);
    painter.setBrush(checked_ ? QColor(UI::Colors::SUCCESS) : QColor("#3A3A40"));
    painter.drawRoundedRect(rect(), radius, radius);
}

void ToggleSwitch::drawHandle(QPainter& painter, int radius) const {
    const int diameter = static_cast<int>(height() * 0.78);
    const int constrainedDiameter = std::clamp(diameter, 2, height() - 4);

    const int y = (height() - constrainedDiameter) / 2;
    const int margin = (height() - constrainedDiameter) / 2;
    const int x = checked_ ? width() - constrainedDiameter - margin : margin;

    painter.setBrush(Qt::white);
    painter.drawEllipse(QRect(x, y, constrainedDiameter, constrainedDiameter));
}

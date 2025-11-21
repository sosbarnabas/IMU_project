#pragma once
#include <QAbstractButton>
#include <QSvgRenderer>

class IconTab : public QAbstractButton {
    Q_OBJECT
public:
    IconTab(const QString& svgPath, const QString& label, QWidget* parent=nullptr);
protected:
    void paintEvent(QPaintEvent*) override;
    QSize sizeHint() const override { return {80, 64}; }
private:
    QSvgRenderer svg_;
};

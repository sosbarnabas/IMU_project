#include "IconTab.h"
#include <QPainter>
#include <QAbstractButton>
#include <QSvgRenderer>

static const QColor COL_TEXT("#D9D9D9");
static const QColor COL_ACTIVE("#2DA8FF");

IconTab::IconTab(const QString& svgPath, const QString& label, QWidget* parent)
    : QAbstractButton(parent), svg_(svgPath)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setText(label);
}

void IconTab::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const bool active = isChecked();
    const QColor col = active ? QColor("#2DA8FF") : QColor("#D9D9D9");

    // ikon mérete logikai koordinátában
    const int iconW = 28, iconH = 28;
    const int ix = (width() - iconW) / 2;
    const int iy = 2;
    const QRect iconRect(ix, iy, iconW, iconH);

    // HiDPI-korrekt maszk
    const qreal dpr = devicePixelRatioF();
    const QSize pxSize(qRound(iconW * dpr), qRound(iconH * dpr));

    QImage mask(pxSize, QImage::Format_ARGB32_Premultiplied);
    mask.setDevicePixelRatio(dpr);
    mask.fill(Qt::transparent);
    {
        QPainter ip(&mask);
        ip.setRenderHint(QPainter::Antialiasing, true);
        svg_.render(&ip, QRectF(0, 0, iconW, iconH)); // logikai méret
    }

    // Színezett változat
    QImage colored(pxSize, QImage::Format_ARGB32_Premultiplied);
    colored.setDevicePixelRatio(dpr);
    colored.fill(col);
    {
        QPainter tp(&colored);
        tp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        tp.drawImage(QPoint(0,0), mask);
    }

    p.drawImage(iconRect.topLeft(), colored);

    // felirat
    p.setPen(col);
    QFont f = p.font(); f.setPointSize(10); p.setFont(f);
    p.drawText(QRect(0, iconRect.bottom() + 4, width(), 20),
               Qt::AlignHCenter | Qt::AlignTop, text());

    if (active)
        p.fillRect(QRect((width()-48)/2, height()-3, 48, 2), col);

}

#include "ControlRow.h"
#include "ToggleSwitch.h"
#include "StyleHelper.h"
#include "Constants.h"

#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QSignalBlocker>

using namespace UI;
ControlRow::ControlRow(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);
    initializeUI();
}

void ControlRow::initializeUI() {
    layout_.grid = new QGridLayout(this);
    layout_.grid->setContentsMargins(Layout::MARGIN, Layout::MARGIN, Layout::MARGIN, Layout::MARGIN);
    layout_.grid->setHorizontalSpacing(Layout::SPACING_H);
    layout_.grid->setVerticalSpacing(Layout::SPACING_V);

    w_.name = new QLabel("MOTOR_E_FLEX", this);
    w_.name->setStyleSheet(QString("color:%1; background:transparent;").arg(Colors::TEXT));

    w_.edit = new QLineEdit(this);
    w_.edit->setPlaceholderText("Írj ide…");
    w_.edit->setStyleSheet("background:#D9D9D9; border:0; padding:3px; color:#000;");

    w_.toggle = new ToggleSwitch(this);

    layout_.grid->addWidget(w_.name,   0, 0, 1, 1, Qt::AlignLeft | Qt::AlignVCenter);
    layout_.grid->addWidget(w_.edit,   1, 0, 1, 1);
    layout_.grid->addWidget(w_.toggle, 0, 1, 2, 1, Qt::AlignRight | Qt::AlignVCenter);

    layout_.grid->setColumnStretch(0, 1);
    layout_.grid->setColumnStretch(1, 0);

    connect(w_.toggle, &ToggleSwitch::toggled, this, &ControlRow::switchToggled);
    connect(w_.edit,   &QLineEdit::textChanged, this, &ControlRow::textChanged);

    applyScaling(
        double(width())  / ControlRowSize::BASE_WIDTH,
        double(height()) / ControlRowSize::BASE_HEIGHT
        );
}

void ControlRow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(QColor(Colors::CARD_BG));
    p.setPen(QPen(QColor(Colors::CARD_BORDER), 1));
    p.drawRoundedRect(rect().adjusted(1,1,-1,-1), Layout::CARD_RADIUS, Layout::CARD_RADIUS);
}

void ControlRow::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    applyScaling(double(width())/Layout::BASE_WIDTH, double(height())/Layout::BASE_HEIGHT);
}

void ControlRow::applyScaling(double wr, double hr) {
    scaleLayout(wr, hr);
    scaleWidgets(wr, hr);
}

void ControlRow::scaleLayout(double wr, double hr) {
    const int mH = StyleHelper::scaled(Layout::MARGIN, wr);
    const int mV = StyleHelper::scaled(Layout::MARGIN, hr);
    layout_.grid->setContentsMargins(mH, mV, mH, mV);
    layout_.grid->setHorizontalSpacing(StyleHelper::scaled(Layout::SPACING_H, wr));
    layout_.grid->setVerticalSpacing(StyleHelper::scaled(Layout::SPACING_V, hr));
}

void ControlRow::scaleWidgets(double wr, double hr) {
    StyleHelper::setScaledFont(w_.name, ControlRowSize::LABEL_FONT, hr, true);
    const int editH = StyleHelper::scaled(ControlRowSize::EDIT_HEIGHT, hr);
    w_.edit->setMinimumHeight(editH);
    w_.toggle->setFixedSize(
        StyleHelper::scaled(ControlRowSize::TOGGLE_WIDTH,  wr),
        StyleHelper::scaled(ControlRowSize::TOGGLE_HEIGHT, hr)
        );
}

void ControlRow::setLabelText(const QString& t) { w_.name->setText(t); }
QString ControlRow::labelText() const { return w_.name->text(); }
void ControlRow::setInputText(const QString& t) {
    if (w_.edit->text() == t)
        return;
    const QSignalBlocker blocker(w_.edit);
    w_.edit->setText(t);
}
QString ControlRow::inputText() const { return w_.edit->text(); }
void ControlRow::setPlaceholderText(const QString& t) { w_.edit->setPlaceholderText(t); }
void ControlRow::setSwitchChecked(bool on) { w_.toggle->setChecked(on); }
bool  ControlRow::isSwitchChecked() const  { return w_.toggle->isChecked(); }

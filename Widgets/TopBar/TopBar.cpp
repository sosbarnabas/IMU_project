#include "TopBar.h"
#include "IconTab.h"
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QPainter>
#include <QStyleOption>
#include <QSignalBlocker>

static const QColor BAR_BG(0x26,0x25,0x2B);   // #26252B

TopBar::TopBar(QWidget *parent) : QWidget(parent) {
    setFixedHeight(87);
    setObjectName("TopBar");

    // Ensure proper background handling without QSS
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAttribute(Qt::WA_StyledBackground, true);

    auto *h = new QHBoxLayout(this);
    h->setContentsMargins(24,10,24,10);
    h->setSpacing(0);

    group_ = new QButtonGroup(this);
    group_->setExclusive(true);

    auto add = [&](const QString& t, const QString& s){
        auto *tab = new IconTab(s, t, this);
        tab->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        group_->addButton(tab, group_->buttons().size());
        h->addWidget(tab);
    };
    h->addStretch();
    add("Home",     "icons/home.svg");
    add("Spring",   "icons/spring.svg");
    add("Arm\nSettings", "icons/arm.svg");
    add("Settings", "icons/settings.svg");
    h->addStretch();
    
    connect(group_, &QButtonGroup::idToggled, this,
            [this](int id, bool on){ if (on) emit tabChanged(id); });

    group_->button(3)->setChecked(true);
}

void TopBar::setCurrentIndex(int index) {
    if (!group_)
        return;

    if (auto* button = group_->button(index)) {
        const QSignalBlocker blocker(group_);
        button->setChecked(true);
    }
}

int TopBar::currentIndex() const {
    return group_ ? group_->checkedId() : -1;
}

void TopBar::paintEvent(QPaintEvent* e) {
    Q_UNUSED(e);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor("#26252B"));   // guaranteed background color
}

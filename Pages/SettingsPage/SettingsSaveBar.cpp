#include "SettingsSaveBar.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QString>

SettingsSaveBar::SettingsSaveBar(QWidget* parent)
    : QWidget(parent)
    , saveButton_(new QPushButton(tr("Save"), this)) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addStretch(1);
    layout->addWidget(saveButton_);

    saveButton_->setVisible(false);
}

void SettingsSaveBar::updatePending(bool dirty, int pendingCount) {
    if (!saveButton_)
        return;

    if (!dirty) {
        saveButton_->setVisible(false);
        saveButton_->setText(tr("Save"));
        return;
    }

    saveButton_->setVisible(true);
    saveButton_->setText(QStringLiteral("Save (%1 motors)").arg(pendingCount));
}

QPushButton* SettingsSaveBar::button() const {
    return saveButton_;
}

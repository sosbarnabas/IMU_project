#pragma once

#include <QWidget>

class QPushButton;

class SettingsSaveBar : public QWidget {
    Q_OBJECT
public:
    explicit SettingsSaveBar(QWidget* parent=nullptr);

    void updatePending(bool dirty, int pendingCount);
    QPushButton* button() const;

private:
    QPushButton* saveButton_{nullptr};
};

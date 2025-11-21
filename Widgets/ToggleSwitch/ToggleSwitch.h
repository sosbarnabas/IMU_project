#pragma once
#include <QWidget>
#include <memory>

class ToggleSwitch : public QWidget {
    Q_OBJECT

public:
    explicit ToggleSwitch(QWidget* parent = nullptr);
    ~ToggleSwitch() override = default;

    [[nodiscard]] bool isChecked() const noexcept { return checked_; }

signals:
    void toggled(bool checked);

public slots:
    void setChecked(bool checked);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    bool checked_ = true;

    void drawBackground(QPainter& painter, int radius) const;
    void drawHandle(QPainter& painter, int radius) const;
};

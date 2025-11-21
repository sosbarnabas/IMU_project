#pragma once
#include <QWidget>
#include <QString>

class QGridLayout;
class QLineEdit;
class QLabel;
class ToggleSwitch;

class ControlRow : public QWidget {
    Q_OBJECT
public:
    explicit ControlRow(QWidget* parent = nullptr);
    ~ControlRow() override = default;

    void setLabelText(const QString& text);
    QString labelText() const;

    void setInputText(const QString& text);
    QString inputText() const;
    void setPlaceholderText(const QString& text);

    void setSwitchChecked(bool on);
    bool isSwitchChecked() const;

signals:
    void switchToggled(bool on);
    void textChanged(const QString& text);

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    struct LayoutComponents {
        QGridLayout* grid = nullptr;
    } layout_;

    struct WidgetsComponents {
        QLabel* name = nullptr;
        QLineEdit* edit = nullptr;
        ToggleSwitch* toggle = nullptr;
    } w_;

    void initializeUI();
    void applyScaling(double wr, double hr);
    void scaleLayout(double wr, double hr);
    void scaleWidgets(double wr, double hr);
};

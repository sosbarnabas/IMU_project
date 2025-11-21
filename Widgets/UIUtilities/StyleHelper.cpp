#include "StyleHelper.h"
#include "Constants.h"
#include <QWidget>
#include <QFont>
#include <algorithm>

namespace UI {

QString StyleHelper::pillButtonStyle(int radiusPx) {
    return QString(R"(
        QPushButton {
            background: %1;
            color: %2;
            border: 0;
            border-radius: %3px;
        }
        QPushButton:hover {
            background: %4;
        }
        QPushButton:pressed {
            background: %5;
        }
    )").arg(Colors::BUTTON_DEFAULT)
        .arg(Colors::TEXT)
        .arg(radiusPx)
        .arg(Colors::BUTTON_HOVER)
        .arg(Colors::BUTTON_PRESSED);
}

QString StyleHelper::labelChipStyle(int radiusPx, int padH, int padV) {
    return QString(R"(
        QLabel {
            background: %1;
            color: %2;
            border-radius: %3px;
            padding: %4px %5px;
        }
    )").arg(Colors::CHIP_BG)
        .arg(Colors::TEXT)
        .arg(radiusPx)
        .arg(padV)
        .arg(padH);
}

QString StyleHelper::enabledPillStyle(int radiusPx, int padH, int padV) {
    return QString(R"(
        QLabel {
            background: %1;
            color: white;
            border-radius: %2px;
            padding: %3px %4px;
        }
    )").arg(Colors::SUCCESS)
        .arg(radiusPx)
        .arg(padV)
        .arg(padH);
}

void StyleHelper::setScaledFont(QWidget* widget, int baseSize, double scale, bool bold) {
    QFont font = widget->font();
    font.setPointSize(std::max(6, static_cast<int>(baseSize * scale)));
    font.setBold(bold);
    widget->setFont(font);
}

int StyleHelper::scaled(int value, double ratio) {
    return static_cast<int>(std::round(value * ratio));
}

QString StyleHelper::lineEditStyle() {
    return QString(R"(
        QLineEdit {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 4px;
            padding: 5px;
        }
        QLineEdit:focus {
            border: 1px solid %4;
        }
    )").arg(Colors::CARD_BG)
        .arg(Colors::TEXT)
        .arg(Colors::CARD_BORDER)
        .arg(Colors::TEXT_MUTED);
}

QString StyleHelper::comboBoxStyle() {
    return QString(R"(
        QComboBox {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 4px;
            padding: 5px;
        }
        QComboBox:hover {
            border: 1px solid %4;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox QAbstractItemView {
            background: %1;
            color: %2;
            selection-background-color: %5;
        }
    )").arg(Colors::CARD_BG)
        .arg(Colors::TEXT)
        .arg(Colors::CARD_BORDER)
        .arg(Colors::TEXT_MUTED)
        .arg(Colors::BUTTON_HOVER);
}

QString StyleHelper::checkBoxStyle() {
    return QString(R"(
        QCheckBox {
            color: %1;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 1px solid %2;
            border-radius: 3px;
            background: %3;
        }
        QCheckBox::indicator:checked {
            background: %4;
            border-color: %4;
        }
    )").arg(Colors::TEXT)
        .arg(Colors::CARD_BORDER)
        .arg(Colors::CARD_BG)
        .arg(Colors::SUCCESS);
}

QString StyleHelper::groupBoxStyle() {
    return QString(R"(
        QGroupBox {
            color: %1;
            border: 1px solid %2;
            border-radius: 6px;
            margin-top: 12px;
            padding-top: 12px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
    )").arg(Colors::TEXT)
        .arg(Colors::CARD_BORDER);
}

QString StyleHelper::scrollAreaStyle() {
    return QString(R"(
        QScrollArea {
            background: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background: %1;
            width: 10px;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical {
            background: %2;
            border-radius: 5px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )").arg(Colors::CARD_BG)
        .arg(Colors::CHIP_BG);
}

QString StyleHelper::dialogStyle() {
    return QString(R"(
        QDialog {
            background: %1;
        }
        QLabel {
            color: %2;
        }
        QPushButton {
            background: %3;
            color: %2;
            border: none;
            border-radius: 4px;
            padding: 8px 16px;
        }
        QPushButton:hover {
            background: %4;
        }
        QPushButton:pressed {
            background: %5;
        }
    )").arg(Colors::CARD_BG)
        .arg(Colors::TEXT)
        .arg(Colors::BUTTON_DEFAULT)
        .arg(Colors::BUTTON_HOVER)
        .arg(Colors::BUTTON_PRESSED);
}

} // namespace UI

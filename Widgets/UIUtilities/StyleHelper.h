#pragma once
#include <QString>
#include <QFont>

class QWidget;
class QLabel;

namespace UI {

class StyleHelper {
public:
    // Generate button pill style
    static QString pillButtonStyle(int radiusPx);

    // Generate label chip style
    static QString labelChipStyle(int radiusPx, int padH, int padV);

    // Generate enabled pill style
    static QString enabledPillStyle(int radiusPx, int padH, int padV);

    // Apply font scaling
    static void setScaledFont(QWidget* widget, int baseSize, double scale, bool bold = false);

    // Calculate scaled value
    static int scaled(int value, double ratio);

    // Common widget styles
    static QString lineEditStyle();
    static QString comboBoxStyle();
    static QString checkBoxStyle();
    static QString groupBoxStyle();
    static QString scrollAreaStyle();
    static QString dialogStyle();
};

} // namespace UI

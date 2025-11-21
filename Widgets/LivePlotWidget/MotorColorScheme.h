#pragma once

#include <QColor>
#include <QMap>
#include <QString>

// Motor color management for visual correlation between SlotWidgets and LivePlot
class MotorColorScheme {
public:
    // Predefined colors for each motor type
    static const QMap<QString, QColor>& defaultColors() {
        static const QMap<QString, QColor> colors = {
            {"MOTOR_E_FLEX",      QColor("#FF00FF")},  // Red
            {"MOTOR_E_EXT",       QColor("#00FFFF")},  // Cyan
            {"MOTOR_S_FLEX",      QColor("#FFFF00")},  // Yellow
            {"MOTOR_S_EXT",       QColor("#FF8C00")},  // orange
            {"MOTOR_S_ADD_PRON",  QColor("#0000CD")},  // blue
            {"MOTOR_S_ABD",       QColor("#006400")},  // green
            {"MOTOR_S_ADD_SUP",   QColor("#DC143C")},  // Light Green
        };
        return colors;
    }
    
    // Get color for a motor name
    static QColor colorForMotor(const QString& motorName) {
        const auto normalized = motorName.trimmed().toUpper();
        const auto& colors = defaultColors();
        
        if (colors.contains(normalized)) {
            return colors[normalized];
        }
        
        // Fallback color for unknown motors
        return QColor("#888888");
    }
    
    // Check if motor has a predefined color
    static bool hasColor(const QString& motorName) {
        const auto normalized = motorName.trimmed().toUpper();
        return defaultColors().contains(normalized);
    }
};

#pragma once
#include <QColor>

namespace UI {

// Color scheme
namespace Colors {
inline constexpr auto CARD_BG = "#26252B";
inline constexpr auto CARD_BORDER = "#1A1A1D";
inline constexpr auto TEXT = "#E6E6E6";
inline constexpr auto TEXT_MUTED = "#BDBDBD";
inline constexpr auto CHIP_BG = "#2C2B31";
inline constexpr auto CHART_LINE = "#D9D9D9";
inline constexpr auto SUCCESS = "#27AE60";
inline constexpr auto DANGER = "#C0392B";
inline constexpr auto BUTTON_DEFAULT = "#1F1E23";
inline constexpr auto BUTTON_HOVER = "#27262C";
inline constexpr auto BUTTON_PRESSED = "#141318";
}

// Layout dimensions
namespace Layout {
inline constexpr int BASE_WIDTH = 980;
inline constexpr int BASE_HEIGHT = 280;
inline constexpr int CARD_RADIUS = 16;
inline constexpr int MARGIN = 18;
inline constexpr int SPACING_H = 18;
inline constexpr int SPACING_V = 12;
}

// Widget dimensions
namespace Widgets {
inline constexpr int TOGGLE_WIDTH = 64;
inline constexpr int TOGGLE_HEIGHT = 32;
inline constexpr int BUTTON_WIDTH = 280;
inline constexpr int BUTTON_HEIGHT = 45;
inline constexpr int PLOT_WIDTH = 280;
inline constexpr int PLOT_HEIGHT = 235;
}
namespace ControlRowSize {
inline constexpr int BASE_WIDTH    = 300;
inline constexpr int BASE_HEIGHT   = 150;
inline constexpr int LABEL_FONT    = 60;
inline constexpr int EDIT_HEIGHT   = 36;
inline constexpr int TOGGLE_WIDTH  = 82;
inline constexpr int TOGGLE_HEIGHT = 82;
}

// Spring function constants
namespace SpringFunction {
inline constexpr int POINTS_COUNT = 360;  // Number of points in spring function
inline constexpr double MAX_AMPLITUDE = 50000.0;  // Maximum amplitude value
inline constexpr int PLOT_MIN_HEIGHT = 150;  // Minimum plot height
}

// Dialog dimensions
namespace Dialog {
inline constexpr int STARTUP_WIDTH = 500;
inline constexpr int STARTUP_HEIGHT = 400;
inline constexpr int GROUP_SPACING = 15;
inline constexpr int BUTTON_MIN_WIDTH = 120;
}

} // namespace UI

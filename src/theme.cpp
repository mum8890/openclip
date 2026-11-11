#include "theme.h"

// Dark editor theme. Colours: background #0E0E13, panels #15151C, accent cyan #22D3EE, violet #7C3AED.
QString appStyleSheet() {
    return R"(
* { font-size: 13px; font-family: "Segoe UI", "Inter", "Noto Sans", "DejaVu Sans", sans-serif; }
QWidget { background: #0E0E13; color: #E6E6EE; }
QMainWindow, QDialog { background: #0E0E13; }
QLabel { background: transparent; }
QLabel#muted { color: #8B8B9E; font-size: 12px; }
QLabel#title { font-size: 15px; font-weight: bold; }
QLabel#brand { font-size: 16px; font-weight: bold; }
QLabel#hint { color: #8B8B9E; }
QWidget#panel { background: #15151C; border-radius: 10px; }
QWidget#topbar { background: #12121A; border-bottom: 1px solid #22222E; }
QWidget#playerbar { background: #12121A; border-radius: 10px; }
QWidget#timelinebar { background: #12121A; }

QPushButton { background: #1F1F2A; border: 1px solid #2C2C3B; border-radius: 8px; padding: 7px 14px; }
QPushButton:hover { background: #2A2A39; border-color: #3A3A4E; }
QPushButton:pressed { background: #171722; }
QPushButton:disabled { color: #5A5A6A; }
QPushButton#accent { color: #08080C; font-weight: bold; border: none; padding: 8px 20px;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #22D3EE, stop:1 #7C3AED); }
QPushButton#accent:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #67E8F9, stop:1 #8B5CF6); }
QPushButton#flat { background: transparent; border: none; padding: 6px 10px; }
QPushButton#flat:hover { background: #22222E; }
QPushButton#round { background: #22D3EE; border: none; border-radius: 18px; min-width: 36px; max-width: 36px; min-height: 36px; max-height: 36px; padding: 0; }
QPushButton#round:hover { background: #67E8F9; }

QTabWidget::pane { border: none; background: #15151C; border-radius: 10px; }
QTabBar { background: transparent; }
QTabBar::tab { background: transparent; color: #8B8B9E; padding: 9px 11px; border-bottom: 2px solid transparent; margin-right: 2px; }
QTabBar::tab:hover { color: #E6E6EE; }
QTabBar::tab:selected { color: #FFFFFF; border-bottom: 2px solid #22D3EE; font-weight: bold; }

QListWidget { background: transparent; border: none; outline: none; }
QListWidget::item { background: #1B1B25; border-radius: 8px; padding: 9px 10px; margin: 3px 2px; }
QListWidget::item:hover { background: #242433; }
QListWidget::item:selected { background: #26324A; border: 1px solid #22D3EE; color: #FFFFFF; }

QLineEdit, QComboBox, QDoubleSpinBox, QSpinBox { background: #1B1B25; border: 1px solid #2C2C3B; border-radius: 7px; padding: 6px 8px; selection-background-color: #22D3EE; selection-color: #000; }
QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus, QSpinBox:focus { border-color: #22D3EE; }
QComboBox QAbstractItemView { background: #1B1B25; border: 1px solid #2C2C3B; selection-background-color: #26324A; outline: none; }

QSlider::groove:horizontal { height: 4px; background: #2C2C3B; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #22D3EE; border-radius: 2px; }
QSlider::handle:horizontal { background: #FFFFFF; width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; }
QSlider::handle:horizontal:hover { background: #22D3EE; }

QScrollArea { border: none; background: transparent; }
QScrollBar:horizontal { height: 10px; background: transparent; margin: 0; }
QScrollBar::handle:horizontal { background: #2C2C3B; border-radius: 5px; min-width: 30px; }
QScrollBar::handle:horizontal:hover { background: #3C3C52; }
QScrollBar:vertical { width: 10px; background: transparent; margin: 0; }
QScrollBar::handle:vertical { background: #2C2C3B; border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: #3C3C52; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QSplitter::handle { background: #0E0E13; }
QProgressBar { background: #1B1B25; border: none; border-radius: 4px; height: 8px; text-align: center; color: transparent; }
QProgressBar::chunk { border-radius: 4px; background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #22D3EE, stop:1 #7C3AED); }
QMainWindow::separator { background: #0E0E13; width: 6px; height: 6px; }
QMainWindow::separator:hover { background: #22D3EE; }
QWidget#docktitle { background: #15151C; border-bottom: 1px solid #22222E; border-top-left-radius: 10px; border-top-right-radius: 10px; }
QToolBar { border: none; padding: 0; margin: 0; spacing: 0; background: #12121A; }
QStatusBar { background: #12121A; color: #8B8B9E; border-top: 1px solid #22222E; }
QMenuBar { background: #12121A; }
QMenuBar::item { padding: 6px 12px; background: transparent; }
QMenuBar::item:selected { background: #22222E; border-radius: 6px; }
QMenu { background: #1B1B25; border: 1px solid #2C2C3B; padding: 4px; }
QMenu::item { padding: 7px 22px; border-radius: 6px; }
QMenu::item:selected { background: #26324A; }
QToolTip { background: #1B1B25; color: #E6E6EE; border: 1px solid #2C2C3B; padding: 4px; }
QMessageBox { background: #15151C; }
)";
}

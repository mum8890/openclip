#include "theme.h"

// Glassy, colourful editor theme for OpenClip 0.8.
// Deep indigo window, aurora gradient, translucent "frosted" panels, cyan -> violet -> pink accents.
QString appStyleSheet() {
    return R"(
* { font-size: 13px; font-family: "Segoe UI", "Inter", "Noto Sans", "DejaVu Sans", sans-serif; }
QWidget { background: transparent; color: #E8E8F4; }
QMainWindow {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #08080F, stop:0.22 #14102E, stop:0.48 #1B0F31,
                stop:0.72 #0D1730, stop:1 #070C18);
}
QDialog { background: #12101E; }
QDialog QWidget { background: transparent; }
QLabel { background: transparent; }
QLabel#muted { color: #A0A0BC; font-size: 12px; }
QLabel#title { font-size: 15px; font-weight: bold; color: #FFFFFF; }
QLabel#brand { font-size: 17px; font-weight: bold; color: #A5F3FC; }
QLabel#hint { color: #A0A0BC; }

QWidget#panel {
    background: rgba(22, 21, 38, 168);
    border: 1px solid rgba(255, 255, 255, 20);
    border-radius: 14px;
}
QWidget#topbar {
    background: rgba(9, 9, 18, 190);
    border-bottom: 1px solid rgba(255, 255, 255, 22);
}
QWidget#playerbar { background: rgba(18, 17, 32, 190); border: 1px solid rgba(255, 255, 255, 18); border-radius: 12px; }
QWidget#timelinebar { background: rgba(18, 17, 32, 160); border-radius: 12px; }
QWidget#glassbox {
    background: rgba(255, 255, 255, 10);
    border: 1px solid rgba(255, 255, 255, 22);
    border-radius: 12px;
}

QPushButton {
    background: rgba(255, 255, 255, 16);
    border: 1px solid rgba(255, 255, 255, 30);
    border-radius: 9px; padding: 7px 14px; color: #F2F2FF;
}
QPushButton:hover { background: rgba(255, 255, 255, 30); border-color: rgba(165, 243, 252, 140); }
QPushButton:pressed { background: rgba(255, 255, 255, 10); }
QPushButton:disabled { color: #6E6E88; background: rgba(255, 255, 255, 8); }
QPushButton#accent {
    color: #07070F; font-weight: bold; border: none; padding: 8px 20px;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #22D3EE, stop:0.55 #7C3AED, stop:1 #F472B6);
}
QPushButton#accent:hover {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #67E8F9, stop:0.55 #8B5CF6, stop:1 #FB9BCB);
}
QPushButton#flat { background: transparent; border: none; padding: 6px 10px; }
QPushButton#flat:hover { background: rgba(255, 255, 255, 26); }
QPushButton#round {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #22D3EE, stop:1 #7C3AED);
    border: none; border-radius: 18px; min-width: 36px; max-width: 36px;
    min-height: 36px; max-height: 36px; padding: 0;
}
QPushButton#round:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #67E8F9, stop:1 #A78BFA); }

QCheckBox { spacing: 8px; color: #D8D8EE; }
QCheckBox::indicator {
    width: 16px; height: 16px; border-radius: 5px;
    border: 1px solid rgba(255, 255, 255, 70); background: rgba(255, 255, 255, 18);
}
QCheckBox::indicator:hover { border-color: #67E8F9; }
QCheckBox::indicator:checked {
    border-color: #22D3EE;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #22D3EE, stop:1 #7C3AED);
}

QTabWidget::pane { border: none; background: transparent; border-radius: 12px; }
QTabBar { background: transparent; }
QTabBar::tab { background: transparent; color: #A0A0BC; padding: 9px 12px; border-bottom: 2px solid transparent; margin-right: 2px; }
QTabBar::tab:hover { color: #FFFFFF; }
QTabBar::tab:selected { color: #FFFFFF; border-bottom: 2px solid #22D3EE; font-weight: bold; }

QListWidget { background: transparent; border: none; outline: none; }
QListWidget::item {
    background: rgba(255, 255, 255, 14);
    border: 1px solid transparent;
    border-radius: 10px; padding: 9px 10px; margin: 3px 2px;
}
QListWidget::item:hover { background: rgba(255, 255, 255, 28); border-color: rgba(255, 255, 255, 26); }
QListWidget::item:selected {
    background: rgba(34, 211, 238, 46);
    border: 1px solid #22D3EE; color: #FFFFFF;
}

QLineEdit, QComboBox, QDoubleSpinBox, QSpinBox {
    background: rgba(255, 255, 255, 18);
    border: 1px solid rgba(255, 255, 255, 34);
    border-radius: 8px; padding: 6px 8px;
    selection-background-color: #22D3EE; selection-color: #07070F;
}
QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus, QSpinBox:focus { border-color: #67E8F9; }
QComboBox::drop-down { border: none; width: 18px; }
QComboBox QAbstractItemView {
    background: #191628; border: 1px solid rgba(255, 255, 255, 40);
    selection-background-color: rgba(34, 211, 238, 60); outline: none;
}
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { width: 14px; border: none; background: transparent; }

QSlider::groove:horizontal { height: 5px; background: rgba(255, 255, 255, 30); border-radius: 3px; }
QSlider::sub-page:horizontal {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #22D3EE, stop:1 #A78BFA);
    border-radius: 3px;
}
QSlider::handle:horizontal { background: #FFFFFF; width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; }
QSlider::handle:horizontal:hover { background: #67E8F9; }

QTextBrowser#chat {
    background: rgba(8, 8, 16, 110);
    border: 1px solid rgba(255, 255, 255, 30);
    border-radius: 12px; padding: 8px;
    selection-background-color: #7C3AED;
}

QScrollArea { border: none; background: transparent; }
QScrollArea > QWidget > QWidget { background: transparent; }
QScrollBar:horizontal { height: 10px; background: transparent; margin: 0; }
QScrollBar::handle:horizontal { background: rgba(255, 255, 255, 50); border-radius: 5px; min-width: 30px; }
QScrollBar::handle:horizontal:hover { background: rgba(255, 255, 255, 90); }
QScrollBar:vertical { width: 10px; background: transparent; margin: 0; }
QScrollBar::handle:vertical { background: rgba(255, 255, 255, 50); border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: rgba(255, 255, 255, 90); }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QSplitter::handle { background: transparent; }
QProgressBar { background: rgba(255, 255, 255, 22); border: none; border-radius: 4px; height: 8px; text-align: center; color: transparent; }
QProgressBar::chunk { border-radius: 4px; background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #22D3EE, stop:0.5 #7C3AED, stop:1 #F472B6); }
QProgressBar#big { height: 16px; border-radius: 8px; }
QProgressBar#big::chunk { border-radius: 8px; }
QLabel#percent { font-size: 42px; font-weight: bold; color: #FFFFFF; }
QMainWindow::separator { background: transparent; width: 6px; height: 6px; }
QMainWindow::separator:hover { background: rgba(34, 211, 238, 120); }
QWidget#docktitle {
    background: rgba(255, 255, 255, 10);
    border-bottom: 1px solid rgba(255, 255, 255, 18);
    border-top-left-radius: 14px; border-top-right-radius: 14px;
}
QDockWidget { color: #E8E8F4; }
QDockWidget::title { background: rgba(255, 255, 255, 10); padding: 6px; }
QToolBar { border: none; padding: 0; margin: 0; spacing: 0; background: transparent; }
QStatusBar { background: rgba(9, 9, 18, 180); color: #A0A0BC; border-top: 1px solid rgba(255, 255, 255, 18); }
QMenuBar { background: rgba(9, 9, 18, 150); }
QMenuBar::item { padding: 6px 12px; background: transparent; border-radius: 6px; }
QMenuBar::item:selected { background: rgba(255, 255, 255, 26); }
QMenu { background: #171426; border: 1px solid rgba(255, 255, 255, 34); padding: 5px; border-radius: 10px; }
QMenu::item { padding: 7px 24px; border-radius: 7px; }
QMenu::item:selected { background: rgba(124, 58, 237, 150); }
QMenu::separator { height: 1px; background: rgba(255, 255, 255, 24); margin: 4px 8px; }
QToolTip { background: #171426; color: #E8E8F4; border: 1px solid rgba(255, 255, 255, 40); padding: 5px; border-radius: 6px; }
QMessageBox { background: #151222; }
QMessageBox QLabel { color: #E8E8F4; }
)";
}

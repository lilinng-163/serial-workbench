# SPDX-License-Identifier: Apache-2.0

"""Shared color tokens for widgets, plots, and received text."""

PALETTES = {
    "light": {
        "base": "#edf2f7", "panel": "#f7f9fc", "input": "#ffffff",
        "text": "#24334a", "muted": "#657a92", "border": "#c9d5e3",
        "accent": "#2468a8", "hover": "#dce9f7", "badge": "#e0ebf7",
        "monitor": "#ffffff", "rx": "#087c70", "tx": "#2468a8",
        "status": "#9c6500", "danger": "#b73f4c", "button_text": "#ffffff",
    },
    "dark": {
        "base": "#111b29", "panel": "#182638", "input": "#1d2d41",
        "text": "#dce7f3", "muted": "#93a8c0", "border": "#344b65",
        "accent": "#5aa7f1", "hover": "#29415c", "badge": "#253b53",
        "monitor": "#0c1520", "rx": "#70d5c4", "tx": "#91c3fa",
        "status": "#eac077", "danger": "#e87886", "button_text": "#102033",
    },
}


def stylesheet(c):
    return """
    QMainWindow, QWidget { background: %(base)s; color: %(text)s; }
    QLabel#brand { font-size: 17px; font-weight: 700; }
    QLabel#subtitle { color: %(muted)s; }
    QLabel#section { font-size: 14px; font-weight: 600; }
    QLabel#badge { background: %(badge)s; color: %(text)s; border-radius: 5px; padding: 7px 10px; }
    QFrame#sidebar { background: %(panel)s; border: 1px solid %(border)s; border-radius: 8px; }
    QFrame#sidebar QLabel, QFrame#sidebar QScrollArea, QWidget#sidebarContent, QWidget#sidebarSection { background: %(panel)s; }
    QLineEdit, QComboBox, QSpinBox, QPlainTextEdit, QTextEdit, QListWidget, QTreeWidget {
        background: %(input)s; color: %(text)s; border: 1px solid %(border)s;
        border-radius: 5px; padding: 6px; selection-background-color: %(accent)s;
        selection-color: %(button_text)s;
    }
    QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QPlainTextEdit:focus { border: 1px solid %(accent)s; }
    QComboBox { padding-right: 30px; combobox-popup: 0; min-height: 20px; }
    QComboBox::drop-down { border: none; width: 30px; }
    QComboBox::down-arrow { image: none; }
    QComboBox QAbstractItemView {
        background: %(input)s; color: %(text)s; border: 1px solid %(border)s;
        selection-background-color: %(hover)s; selection-color: %(text)s;
        outline: none; padding: 4px;
    }
    QComboBox QAbstractItemView::item { min-height: 29px; padding-left: 8px; }
    QSpinBox { padding-right: 27px; }
    QSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right; width: 24px; border: none; }
    QSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; width: 24px; border: none; }
    QSpinBox::up-arrow, QSpinBox::down-arrow { image: none; }
    QPushButton { background: %(input)s; color: %(text)s; border: 1px solid %(border)s; padding: 6px 10px; border-radius: 5px; min-height: 20px; }
    QPushButton:hover { background: %(hover)s; border-color: %(accent)s; }
    QPushButton:pressed { background: %(border)s; }
    QPushButton:disabled { color: %(muted)s; }
    QPushButton#primary { background: %(accent)s; color: %(button_text)s; border-color: %(accent)s; font-weight: 600; }
    QPushButton#primary[connectionState="active"] { background: %(danger)s; border-color: %(danger)s; }
    QPushButton#primary[connectionState="busy"] { background: %(badge)s; color: %(text)s; border-color: %(border)s; }
    QPushButton:focus { border: 1px solid %(accent)s; }
    QToolButton { background: transparent; color: %(text)s; padding: 6px 10px; border: 1px solid transparent; border-radius: 5px; min-height: 20px; }
    QToolButton:hover { background: %(hover)s; border-color: %(border)s; }
    QToolButton::menu-indicator { subcontrol-position: right center; }
    QMenu { background: %(input)s; color: %(text)s; border: 1px solid %(border)s; padding: 5px; }
    QMenu::item { padding: 7px 26px 7px 12px; border-radius: 4px; }
    QMenu::item:selected { background: %(hover)s; }
    QMenu::separator { height: 1px; background: %(border)s; margin: 5px 8px; }
    QTabWidget::pane { border: 1px solid %(border)s; border-radius: 6px; }
    QTabBar::tab { padding: 8px 14px; color: %(muted)s; background: %(base)s; }
    QTabBar::tab:selected { background: %(input)s; color: %(accent)s; border-bottom: 3px solid %(accent)s; }
    QListWidget::item { padding: 8px; border-radius: 4px; }
    QListWidget::item:selected { background: %(hover)s; color: %(text)s; }
    QTreeWidget::item { min-height: 24px; padding: 2px 4px; }
    QTreeWidget::item:selected { background: %(hover)s; color: %(text)s; }
    QHeaderView::section { background: %(badge)s; color: %(text)s; border: none; border-right: 1px solid %(border)s; padding: 5px; }
    QTextEdit#console, QPlainTextEdit#terminal { background: %(monitor)s; color: %(text)s; border: none; padding: 12px; }
    QCheckBox { spacing: 6px; }
    QStatusBar { color: %(muted)s; }
    QSplitter::handle { background: %(border)s; }
    QScrollBar:vertical { background: %(base)s; width: 10px; margin: 0; }
    QScrollBar:horizontal { background: %(base)s; height: 10px; margin: 0; }
    QScrollBar::handle:vertical { background: %(border)s; min-height: 28px; border-radius: 4px; }
    QScrollBar::handle:horizontal { background: %(border)s; min-width: 28px; border-radius: 4px; }
    QScrollBar::handle:hover { background: %(muted)s; }
    QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
    QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
    QToolTip { background: %(input)s; color: %(text)s; border: 1px solid %(border)s; padding: 4px; }
    """ % c

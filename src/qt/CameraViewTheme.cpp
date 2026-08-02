#include "CameraViewTheme.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QStyleFactory>

void applyCameraViewTheme(QApplication& application)
{
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setPointSize(10);
    application.setFont(font);

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#11171f")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#e8edf5")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#0e141b")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#151d27")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#e8edf5")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#202a36")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#e8edf5")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#2f7ff7")));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#748094")));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(QStringLiteral("#657184")));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(QStringLiteral("#657184")));
    application.setPalette(palette);

    application.setStyleSheet(QStringLiteral(R"(
        QMainWindow { background: #0e131a; }
        QWidget { color: #e8edf5; selection-background-color: #2f7ff7; selection-color: #ffffff; }

        QMenuBar {
            background: #151c25;
            border-bottom: 1px solid #273241;
            padding: 3px 7px;
        }
        QMenuBar::item { padding: 6px 10px; border-radius: 5px; }
        QMenuBar::item:selected { background: #273344; }
        QMenu {
            background: #18212c;
            border: 1px solid #344254;
            border-radius: 7px;
            padding: 6px;
        }
        QMenu::item { padding: 7px 28px 7px 12px; border-radius: 5px; }
        QMenu::item:selected { background: #2b3b50; }
        QMenu::separator { height: 1px; background: #334051; margin: 5px 8px; }

        QToolBar {
            background: #151c25;
            border: 0;
            border-bottom: 1px solid #273241;
            padding: 6px 9px;
            spacing: 5px;
        }
        QToolBar::separator { width: 1px; background: #344152; margin: 4px 7px; }
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 6px;
            padding: 6px 9px;
        }
        QToolButton:hover { background: #222d3a; border-color: #344456; }
        QToolButton:pressed, QToolButton:checked { background: #253b5a; border-color: #397fdf; }

        QDockWidget { background: #131a23; border-left: 1px solid #2c3745; }
        QDockWidget::title {
            background: #18212b;
            border-bottom: 1px solid #2a3543;
            padding: 10px 12px;
            font-weight: 600;
        }

        QTabWidget::pane { border: 1px solid #2d3948; background: #111820; }
        QTabBar::tab {
            background: #18212b;
            color: #9ca8b9;
            border: 1px solid transparent;
            padding: 8px 11px;
            min-width: 34px;
        }
        QTabBar::tab:hover { color: #f3f6fb; background: #202b38; }
        QTabBar::tab:selected {
            color: #ffffff;
            background: #22344c;
            border-bottom: 2px solid #4d94ff;
            font-weight: 600;
        }
        QTabWidget#FunctionTabs::pane { border: 0; border-top: 1px solid #2d3948; }
        QTabWidget#FunctionTabs > QTabBar::tab { padding: 9px 10px; }

        QScrollArea, QScrollArea > QWidget > QWidget { background: #111820; border: 0; }
        QWidget[panelPage="true"] { background: #111820; }

        QGroupBox {
            background: #171f29;
            border: 1px solid #2c3847;
            border-radius: 8px;
            margin-top: 15px;
            padding: 13px 10px 10px 10px;
            font-weight: 600;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 5px;
            color: #cbd5e4;
            background: #171f29;
        }

        QPushButton {
            background: #222d3a;
            border: 1px solid #3a4859;
            border-radius: 6px;
            padding: 7px 11px;
            min-height: 18px;
        }
        QPushButton:hover { background: #2a3746; border-color: #52657a; }
        QPushButton:pressed { background: #1b2531; }
        QPushButton:focus { border-color: #4d94ff; }
        QPushButton:disabled { background: #171e27; color: #657184; border-color: #28323e; }
        QPushButton[role="primary"] { background: #2f7ff7; border-color: #438cf8; color: white; font-weight: 600; }
        QPushButton[role="primary"]:hover { background: #428cf8; border-color: #6aa7ff; }
        QPushButton[role="primary"]:pressed { background: #2469cf; }
        QPushButton[role="danger"] { color: #ffb4b4; border-color: #684044; background: #36252a; }
        QPushButton[role="danger"]:hover { background: #4a2b31; border-color: #92535b; }

        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QPlainTextEdit, QListWidget {
            background: #0e151d;
            border: 1px solid #354354;
            border-radius: 6px;
            padding: 6px 8px;
        }
        QLineEdit:hover, QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover, QListWidget:hover {
            border-color: #4b5d72;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus,
        QPlainTextEdit:focus, QListWidget:focus { border-color: #4d94ff; }
        QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled {
            background: #151b23; color: #657184; border-color: #28323e;
        }
        QComboBox::drop-down { border: 0; width: 24px; }
        QComboBox QAbstractItemView {
            background: #18212c;
            border: 1px solid #3a485a;
            selection-background-color: #2f7ff7;
            padding: 4px;
        }
        QListWidget { outline: 0; padding: 4px; }
        QListWidget::item { padding: 7px 6px; border-radius: 4px; }
        QListWidget::item:hover { background: #1e2a38; }
        QListWidget::item:selected { background: #294e7c; color: white; }

        QCheckBox { spacing: 8px; }
        QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #506075; border-radius: 4px; background: #0e151d; }
        QCheckBox::indicator:hover { border-color: #6e88a8; }
        QCheckBox::indicator:checked { background: #2f7ff7; border-color: #5b9cff; }

        QSlider::groove:horizontal { height: 4px; border-radius: 2px; background: #334153; }
        QSlider::sub-page:horizontal { background: #3d8cff; border-radius: 2px; }
        QSlider::handle:horizontal { width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; background: #eef5ff; border: 2px solid #3d8cff; }

        QProgressBar { background: #101720; border: 1px solid #344152; border-radius: 6px; text-align: center; min-height: 18px; }
        QProgressBar::chunk { background: #2f7ff7; border-radius: 5px; }

        QStatusBar { background: #151c25; border-top: 1px solid #2b3745; color: #aeb9c9; }
        QStatusBar QLabel { padding: 3px 9px; color: #aeb9c9; }
        QStatusBar::item { border: 0; }

        QScrollBar:vertical { background: #111820; width: 11px; margin: 2px; }
        QScrollBar::handle:vertical { background: #3a4859; border-radius: 4px; min-height: 28px; }
        QScrollBar::handle:vertical:hover { background: #506176; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { background: #111820; height: 11px; margin: 2px; }
        QScrollBar::handle:horizontal { background: #3a4859; border-radius: 4px; min-width: 28px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

        QToolTip { background: #243142; color: #f4f7fb; border: 1px solid #52647a; padding: 6px; }
    )"));
}

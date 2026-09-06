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
            padding: 4px 8px;
            spacing: 4px;
        }
        QToolBar::separator { width: 1px; background: #344152; margin: 4px 7px; }
        QToolBar#MainToolbar { min-height: 66px; }
        QToolBar#MainToolbar QToolButton {
            min-width: 44px;
            min-height: 56px;
            padding: 4px 7px;
        }
        QToolBar#MainToolbar[presentationMode="compact"] QToolButton {
            min-width: 42px;
            padding: 4px 6px;
        }
        QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 6px;
            padding: 6px 9px;
        }
        QToolButton:hover { background: #222d3a; border-color: #344456; }
        QToolButton:pressed, QToolButton:checked { background: #253b5a; border-color: #397fdf; }
        QToolButton[role="measurementTool"],
        QToolButton[role="measurementAction"],
        QToolButton[role="toolbarCommand"],
        QToolButton[role="toolbarMore"],
        QToolButton[role="dangerAction"] {
            background: #1d2733;
            border: 1px solid #354558;
            border-radius: 8px;
            padding: 7px 5px 6px 5px;
            font-weight: 500;
        }
        QToolButton[role="measurementTool"]:hover,
        QToolButton[role="measurementAction"]:hover,
        QToolButton[role="toolbarCommand"]:hover,
        QToolButton[role="toolbarMore"]:hover {
            background: #263548;
            border-color: #52729a;
        }
        QToolButton[role="measurementTool"]:checked,
        QToolButton[role="toolbarMore"][hiddenActiveTool="true"] {
            background: #25558f;
            border-color: #69a7ff;
            color: #ffffff;
            font-weight: 600;
        }
        QToolButton[role="measurementAction"]:pressed {
            background: #253b5a;
            border-color: #397fdf;
        }
        QToolButton[role="dangerAction"]:hover {
            background: #3a2228;
            border-color: #b94b5b;
            color: #ffd7dc;
        }
        QToolButton[role="dangerAction"]:pressed {
            background: #5a2630;
            border-color: #ef6477;
            color: #ffffff;
        }
        QToolButton[role="measurementTool"]:disabled,
        QToolButton[role="measurementAction"]:disabled,
        QToolButton[role="toolbarCommand"]:disabled,
        QToolButton[role="dangerAction"]:disabled {
            background: #171e27;
            color: #657184;
            border-color: #28323e;
        }

        QDockWidget { background: #131a23; border-left: 1px solid #2c3745; }
        QDockWidget::title {
            background: #18212b;
            border-bottom: 1px solid #2a3543;
            padding: 8px 12px;
            font-weight: 600;
        }

        #MainViewport { background: #0d141c; }
        #ViewportContextBar {
            min-height: 32px;
            max-height: 38px;
            background: #121b25;
            border-bottom: 1px solid #293747;
        }
        #ViewportContextBar QLabel { padding: 3px 7px; }
        #ViewportContextBar QLabel[contextRole="source"] {
            color: #e4edf9;
            font-weight: 600;
        }
        #ViewportContextBar QLabel[contextRole="stage"] {
            color: #9fb1c5;
            background: #182431;
            border: 1px solid #2b3b4d;
            border-radius: 7px;
        }
        #ViewportContextBar QLabel[contextRole="mode"] {
            color: #acd2ff;
            background: #172a43;
            border: 1px solid #315f91;
            border-radius: 7px;
        }
        #ViewportContextBar QLabel[contextRole="mode"][activeMode="true"] {
            color: #eef6ff;
            background: #214f82;
            border-color: #4d94ff;
        }
        #WorkspaceToggleButton {
            min-height: 24px;
            padding: 3px 7px;
            color: #c7d8ec;
            background: #1a2633;
            border: 1px solid #324458;
        }
        #WorkspaceToggleButton:hover,
        #WorkspaceToggleButton:checked {
            background: #223a58;
            border-color: #4d83bf;
            color: #ffffff;
        }

        #WorkspaceShell { background: #111820; }
        #WorkspaceStageNavigation {
            background: #141d28;
            border-bottom: 1px solid #293747;
        }
        #WorkspaceStageNavigation QToolButton[workspaceStage="true"] {
            min-height: 38px;
            padding: 4px 7px;
            color: #9eafc2;
            background: transparent;
            border: 1px solid transparent;
            border-radius: 7px;
            font-weight: 600;
        }
        #WorkspaceStageNavigation QToolButton[workspaceStage="true"]:hover {
            color: #edf5ff;
            background: #1d2b3a;
            border-color: #30445a;
        }
        #WorkspaceStageNavigation QToolButton[workspaceStage="true"]:checked {
            color: #ffffff;
            background: #21466f;
            border-color: #427fbe;
        }
        #WorkspacePageHeader {
            background: #111a24;
            border-bottom: 1px solid #253343;
        }
        #WorkspacePageTitle {
            color: #e9f2fd;
            font-size: 11pt;
            font-weight: 700;
        }
        #WorkspacePageDescription { color: #879bb1; font-size: 9pt; }
        #WorkspacePageNavigation {
            background: #111a24;
            border-bottom: 1px solid #2b3949;
        }
        #WorkspacePageNavigation QToolButton[workspacePage="true"] {
            min-height: 28px;
            padding: 3px 12px;
            color: #9fb0c2;
            background: #17212c;
            border: 1px solid #2c3b4c;
            border-radius: 6px;
        }
        #WorkspacePageNavigation QToolButton[workspacePage="true"]:hover {
            color: #edf5ff;
            background: #202e3d;
            border-color: #435b75;
        }
        #WorkspacePageNavigation QToolButton[workspacePage="true"]:checked {
            color: #ffffff;
            background: #285b96;
            border-color: #5a9dea;
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
        QTabWidget#FunctionTabs::pane { border: 0; background: #111820; }
        QTabWidget#FunctionTabs > QTabBar::tab { padding: 0; min-height: 0; max-height: 0; }

        QScrollArea, QScrollArea > QWidget > QWidget { background: #111820; border: 0; }
        QWidget[panelPage="true"] { background: #111820; }
        #CameraStatusCard {
            background: #17222f;
            border: 1px solid #33475e;
            border-radius: 10px;
        }
        #CameraStateBadge {
            border-radius: 9px;
            padding: 3px 8px;
            font-size: 9pt;
            font-weight: 700;
        }
        #CameraStateBadge[cameraState="busy"] { background: #213d61; color: #9dccff; border: 1px solid #3569a1; }
        #CameraStateBadge[cameraState="success"] { background: #173c2b; color: #75e5a6; border: 1px solid #287a50; }
        #CameraStateBadge[cameraState="warning"] { background: #46371b; color: #ffd47a; border: 1px solid #80612a; }
        #CameraStateBadge[cameraState="error"] { background: #49252a; color: #ff9ca5; border: 1px solid #854049; }
        #CameraStateBadge[cameraState="ready"] { background: #26313e; color: #b9c7d8; border: 1px solid #43546a; }
        #CameraDeviceSummary { color: #d9e6f5; font-weight: 600; }
        #CameraTelemetry { color: #8fa5bd; font-size: 9pt; }
        #CameraInlineFeedback {
            background: #151f2b;
            border: 1px solid #2d4055;
            border-radius: 7px;
            padding: 8px 10px;
            color: #9fb5cc;
        }
        #CameraInlineFeedback[feedbackState="success"] { color: #76dfa4; border-color: #286c4a; background: #142a22; }
        #CameraInlineFeedback[feedbackState="warning"] { color: #ffd27b; border-color: #725829; background: #2c2618; }
        #CameraInlineFeedback[feedbackState="error"] { color: #ff9ca5; border-color: #794049; background: #322126; }
        QToolButton[cameraSectionHeader="true"] {
            background: #192431;
            border: 1px solid #2e4053;
            border-radius: 7px;
            padding: 7px 9px;
            font-weight: 600;
            text-align: left;
        }
        QToolButton[cameraSectionHeader="true"]:hover { background: #213044; border-color: #45617e; }
        QWidget[cameraSection="true"] QPushButton { min-height: 22px; }

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
        #PointCloudWorkspaceToolbar {
            background: #141e2a;
            border: 1px solid #293b4f;
            border-radius: 8px;
        }
        #PointCloudWorkspaceToolbar > QLabel {
            color: #91a7bf;
        }
        #PointCloudWorkspaceToolbar QPushButton,
        #PointCloudWorkspaceToolbar QToolButton {
            min-width: 30px;
            min-height: 30px;
            max-height: 34px;
            padding: 2px 8px;
            border-radius: 6px;
        }
        #PointCloudWorkspaceToolbar QPushButton:checked,
        #PointCloudWorkspaceToolbar QToolButton:checked,
        #PointCloudWorkspaceToolbar QToolButton[activeTool="true"] {
            color: #eef6ff;
            background: #214f82;
            border-color: #4d94ff;
        }
        #PointCloudDataSummary {
            min-height: 30px;
            max-height: 34px;
            background: #101923;
            border: 1px solid #29394b;
            border-left: 3px solid #3d8cff;
            border-radius: 7px;
        }
        #PointCloudDataSummary QLabel {
            color: #95a8bc;
        }
        #PointCloudDataSummary #PointCloudSourceLabel {
            color: #e1ecfa;
            font-weight: 600;
        }
        #PointCloudTaskBar {
            min-height: 32px;
            max-height: 38px;
            background: #152235;
            border: 1px solid #315176;
            border-radius: 7px;
        }
        #PointCloudTaskBar[status="ok"] { background: #13291f; border-color: #2f7652; }
        #PointCloudTaskBar[status="warning"] { background: #302616; border-color: #806128; }
        #PointCloudTaskBar[status="error"] { background: #321c23; border-color: #86404c; }
        #PointCloudTaskBar QProgressBar {
            min-height: 5px;
            max-height: 5px;
            border: 0;
            border-radius: 2px;
            background: #27394e;
        }
        #PointCloudTaskBar QProgressBar::chunk {
            border-radius: 2px;
            background: #4d94ff;
        }
        #PointCloudInspector {
            background: #111820;
            border: 1px solid #2b3949;
            border-radius: 8px;
        }
        #PointCloudInspectorHeader {
            min-height: 32px;
            max-height: 38px;
            background: #162130;
            border-bottom: 1px solid #2d4055;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
        }
        #PointCloudTaskPageTitle {
            color: #e1ecfa;
            font-weight: 600;
        }
        #PointCloudToolTabs::pane,
        #PointCloudCompactDrawerTabs::pane {
            border: 0;
            border-top: 1px solid #2a394a;
            background: #111820;
        }
        #PointCloudToolTabs > QTabBar::tab,
        #PointCloudCompactDrawerTabs > QTabBar::tab {
            min-height: 30px;
            padding: 4px 8px;
            margin: 0;
            background: #141d28;
            border: 0;
            border-bottom: 2px solid transparent;
        }
        #PointCloudToolTabs > QTabBar::tab:selected,
        #PointCloudCompactDrawerTabs > QTabBar::tab:selected {
            color: #f0f6ff;
            background: #1b2a3b;
            border-bottom-color: #4d94ff;
        }
        #PointCloudInspector QGroupBox {
            margin-top: 12px;
            padding: 10px 8px 8px 8px;
        }
        #PointCloudInspector QGroupBox[expanded="false"] {
            color: #9bacbf;
            background: #131c26;
            border-color: #293746;
            padding: 0;
        }
        #PointCloudInspector QGroupBox::indicator {
            width: 12px;
            height: 12px;
        }
        #PointCloudInspector QPushButton,
        #PointCloudInspector QToolButton,
        #PointCloudInspector QComboBox,
        #PointCloudInspector QSpinBox,
        #PointCloudInspector QDoubleSpinBox {
            min-height: 20px;
        }
        #PointCloudInspector QPushButton[role="danger"] {
            color: #d7e1ed;
            border-color: #3a4859;
            background: #222d3a;
        }
        #PointCloudInspector QPushButton[role="danger"]:hover,
        #PointCloudInspector QPushButton[role="danger"]:pressed {
            color: #ffd6d8;
            border-color: #92535b;
            background: #4a2b31;
        }
        #PointCloudSectionWorkspace {
            background: #0d151e;
            border: 1px solid #2b3b4d;
            border-radius: 7px;
        }
        #PointCloudViewPresetCombo {
            min-width: 92px;
            max-width: 118px;
            padding-top: 4px;
            padding-bottom: 4px;
        }
        #PointCloudSourceLabel {
            color: #e6f0fc;
            font-weight: 600;
        }
        #PointCloudTextureStatus {
            color: #8799ad;
            font-size: 12px;
        }
        #PointCloudTextureStatus[status="ok"] {
            color: #70dda1;
        }
        #PointCloudSelectionStatus, #PointCloudWorkspaceStatus {
            color: #a9c9ea;
            padding: 3px 7px;
        }
        #PointCloudDialog QLabel[status] {
            border: 1px solid transparent;
            border-radius: 7px;
            padding: 3px 7px;
        }
        #PointCloudDialog QLabel[status="neutral"] {
            color: #9eafc1;
            background: #18222d;
            border-color: #2c3a49;
        }
        #PointCloudDialog QLabel[status="processing"] {
            color: #b8d9ff;
            background: #172a43;
            border-color: #315f91;
        }
        #PointCloudDialog QLabel[status="ok"] {
            color: #75dfa7;
            background: #14271f;
            border-color: #2b684b;
        }
        #PointCloudDialog QLabel[status="warning"] {
            color: #ffd071;
            background: #2c2417;
            border-color: #745923;
        }
        #PointCloudDialog QLabel[status="error"] {
            color: #ff9292;
            background: #301c22;
            border-color: #7a3d47;
        }

        QLabel[exposureState="ok"] { color: #9ee6b0; }
        QLabel[exposureState="caution"] { color: #ffd98a; }
        QLabel[exposureState="warning"] {
            color: #ffb4b4;
            background: #36252a;
            border: 1px solid #684044;
            border-radius: 6px;
            padding: 7px;
        }
        QLabel[role="sliderValue"] {
            color: #d8e7fb;
            background: #111a24;
            border: 1px solid #2f4053;
            border-radius: 4px;
            padding: 3px 6px;
        }
        QLabel[role="sliderValue"]:disabled {
            color: #657184;
            background: #151b23;
            border-color: #28323e;
        }

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
        QSlider::groove:horizontal:disabled { background: #252f3c; }
        QSlider::sub-page:horizontal:disabled { background: #47566a; }
        QSlider::handle:horizontal:disabled { background: #768295; border-color: #47566a; }

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

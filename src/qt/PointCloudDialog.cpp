#include "PointCloudDialog.h"

#include "PointCloudWidget.h"
#include "PointCloudDeviationDialog.h"
#include "PointCloudSectionDialog.h"
#include "pointcloud/PointCloudDeviationDistribution.h"
#include "pointcloud/PointCloudIO.h"
#include "pointcloud/PointCloudMeasurement.h"
#include "pointcloud/PointCloudMetrology.h"
#include "pointcloud/PointCloudProcessor.h"
#include "pointcloud/PointCloudSection.h"

#include <QAction>
#include <QButtonGroup>
#include <QBuffer>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImageWriter>
#include <QInputDialog>
#include <QLabel>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMenu>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QSignalBlocker>
#include <QSet>
#include <QScrollArea>
#include <QSplitter>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStyle>
#include <QTabWidget>
#include <QToolButton>
#include <QTextStream>
#include <QStringConverter>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <memory>

namespace {

QString errorText(const std::wstring& value)
{
    return QString::fromStdWString(value);
}

struct PointCloudLoadResult {
    PointCloud cloud;
    std::wstring error;
    bool loaded = false;
    bool cancelled = false;
};

QDoubleSpinBox* coordinateSpin(const QString& name)
{
    auto* spin = new QDoubleSpinBox;
    spin->setObjectName(name);
    spin->setDecimals(6);
    spin->setRange(-1e12, 1e12);
    spin->setMinimumWidth(88);
    spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    return spin;
}

QWidget* rowOf(std::initializer_list<QWidget*> widgets)
{
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    for (QWidget* widget : widgets) layout->addWidget(widget);
    return row;
}

void setInlineStatus(QLabel* label, const QString& text, const QString& status)
{
    if (!label) return;
    label->setText(text);
    label->setProperty("status", status);
    label->style()->unpolish(label);
    label->style()->polish(label);
}

QString htmlEscape(QString value)
{
    return value.replace('&', QStringLiteral("&amp;"))
        .replace('<', QStringLiteral("&lt;"))
        .replace('>', QStringLiteral("&gt;"))
        .replace('"', QStringLiteral("&quot;"));
}

QString sectionFeatureLabel(PointCloudSectionFeatureType type)
{
    switch (type) {
    case PointCloudSectionFeatureType::Step: return QObject::tr("台阶");
    case PointCloudSectionFeatureType::Groove: return QObject::tr("沟槽");
    case PointCloudSectionFeatureType::Peak: return QObject::tr("峰值");
    }
    return {};
}

void setGroupExpanded(QGroupBox* group, bool expanded)
{
    if (!group) return;
    for (QWidget* child : group->findChildren<QWidget*>(
             QString(), Qt::FindDirectChildrenOnly)) {
        child->setVisible(expanded);
    }
    group->setMinimumHeight(expanded ? 0 : 38);
    group->setMaximumHeight(expanded ? QWIDGETSIZE_MAX : 38);
    group->setProperty("expanded", expanded);
    group->style()->unpolish(group);
    group->style()->polish(group);
}

void configureAccordion(
    QObject* owner,
    const QString& settingsKey,
    const QList<QGroupBox*>& groups,
    int defaultIndex)
{
    if (groups.isEmpty()) return;
    QSettings settings;
    settings.beginGroup(QStringLiteral("PointCloudWorkspace/accordion"));
    const bool automatedTest = QCoreApplication::applicationName().contains(
        QStringLiteral("Test"), Qt::CaseInsensitive);
    const int restored = automatedTest
        ? defaultIndex
        : std::clamp(settings.value(settingsKey, defaultIndex).toInt(),
              0, static_cast<int>(groups.size()) - 1);
    settings.endGroup();

    for (int index = 0; index < groups.size(); ++index) {
        QGroupBox* group = groups[index];
        group->setCheckable(true);
        group->setChecked(index == restored);
        setGroupExpanded(group, index == restored);
        QObject::connect(group, &QGroupBox::toggled, owner,
            [groups, group, settingsKey](bool checked) {
                if (!checked) {
                    setGroupExpanded(group, false);
                    return;
                }
                for (QGroupBox* other : groups) {
                    if (other == group) continue;
                    const QSignalBlocker blocker(other);
                    other->setChecked(false);
                    setGroupExpanded(other, false);
                }
                setGroupExpanded(group, true);
                QSettings settings;
                settings.beginGroup(QStringLiteral("PointCloudWorkspace/accordion"));
                settings.setValue(settingsKey, groups.indexOf(group));
                settings.endGroup();
            });
    }
}

QToolButton* toolbarButton(QAction* action, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setAutoRaise(false);
    button->setFocusPolicy(Qt::StrongFocus);
    button->setAccessibleName(action->text());
    return button;
}

void bindPushButton(QPushButton* button, QAction* action)
{
    button->setText(action->text());
    button->setIcon(action->icon());
    button->setToolTip(action->toolTip());
    button->setCheckable(action->isCheckable());
    button->setChecked(action->isChecked());
    button->setEnabled(action->isEnabled());
    QObject::connect(button, &QPushButton::clicked, action, &QAction::trigger);
    QObject::connect(action, &QAction::changed, button, [button, action] {
        const QSignalBlocker blocker(button);
        button->setEnabled(action->isEnabled());
        button->setChecked(action->isChecked());
        button->setIcon(action->icon());
        button->setToolTip(action->toolTip());
    });
}

} // namespace

PointCloudDialog::PointCloudDialog(QWidget* parent) : QDialog(parent)
{
    buildUi();
}

PointCloudDialog::PointCloudDialog(const PointCloud& cloud, QWidget* parent)
    : QDialog(parent)
{
    buildUi();
    setCloud(cloud);
}

PointCloudDialog::~PointCloudDialog()
{
    if (active_task_cancel_token_) {
        active_task_cancel_token_->store(true, std::memory_order_relaxed);
    }
    if (section_cancel_token_) {
        section_cancel_token_->store(true, std::memory_order_relaxed);
    }
    saveSettings();
}

void PointCloudDialog::buildUi()
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("3D 点云工作台"));
    setObjectName(QStringLiteral("PointCloudDialog"));
    resize(1280, 820);
    setMinimumSize(960, 620);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    open_action_ = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton),
        tr("打开"), this);
    open_action_->setObjectName(QStringLiteral("PointCloudOpenAction"));
    open_action_->setShortcut(QKeySequence::Open);
    open_action_->setToolTip(tr("打开 H3D、PLY、PCD 或文本点云（Ctrl+O）"));
    browse_action_ = new QAction(style()->standardIcon(QStyle::SP_DesktopIcon),
        tr("浏览"), this);
    browse_action_->setObjectName(QStringLiteral("PointCloudBrowseAction"));
    browse_action_->setCheckable(true);
    browse_action_->setChecked(true);
    browse_action_->setToolTip(tr("浏览模式：左键旋转、右键平移、滚轮缩放"));
    select_action_ = new QAction(style()->standardIcon(QStyle::SP_FileDialogListView),
        tr("自由选择"), this);
    select_action_->setObjectName(QStringLiteral("PointCloudSelectAction"));
    select_action_->setCheckable(true);
    select_action_->setToolTip(tr("绘制自由选区；Shift 添加，Ctrl 移除"));
    clear_selection_action_ = new QAction(
        style()->standardIcon(QStyle::SP_DialogResetButton), tr("清除选择"), this);
    clear_selection_action_->setObjectName(QStringLiteral("PointCloudClearSelectionAction"));
    clear_selection_action_->setToolTip(tr("清除当前选区，不影响已有模型和测量结果"));
    undo_action_ = new QAction(style()->standardIcon(QStyle::SP_ArrowBack), tr("撤销"), this);
    undo_action_->setObjectName(QStringLiteral("PointCloudUndoAction"));
    undo_action_->setShortcut(QKeySequence::Undo);
    undo_action_->setToolTip(tr("撤销上一次点云处理（Ctrl+Z）"));
    redo_action_ = new QAction(style()->standardIcon(QStyle::SP_ArrowForward), tr("重做"), this);
    redo_action_->setObjectName(QStringLiteral("PointCloudRedoAction"));
    redo_action_->setShortcut(QKeySequence::Redo);
    redo_action_->setToolTip(tr("重做点云处理（Ctrl+Y）"));
    section_action_ = new QAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView),
        tr("绘制剖线"), this);
    section_action_->setObjectName(QStringLiteral("PointCloudSectionAction"));
    section_action_->setToolTip(tr("在点云表面绘制一条持久化三维剖线"));

    auto* toolbar = new QWidget;
    toolbar->setObjectName(QStringLiteral("PointCloudWorkspaceToolbar"));
    auto* toolbar_layout = new QHBoxLayout(toolbar);
    toolbar_layout->setContentsMargins(6, 4, 6, 4);
    toolbar_layout->setSpacing(4);
    toolbar_open_button_ = toolbarButton(open_action_, toolbar);
    toolbar_open_button_->setObjectName(QStringLiteral("PointCloudToolbarOpenButton"));
    navigation_button_ = new QPushButton;
    navigation_button_->setObjectName(QStringLiteral("PointCloudWorkspaceNavigateButton"));
    bindPushButton(navigation_button_, browse_action_);
    free_selection_button_ = new QPushButton;
    free_selection_button_->setObjectName(QStringLiteral("PointCloudFreeSelectionButton"));
    free_selection_button_->setProperty("requiresCloud", true);
    bindPushButton(free_selection_button_, select_action_);
    auto* workspace_mode_group = new QButtonGroup(this);
    workspace_mode_group->setExclusive(true);
    workspace_mode_group->addButton(navigation_button_);
    workspace_mode_group->addButton(free_selection_button_);
    clear_selection_button_ = new QPushButton;
    clear_selection_button_->setObjectName(QStringLiteral("PointCloudClearSelectionButton"));
    bindPushButton(clear_selection_button_, clear_selection_action_);
    undo_button_ = new QPushButton;
    undo_button_->setObjectName(QStringLiteral("PointCloudUndoButton"));
    bindPushButton(undo_button_, undo_action_);
    redo_button_ = new QPushButton;
    redo_button_->setObjectName(QStringLiteral("PointCloudRedoButton"));
    bindPushButton(redo_button_, redo_action_);

    toolbar_fit_button_ = new QToolButton(toolbar);
    toolbar_fit_button_->setObjectName(QStringLiteral("PointCloudToolbarFitButton"));
    toolbar_fit_button_->setText(tr("拟合"));
    toolbar_fit_button_->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    toolbar_fit_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar_fit_button_->setPopupMode(QToolButton::InstantPopup);
    toolbar_fit_button_->setProperty("requiresCloud", true);
    auto* fit_menu = new QMenu(toolbar_fit_button_);
    auto* quick_plane = fit_menu->addAction(tr("平面拟合"));
    auto* quick_sphere = fit_menu->addAction(tr("球拟合"));
    auto* quick_cylinder = fit_menu->addAction(tr("圆柱拟合"));
    toolbar_fit_button_->setMenu(fit_menu);

    toolbar_measure_button_ = new QToolButton(toolbar);
    toolbar_measure_button_->setObjectName(QStringLiteral("PointCloudToolbarMeasureButton"));
    toolbar_measure_button_->setText(tr("测量"));
    toolbar_measure_button_->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    toolbar_measure_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar_measure_button_->setPopupMode(QToolButton::InstantPopup);
    toolbar_measure_button_->setProperty("requiresCloud", true);
    auto* measure_menu = new QMenu(toolbar_measure_button_);
    const QList<QPair<QString, PointCloudMeasureMode>> quick_measurements{
        {tr("点坐标"), PointCloudMeasureMode::Point},
        {tr("三维距离"), PointCloudMeasureMode::Distance},
        {tr("高度差"), PointCloudMeasureMode::HeightDifference},
        {tr("三点角度"), PointCloudMeasureMode::Angle},
        {tr("点到参考平面"), PointCloudMeasureMode::PointToPlane}};
    for (const auto& command : quick_measurements) {
        QAction* action = measure_menu->addAction(command.first);
        connect(action, &QAction::triggered, this, [this, mode = command.second] {
            setWorkspacePage(PointCloudWorkspacePage::GeometryMeasurement);
            if (auto* group = findChild<QGroupBox*>(QStringLiteral("PointCloudMeasurementGroup"))) {
                group->setChecked(true);
            }
            setMeasureMode(mode);
        });
    }
    toolbar_measure_button_->setMenu(measure_menu);
    toolbar_section_button_ = toolbarButton(section_action_, toolbar);
    toolbar_section_button_->setObjectName(QStringLiteral("PointCloudToolbarSectionButton"));
    toolbar_section_button_->setProperty("requiresCloud", true);

    toolbar_view_button_ = new QToolButton(toolbar);
    toolbar_view_button_->setObjectName(QStringLiteral("PointCloudToolbarViewButton"));
    toolbar_view_button_->setText(tr("视图"));
    toolbar_view_button_->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    toolbar_view_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar_view_button_->setPopupMode(QToolButton::InstantPopup);
    auto* view_menu = new QMenu(toolbar_view_button_);
    const QList<QPair<QString, PointCloudViewPreset>> view_commands{
        {tr("等轴测"), PointCloudViewPreset::Isometric},
        {tr("俯视"), PointCloudViewPreset::Top},
        {tr("前视"), PointCloudViewPreset::Front},
        {tr("右视"), PointCloudViewPreset::Right}};
    for (const auto& command : view_commands) {
        QAction* action = view_menu->addAction(command.first);
        connect(action, &QAction::triggered, this, [this, preset = command.second] {
            cloud_widget_->setViewPreset(preset);
            if (view_preset_combo_) {
                const int index = view_preset_combo_->findData(static_cast<int>(preset));
                if (index >= 0) view_preset_combo_->setCurrentIndex(index);
            }
        });
    }
    view_menu->addSeparator();
    QAction* reset_view_action = view_menu->addAction(tr("复位视角"));
    toolbar_view_button_->setMenu(view_menu);

    view_preset_combo_ = new QComboBox;
    view_preset_combo_->setObjectName(QStringLiteral("PointCloudViewPresetCombo"));
    view_preset_combo_->setToolTip(tr("切换工业检测常用标准视角"));
    view_preset_combo_->addItem(tr("等轴测"), static_cast<int>(PointCloudViewPreset::Isometric));
    view_preset_combo_->addItem(tr("俯视"), static_cast<int>(PointCloudViewPreset::Top));
    view_preset_combo_->addItem(tr("前视"), static_cast<int>(PointCloudViewPreset::Front));
    view_preset_combo_->addItem(tr("右视"), static_cast<int>(PointCloudViewPreset::Right));
    selection_status_ = new QLabel(tr("选择 0"));
    selection_status_->setObjectName(QStringLiteral("PointCloudSelectionStatus"));
    workspace_status_ = new QLabel(tr("未载入点云"));
    workspace_status_->setObjectName(QStringLiteral("PointCloudWorkspaceStatus"));
    toolbar_layout->addWidget(toolbar_open_button_);
    toolbar_layout->addWidget(navigation_button_);
    toolbar_layout->addWidget(free_selection_button_);
    toolbar_layout->addWidget(clear_selection_button_);
    toolbar_layout->addWidget(undo_button_);
    toolbar_layout->addWidget(redo_button_);
    toolbar_layout->addSpacing(4);
    toolbar_layout->addWidget(toolbar_fit_button_);
    toolbar_layout->addWidget(toolbar_measure_button_);
    toolbar_layout->addWidget(toolbar_section_button_);
    toolbar_layout->addWidget(toolbar_view_button_);
    toolbar_layout->addStretch();
    root->addWidget(toolbar);

    data_summary_bar_ = new QFrame;
    data_summary_bar_->setObjectName(QStringLiteral("PointCloudDataSummary"));
    auto* summary_layout = new QHBoxLayout(data_summary_bar_);
    summary_layout->setContentsMargins(10, 4, 10, 4);
    summary_layout->setSpacing(10);
    source_label_ = new QLabel(tr("尚未载入点云"));
    source_label_->setObjectName(QStringLiteral("PointCloudSourceLabel"));
    source_label_->setProperty("role", QStringLiteral("summary"));
    source_label_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    statistics_label_ = new QLabel(tr("点数 0"));
    statistics_label_->setObjectName(QStringLiteral("PointCloudStatistics"));
    texture_status_label_ = new QLabel(tr("等待载入数据"));
    texture_status_label_->setObjectName(QStringLiteral("PointCloudTextureStatus"));
    texture_status_label_->setProperty("status", QStringLiteral("neutral"));
    summary_layout->addWidget(source_label_, 2);
    summary_layout->addWidget(statistics_label_);
    summary_layout->addWidget(texture_status_label_);
    summary_layout->addWidget(selection_status_);
    summary_layout->addWidget(workspace_status_, 1);
    root->addWidget(data_summary_bar_);

    task_bar_ = new QFrame;
    task_bar_->setObjectName(QStringLiteral("PointCloudTaskBar"));
    auto* task_layout = new QHBoxLayout(task_bar_);
    task_layout->setContentsMargins(10, 4, 6, 4);
    task_layout->setSpacing(8);
    task_label_ = new QLabel(tr("任务就绪"));
    task_label_->setObjectName(QStringLiteral("PointCloudTaskLabel"));
    task_progress_ = new QProgressBar;
    task_progress_->setObjectName(QStringLiteral("PointCloudTaskProgress"));
    task_progress_->setTextVisible(false);
    task_progress_->setRange(0, 0);
    task_cancel_button_ = new QToolButton;
    task_cancel_button_->setObjectName(QStringLiteral("PointCloudTaskCancelButton"));
    task_cancel_button_->setText(tr("取消"));
    task_cancel_button_->setToolTip(tr("取消当前后台任务"));
    task_layout->addWidget(task_label_);
    task_layout->addWidget(task_progress_, 1);
    task_layout->addWidget(task_cancel_button_);
    task_bar_->setVisible(false);
    root->addWidget(task_bar_);

    workspace_splitter_ = new QSplitter(Qt::Horizontal);
    workspace_splitter_->setObjectName(QStringLiteral("PointCloudWorkspaceSplitter"));
    workspace_splitter_->setChildrenCollapsible(false);
    cloud_widget_ = new PointCloudWidget;
    view_section_splitter_ = new QSplitter(Qt::Vertical);
    view_section_splitter_->setObjectName(QStringLiteral("PointCloudViewSectionSplitter"));
    view_section_splitter_->addWidget(cloud_widget_);
    section_workspace_ = new QFrame;
    section_workspace_->setObjectName(QStringLiteral("PointCloudSectionWorkspace"));
    auto* section_workspace_layout = new QVBoxLayout(section_workspace_);
    section_workspace_layout->setContentsMargins(8, 6, 8, 6);
    section_workspace_layout->setSpacing(5);
    auto* section_workspace_toolbar = new QHBoxLayout;
    auto* section_workspace_title = new QLabel(tr("剖线轮廓"));
    section_workspace_title->setProperty("role", QStringLiteral("summary"));
    section_cursor_status_ = new QLabel(tr("游标 A/B：—"));
    export_section_csv_button_ = new QPushButton(tr("CSV"));
    export_section_csv_button_->setObjectName(QStringLiteral("PointCloudSectionCsvButton"));
    export_section_png_button_ = new QPushButton(tr("PNG"));
    export_section_png_button_->setObjectName(QStringLiteral("PointCloudSectionPngButton"));
    export_section_report_button_ = new QPushButton(tr("报告…"));
    export_section_report_button_->setObjectName(QStringLiteral("PointCloudSectionReportButton"));
    section_workspace_toolbar->addWidget(section_workspace_title);
    section_workspace_toolbar->addWidget(section_cursor_status_, 1);
    section_workspace_toolbar->addWidget(export_section_csv_button_);
    section_workspace_toolbar->addWidget(export_section_png_button_);
    section_workspace_toolbar->addWidget(export_section_report_button_);
    section_workspace_layout->addLayout(section_workspace_toolbar);
    section_plot_ = new PointCloudSectionPlotWidget;
    section_workspace_layout->addWidget(section_plot_, 1);
    view_section_splitter_->addWidget(section_workspace_);
    view_section_splitter_->setStretchFactor(0, 1);
    view_section_splitter_->setStretchFactor(1, 0);
    view_section_splitter_->setSizes({650, 0});
    workspace_splitter_->addWidget(view_section_splitter_);

    side_panel_ = new QWidget;
    side_panel_->setObjectName(QStringLiteral("PointCloudInspector"));
    side_panel_->setMinimumWidth(336);
    side_panel_->setMaximumWidth(480);
    auto* side_layout = new QVBoxLayout(side_panel_);
    side_layout->setContentsMargins(0, 0, 0, 0);
    auto* side_header = new QFrame;
    side_header->setObjectName(QStringLiteral("PointCloudInspectorHeader"));
    auto* side_header_layout = new QHBoxLayout(side_header);
    side_header_layout->setContentsMargins(10, 4, 6, 4);
    task_page_title_ = new QLabel(tr("数据与显示"));
    task_page_title_->setObjectName(QStringLiteral("PointCloudTaskPageTitle"));
    task_page_title_->setProperty("role", QStringLiteral("summary"));
    drawer_toggle_button_ = new QToolButton;
    drawer_toggle_button_->setObjectName(QStringLiteral("PointCloudDrawerToggleButton"));
    drawer_toggle_button_->setText(tr("收起"));
    drawer_toggle_button_->setToolTip(tr("展开或收起工具抽屉"));
    drawer_toggle_button_->setVisible(false);
    side_header_layout->addWidget(task_page_title_, 1);
    side_header_layout->addWidget(drawer_toggle_button_);
    side_layout->addWidget(side_header);

    compact_drawer_tabs_ = new QTabWidget;
    compact_drawer_tabs_->setObjectName(QStringLiteral("PointCloudCompactDrawerTabs"));
    compact_drawer_tabs_->setTabBarAutoHide(true);
    tool_panel_host_ = new QWidget;
    auto* tool_host_layout = new QVBoxLayout(tool_panel_host_);
    tool_host_layout->setContentsMargins(0, 0, 0, 0);
    tabs_ = new QTabWidget;
    tabs_->setObjectName(QStringLiteral("PointCloudToolTabs"));
    tabs_->setUsesScrollButtons(false);
    tool_host_layout->addWidget(tabs_);
    compact_drawer_tabs_->addTab(tool_panel_host_, tr("工具"));
    side_layout->addWidget(compact_drawer_tabs_, 1);

    auto* data_scroll = new QScrollArea;
    data_scroll->setObjectName(QStringLiteral("PointCloudDataScrollArea"));
    data_scroll->setWidgetResizable(true);
    data_scroll->setFrameShape(QFrame::NoFrame);
    data_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* data_page = new QWidget;
    auto* data_layout = new QVBoxLayout(data_page);
    open_button_ = new QPushButton(tr("打开点云…"));
    open_button_->setObjectName(QStringLiteral("PointCloudOpenButton"));
    open_button_->setProperty("role", QStringLiteral("primary"));
    auto* export_button = new QPushButton(tr("导出处理结果…"));
    export_button->setObjectName(QStringLiteral("PointCloudExportButton"));
    data_layout->addWidget(rowOf({open_button_, export_button}));
    auto* display_group = new QGroupBox(tr("显示与纹理"));
    display_group->setObjectName(QStringLiteral("PointCloudDisplayGroup"));
    auto* display_form = new QFormLayout(display_group);
    unit_combo_ = new QComboBox;
    unit_combo_->setObjectName(QStringLiteral("PointCloudUnitCombo"));
    unit_combo_->addItem(tr("无单位"), static_cast<int>(PointCloudUnit::Unknown));
    unit_combo_->addItem(QStringLiteral("µm"), static_cast<int>(PointCloudUnit::Micrometers));
    unit_combo_->addItem(QStringLiteral("mm"), static_cast<int>(PointCloudUnit::Millimeters));
    unit_combo_->addItem(QStringLiteral("m"), static_cast<int>(PointCloudUnit::Meters));
    color_combo_ = new QComboBox;
    color_combo_->setObjectName(QStringLiteral("PointCloudColorCombo"));
    color_combo_->addItem(tr("高度伪彩"), static_cast<int>(PointCloudColorMode::Height));
    color_combo_->addItem(tr("原始颜色"), static_cast<int>(PointCloudColorMode::Original));
    color_combo_->addItem(tr("统一颜色"), static_cast<int>(PointCloudColorMode::Solid));
    color_combo_->addItem(tr("H3D 纹理表面"), static_cast<int>(PointCloudColorMode::Texture));
    texture_enhance_check_ = new QCheckBox(tr("增强纹理细节"));
    texture_enhance_check_->setObjectName(QStringLiteral("PointCloudTextureEnhancementCheck"));
    texture_enhance_check_->setChecked(true);
    texture_enhance_check_->setToolTip(
        tr("提升偏暗灰度纹理的中间调与局部对比度；关闭后显示文件原始纹理"));
    point_size_spin_ = new QDoubleSpinBox;
    point_size_spin_->setObjectName(QStringLiteral("PointCloudPointSize"));
    point_size_spin_->setRange(1.0, 12.0);
    point_size_spin_->setValue(2.5);
    point_size_spin_->setSuffix(tr(" px"));
    axes_check_ = new QCheckBox(tr("显示 XYZ 坐标轴"));
    axes_check_->setObjectName(QStringLiteral("PointCloudAxesCheck"));
    axes_check_->setChecked(true);
    backend_label_ = new QLabel(cloud_widget_->renderBackend());
    backend_label_->setObjectName(QStringLiteral("PointCloudRenderBackend"));
    backend_label_->setWordWrap(true);
    display_form->addRow(tr("坐标单位"), unit_combo_);
    display_form->addRow(tr("渲染"), color_combo_);
    display_form->addRow({}, texture_enhance_check_);
    display_form->addRow(tr("点大小"), point_size_spin_);
    display_form->addRow({}, axes_check_);
    display_form->addRow(tr("标准视角"), view_preset_combo_);
    display_form->addRow(tr("渲染后端"), backend_label_);
    data_layout->addWidget(display_group);
    auto* reset_view = new QPushButton(tr("复位视角"));
    reset_view->setObjectName(QStringLiteral("PointCloudResetViewButton"));
    data_layout->addWidget(reset_view);
    data_layout->addStretch();
    data_scroll->setWidget(data_page);
    tabs_->addTab(data_scroll, style()->standardIcon(QStyle::SP_FileDialogInfoView), tr("数据"));
    tabs_->setTabToolTip(0, tr("数据与显示"));

    auto* process_page = new QScrollArea;
    process_page->setObjectName(QStringLiteral("PointCloudProcessingScrollArea"));
    process_page->setWidgetResizable(true);
    process_page->setFrameShape(QFrame::NoFrame);
    process_page->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* process_content = new QWidget;
    auto* process_layout = new QVBoxLayout(process_content);
    auto* interactive_crop_group = new QGroupBox(tr("通用选择与裁剪"));
    interactive_crop_group->setObjectName(QStringLiteral("PointCloudSelectionGroup"));
    auto* interactive_crop_layout = new QVBoxLayout(interactive_crop_group);
    crop_selection_label_ = new QLabel(tr("尚未选择点"));
    crop_selection_label_->setObjectName(QStringLiteral("PointCloudCropSelectionLabel"));
    crop_selection_label_->setWordWrap(true);
    begin_crop_button_ = new QPushButton(tr("在视图中自由选择"));
    begin_crop_button_->setObjectName(QStringLiteral("PointCloudBeginInteractiveCropButton"));
    begin_crop_button_->setCheckable(true);
    keep_crop_button_ = new QPushButton(tr("保留框内"));
    keep_crop_button_->setObjectName(QStringLiteral("PointCloudKeepSelectionButton"));
    remove_crop_button_ = new QPushButton(tr("保留框外"));
    remove_crop_button_->setObjectName(QStringLiteral("PointCloudRemoveSelectionButton"));
    keep_crop_button_->setEnabled(false);
    remove_crop_button_->setEnabled(false);
    interactive_crop_layout->addWidget(crop_selection_label_);
    interactive_crop_layout->addWidget(begin_crop_button_);
    interactive_crop_layout->addWidget(rowOf({keep_crop_button_, remove_crop_button_}));
    process_layout->addWidget(interactive_crop_group);
    auto* smart_filter_group = new QGroupBox(tr("智能滤波与去噪"));
    smart_filter_group->setObjectName(QStringLiteral("PointCloudSmartFilterGroup"));
    auto* smart_filter_form = new QFormLayout(smart_filter_group);
    smart_radius_spin_ = coordinateSpin(QStringLiteral("PointCloudSmartFilterRadius"));
    smart_radius_spin_->setRange(0.000001, 1e12);
    smart_neighbors_spin_ = new QSpinBox;
    smart_neighbors_spin_->setObjectName(QStringLiteral("PointCloudSmartFilterNeighbors"));
    smart_neighbors_spin_->setRange(2, 100);
    smart_neighbors_spin_->setValue(4);
    smart_sigma_spin_ = new QDoubleSpinBox;
    smart_sigma_spin_->setObjectName(QStringLiteral("PointCloudSmartFilterSigma"));
    smart_sigma_spin_->setRange(1.0, 10.0);
    smart_sigma_spin_->setDecimals(2);
    smart_sigma_spin_->setValue(3.5);
    smart_deviation_spin_ = coordinateSpin(QStringLiteral("PointCloudSmartFilterDeviation"));
    smart_deviation_spin_->setRange(0.0, 1e12);
    smart_smoothing_spin_ = new QDoubleSpinBox;
    smart_smoothing_spin_->setObjectName(QStringLiteral("PointCloudSmartFilterSmoothing"));
    smart_smoothing_spin_->setRange(0.0, 1.0);
    smart_smoothing_spin_->setSingleStep(0.05);
    smart_smoothing_spin_->setValue(0.25);
    auto* smart_filter_apply = new QPushButton(tr("快速去除飞点与毛刺"));
    smart_filter_apply->setObjectName(QStringLiteral("PointCloudSmartFilterApplyButton"));
    smart_filter_apply->setProperty("role", QStringLiteral("primary"));
    smart_filter_report_ = new QLabel(tr("自动估计间距后可调整参数。"));
    smart_filter_report_->setObjectName(QStringLiteral("PointCloudSmartFilterReport"));
    smart_filter_report_->setWordWrap(true);
    smart_filter_form->addRow(tr("邻域半径"), smart_radius_spin_);
    smart_filter_form->addRow(tr("最少邻点"), smart_neighbors_spin_);
    smart_filter_form->addRow(tr("毛刺灵敏度 σ"), smart_sigma_spin_);
    smart_filter_form->addRow(tr("保边高度阈值"), smart_deviation_spin_);
    smart_filter_form->addRow(tr("平滑强度"), smart_smoothing_spin_);
    smart_filter_form->addRow({}, smart_filter_apply);
    smart_filter_form->addRow({}, smart_filter_report_);
    process_layout->addWidget(smart_filter_group);

    auto* hole_repair_group = new QGroupBox(tr("死角与空洞修复"));
    hole_repair_group->setObjectName(QStringLiteral("PointCloudHoleRepairGroup"));
    auto* hole_repair_form = new QFormLayout(hole_repair_group);
    repair_spacing_spin_ = coordinateSpin(QStringLiteral("PointCloudRepairGridSpacing"));
    repair_spacing_spin_->setRange(0.000001, 1e12);
    repair_max_cells_spin_ = new QSpinBox;
    repair_max_cells_spin_->setObjectName(QStringLiteral("PointCloudRepairMaxHoleCells"));
    repair_max_cells_spin_->setRange(1, 10000);
    repair_max_cells_spin_->setValue(64);
    repair_search_spin_ = new QSpinBox;
    repair_search_spin_->setObjectName(QStringLiteral("PointCloudRepairSearchRadius"));
    repair_search_spin_->setRange(2, 20);
    repair_search_spin_->setValue(5);
    auto* hole_repair_apply = new QPushButton(tr("自动检测并平滑填补"));
    hole_repair_apply->setObjectName(QStringLiteral("PointCloudHoleRepairApplyButton"));
    hole_repair_apply->setProperty("role", QStringLiteral("primary"));
    hole_repair_report_ = new QLabel(tr("仅修复被有效点包围的内部空洞。"));
    hole_repair_report_->setObjectName(QStringLiteral("PointCloudHoleRepairReport"));
    hole_repair_report_->setWordWrap(true);
    hole_repair_form->addRow(tr("补点网格间距"), repair_spacing_spin_);
    hole_repair_form->addRow(tr("最大空洞格数"), repair_max_cells_spin_);
    hole_repair_form->addRow(tr("趋势搜索半径"), repair_search_spin_);
    hole_repair_form->addRow({}, hole_repair_apply);
    hole_repair_form->addRow({}, hole_repair_report_);
    process_layout->addWidget(hole_repair_group);
    auto* voxel_group = new QGroupBox(tr("体素降采样"));
    voxel_group->setObjectName(QStringLiteral("PointCloudVoxelGroup"));
    auto* voxel_form = new QFormLayout(voxel_group);
    voxel_spin_ = coordinateSpin(QStringLiteral("PointCloudVoxelSize"));
    voxel_spin_->setRange(0.000001, 1e12);
    voxel_spin_->setValue(1.0);
    auto* voxel_apply = new QPushButton(tr("应用降采样"));
    voxel_apply->setObjectName(QStringLiteral("PointCloudVoxelApplyButton"));
    voxel_form->addRow(tr("体素大小"), voxel_spin_);
    voxel_form->addRow({}, voxel_apply);
    process_layout->addWidget(voxel_group);
    auto* outlier_group = new QGroupBox(tr("半径离群点过滤"));
    outlier_group->setObjectName(QStringLiteral("PointCloudOutlierGroup"));
    auto* outlier_form = new QFormLayout(outlier_group);
    outlier_radius_spin_ = coordinateSpin(QStringLiteral("PointCloudOutlierRadius"));
    outlier_radius_spin_->setRange(0.000001, 1e12);
    outlier_radius_spin_->setValue(2.0);
    outlier_neighbors_spin_ = new QSpinBox;
    outlier_neighbors_spin_->setObjectName(QStringLiteral("PointCloudOutlierNeighbors"));
    outlier_neighbors_spin_->setRange(1, 1000);
    outlier_neighbors_spin_->setValue(3);
    auto* outlier_apply = new QPushButton(tr("移除离群点"));
    outlier_apply->setObjectName(QStringLiteral("PointCloudOutlierApplyButton"));
    outlier_form->addRow(tr("搜索半径"), outlier_radius_spin_);
    outlier_form->addRow(tr("最少邻点"), outlier_neighbors_spin_);
    outlier_form->addRow({}, outlier_apply);
    process_layout->addWidget(outlier_group);
    auto* plane_group = new QGroupBox(tr("平面拟合与校平"));
    plane_group->setObjectName(QStringLiteral("PointCloudLevelGroup"));
    auto* plane_layout = new QVBoxLayout(plane_group);
    plane_label_ = new QLabel(tr("尚未拟合参考平面"));
    plane_label_->setObjectName(QStringLiteral("PointCloudPlaneLabel"));
    plane_label_->setWordWrap(true);
    auto* fit_button = new QPushButton(tr("拟合参考平面"));
    fit_button->setObjectName(QStringLiteral("PointCloudFitPlaneButton"));
    level_button_ = new QPushButton(tr("按参考平面校平"));
    level_button_->setObjectName(QStringLiteral("PointCloudLevelButton"));
    level_button_->setEnabled(false);
    show_plane_check_ = new QCheckBox(tr("在 3D 视图中显示拟合平面"));
    show_plane_check_->setObjectName(QStringLiteral("PointCloudShowFittedPlaneCheck"));
    show_plane_check_->setChecked(true);
    show_plane_check_->setEnabled(false);
    plane_layout->addWidget(plane_label_);
    plane_layout->addWidget(show_plane_check_);
    plane_layout->addWidget(rowOf({fit_button, level_button_}));
    process_layout->addWidget(plane_group);
    auto* restore_button = new QPushButton(tr("恢复原始点云"));
    restore_button->setObjectName(QStringLiteral("PointCloudRestoreButton"));
    process_layout->addWidget(restore_button);
    process_layout->addStretch();
    process_page->setWidget(process_content);
    configureAccordion(this, QStringLiteral("processing"),
        {interactive_crop_group, smart_filter_group, hole_repair_group,
            voxel_group, outlier_group, plane_group}, 0);
    tabs_->addTab(process_page, style()->standardIcon(QStyle::SP_BrowserReload), tr("处理"));
    tabs_->setTabToolTip(1, tr("清理与处理"));

    auto* fit_page = new QScrollArea;
    fit_page->setObjectName(QStringLiteral("PointCloudFitScrollArea"));
    fit_page->setWidgetResizable(true);
    fit_page->setFrameShape(QFrame::NoFrame);
    fit_page->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* fit_content = new QWidget;
    auto* fit_layout = new QVBoxLayout(fit_content);
    auto* fit_group = new QGroupBox(tr("几何拟合"));
    fit_group->setObjectName(QStringLiteral("PointCloudFitGroup"));
    auto* fit_form = new QFormLayout(fit_group);
    fit_scope_combo_ = new QComboBox;
    fit_scope_combo_->setObjectName(QStringLiteral("PointCloudFitScopeCombo"));
    fit_scope_combo_->addItem(tr("整个点云"), static_cast<int>(PointCloudFitScope::WholeCloud));
    fit_scope_combo_->addItem(tr("当前选择"), static_cast<int>(PointCloudFitScope::Selection));
    fit_threshold_spin_ = coordinateSpin(QStringLiteral("PointCloudFitThreshold"));
    fit_threshold_spin_->setRange(0.0, 1e12);
    fit_threshold_spin_->setSpecialValueText(tr("自动"));
    cylinder_axis_combo_ = new QComboBox;
    cylinder_axis_combo_->addItem(tr("自由轴向"), static_cast<int>(PointCloudCylinderAxisConstraint::Free));
    cylinder_axis_combo_->addItem(QStringLiteral("X"), static_cast<int>(PointCloudCylinderAxisConstraint::X));
    cylinder_axis_combo_->addItem(QStringLiteral("Y"), static_cast<int>(PointCloudCylinderAxisConstraint::Y));
    cylinder_axis_combo_->addItem(QStringLiteral("Z"), static_cast<int>(PointCloudCylinderAxisConstraint::Z));
    minimum_radius_spin_ = coordinateSpin(QStringLiteral("PointCloudMinimumRadius"));
    minimum_radius_spin_->setRange(0.0, 1e12);
    minimum_radius_spin_->setSpecialValueText(tr("不限"));
    maximum_radius_spin_ = coordinateSpin(QStringLiteral("PointCloudMaximumRadius"));
    maximum_radius_spin_->setRange(0.0, 1e12);
    maximum_radius_spin_->setSpecialValueText(tr("不限"));
    fit_plane_model_button_ = new QPushButton(tr("拟合平面"));
    fit_sphere_button_ = new QPushButton(tr("拟合球"));
    fit_cylinder_button_ = new QPushButton(tr("拟合圆柱"));
    fit_plane_model_button_->setObjectName(QStringLiteral("PointCloudFitPlaneModelButton"));
    fit_sphere_button_->setObjectName(QStringLiteral("PointCloudFitSphereButton"));
    fit_cylinder_button_->setObjectName(QStringLiteral("PointCloudFitCylinderButton"));
    cancel_fit_button_ = new QPushButton(tr("取消拟合"));
    cancel_fit_button_->setObjectName(QStringLiteral("PointCloudCancelFitButton"));
    cancel_fit_button_->setEnabled(false);
    fit_status_ = new QLabel(tr("选择拟合类型，结果将加入模型列表。"));
    fit_status_->setObjectName(QStringLiteral("PointCloudFitStatus"));
    fit_status_->setWordWrap(true);
    fit_form->addRow(tr("作用范围"), fit_scope_combo_);
    fit_form->addRow(tr("内点阈值"), fit_threshold_spin_);
    fit_form->addRow(tr("圆柱轴向"), cylinder_axis_combo_);
    fit_form->addRow(tr("最小半径"), minimum_radius_spin_);
    fit_form->addRow(tr("最大半径"), maximum_radius_spin_);
    fit_form->addRow({}, rowOf({fit_plane_model_button_, fit_sphere_button_, fit_cylinder_button_}));
    fit_form->addRow({}, cancel_fit_button_);
    fit_form->addRow({}, fit_status_);
    fit_layout->addWidget(fit_group);
    auto* model_group = new QGroupBox(tr("模型管理"));
    model_group->setObjectName(QStringLiteral("PointCloudModelGroup"));
    auto* model_layout = new QVBoxLayout(model_group);
    model_list_ = new QListWidget;
    model_list_->setObjectName(QStringLiteral("PointCloudModelList"));
    model_details_ = new QLabel(tr("尚未创建模型"));
    model_details_->setWordWrap(true);
    residual_coloring_check_ = new QCheckBox(tr("按当前模型显示残差颜色"));
    show_model_button_ = new QPushButton(tr("显示"));
    hide_model_button_ = new QPushButton(tr("隐藏"));
    delete_model_button_ = new QPushButton(tr("删除"));
    clear_models_button_ = new QPushButton(tr("清空模型"));
    show_model_button_->setObjectName(QStringLiteral("PointCloudShowModelButton"));
    hide_model_button_->setObjectName(QStringLiteral("PointCloudHideModelButton"));
    delete_model_button_->setObjectName(QStringLiteral("PointCloudDeleteModelButton"));
    clear_models_button_->setObjectName(QStringLiteral("PointCloudClearModelsButton"));
    model_list_->setToolTip(tr("单击选择模型；双击可快速显示或隐藏模型"));
    model_layout->addWidget(model_list_);
    model_layout->addWidget(model_details_);
    model_layout->addWidget(residual_coloring_check_);
    model_layout->addWidget(rowOf({show_model_button_, hide_model_button_,
        delete_model_button_, clear_models_button_}));
    fit_layout->addWidget(model_group);

    auto* measure_page = new QGroupBox(tr("三维测量"));
    measure_page->setObjectName(QStringLiteral("PointCloudMeasurementGroup"));
    auto* measure_layout = new QVBoxLayout(measure_page);
    measurement_hint_ = new QLabel(tr("选择测量工具后，在点云中点击取点。"));
    measurement_hint_->setObjectName(QStringLiteral("PointCloudMeasurementHint"));
    measurement_hint_->setWordWrap(true);
    measurement_hint_->setProperty("role", QStringLiteral("summary"));
    measure_layout->addWidget(measurement_hint_);
    measurement_tool_group_ = new QButtonGroup(this);
    measurement_tool_group_->setExclusive(true);
    auto* tool_panel = new QWidget;
    tool_panel->setObjectName(QStringLiteral("PointCloudMeasurementToolGrid"));
    auto* tool_layout = new QGridLayout(tool_panel);
    tool_layout->setContentsMargins(0, 0, 0, 0);
    tool_layout->setSpacing(6);
    int tool_index = 0;
    auto addTool = [this, tool_layout, &tool_index](
                       const QString& text, const QString& name,
                       PointCloudMeasureMode mode) {
        auto* button = new QPushButton(text);
        button->setObjectName(name);
        button->setCheckable(true);
        button->setMinimumHeight(36);
        if (mode != PointCloudMeasureMode::Navigate) {
            button->setProperty("requiresCloud", true);
        }
        measurement_tool_group_->addButton(button, static_cast<int>(mode));
        tool_layout->addWidget(button, tool_index / 2, tool_index % 2);
        ++tool_index;
        connect(button, &QPushButton::clicked, this, [this, mode] { setMeasureMode(mode); });
        return button;
    };
    auto* navigate = addTool(tr("浏览/旋转"), QStringLiteral("PointCloudNavigateButton"),
        PointCloudMeasureMode::Navigate);
    navigate->setChecked(true);
    addTool(tr("点坐标"), QStringLiteral("PointCloudPointMeasureButton"),
        PointCloudMeasureMode::Point);
    addTool(tr("三维距离"), QStringLiteral("PointCloudDistanceMeasureButton"),
        PointCloudMeasureMode::Distance);
    addTool(tr("高度差"), QStringLiteral("PointCloudHeightMeasureButton"),
        PointCloudMeasureMode::HeightDifference);
    addTool(tr("三点角度"), QStringLiteral("PointCloudAngleMeasureButton"),
        PointCloudMeasureMode::Angle);
    addTool(tr("点到参考平面"), QStringLiteral("PointCloudPlaneMeasureButton"),
        PointCloudMeasureMode::PointToPlane);
    addTool(tr("两平面夹角（6 点）"), QStringLiteral("PointCloudPlaneAngleButton"),
        PointCloudMeasureMode::PlaneAngle);
    addTool(tr("空间直线交点（4 点）"), QStringLiteral("PointCloudLineIntersectionButton"),
        PointCloudMeasureMode::LineIntersection);
    measure_layout->addWidget(tool_panel);
    measurement_list_ = new QListWidget;
    measurement_list_->setObjectName(QStringLiteral("PointCloudMeasurementList"));
    measurement_list_->setMinimumHeight(120);
    measure_layout->addWidget(measurement_list_, 1);
    delete_measurement_button_ = new QPushButton(tr("删除选中"));
    delete_measurement_button_->setObjectName(QStringLiteral("PointCloudDeleteMeasurementButton"));
    clear_measurements_button_ = new QPushButton(tr("清空测量"));
    clear_measurements_button_->setObjectName(QStringLiteral("PointCloudClearMeasurementsButton"));
    export_measurements_button_ = new QPushButton(tr("导出 CSV…"));
    export_measurements_button_->setObjectName(QStringLiteral("PointCloudExportMeasurementsButton"));
    delete_measurement_button_->setProperty("role", QStringLiteral("danger"));
    clear_measurements_button_->setProperty("role", QStringLiteral("danger"));
    measurement_list_->setToolTip(tr("选择测量结果可在点云中回显对应取样点"));
    measure_layout->addWidget(rowOf({
        delete_measurement_button_, clear_measurements_button_, export_measurements_button_}));
    fit_layout->addWidget(measure_page);
    fit_layout->addStretch();
    fit_page->setWidget(fit_content);
    configureAccordion(this, QStringLiteral("geometry"),
        {fit_group, model_group, measure_page}, 0);
    tabs_->addTab(fit_page, style()->standardIcon(QStyle::SP_FileIcon), tr("测量"));
    tabs_->setTabToolTip(2, tr("几何测量"));

    auto* inspect_page = new QScrollArea;
    inspect_page->setObjectName(QStringLiteral("PointCloudInspectionScrollArea"));
    inspect_page->setWidgetResizable(true);
    inspect_page->setFrameShape(QFrame::NoFrame);
    inspect_page->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* inspect_content = new QWidget;
    auto* inspect_layout = new QVBoxLayout(inspect_content);
    auto* tolerance_group = new QGroupBox(tr("形位公差一键评级"));
    tolerance_group->setObjectName(QStringLiteral("PointCloudToleranceGroup"));
    auto* tolerance_form = new QFormLayout(tolerance_group);
    auto makeTolerance = [](const QString& name) {
        auto* spin = coordinateSpin(name);
        spin->setRange(0.000001, 1e12);
        spin->setValue(0.05);
        return spin;
    };
    flatness_tolerance_ = makeTolerance(QStringLiteral("PointCloudFlatnessTolerance"));
    cylindricity_tolerance_ = makeTolerance(QStringLiteral("PointCloudCylindricityTolerance"));
    circularity_tolerance_ = makeTolerance(QStringLiteral("PointCloudCircularityTolerance"));
    warpage_tolerance_ = makeTolerance(QStringLiteral("PointCloudWarpageTolerance"));
    profile_tolerance_ = makeTolerance(QStringLiteral("PointCloudProfileTolerance"));
    auto* evaluate_tolerances = new QPushButton(tr("一键测量并评级"));
    evaluate_tolerances->setObjectName(QStringLiteral("PointCloudEvaluateTolerancesButton"));
    evaluate_tolerances->setProperty("role", QStringLiteral("primary"));
    tolerance_summary_ = new QLabel(tr("设置各项公差上限后开始评级。"));
    tolerance_summary_->setObjectName(QStringLiteral("PointCloudToleranceSummary"));
    tolerance_summary_->setWordWrap(true);
    tolerance_summary_->setTextFormat(Qt::RichText);
    tolerance_form->addRow(tr("平面度"), flatness_tolerance_);
    tolerance_form->addRow(tr("圆柱度"), cylindricity_tolerance_);
    tolerance_form->addRow(tr("真圆度"), circularity_tolerance_);
    tolerance_form->addRow(tr("翘曲度"), warpage_tolerance_);
    tolerance_form->addRow(tr("轮廓度"), profile_tolerance_);
    tolerance_form->addRow({}, evaluate_tolerances);
    tolerance_form->addRow({}, tolerance_summary_);
    inspect_layout->addWidget(tolerance_group);
    auto* deviation_group = new QGroupBox(tr("偏差高斯分布"));
    deviation_group->setObjectName(QStringLiteral("PointCloudDeviationGroup"));
    auto* deviation_layout = new QVBoxLayout(deviation_group);
    auto* deviation_help = new QLabel(tr(
        "统计当前选中平面、球或圆柱模型的有符号偏差；存在选区时仅分析选区，"
        "并显示直方图、理论高斯曲线和 σ 覆盖率。"));
    deviation_help->setWordWrap(true);
    auto* deviation_button = new QPushButton(tr("分析偏差高斯分布…"));
    deviation_button->setObjectName(QStringLiteral("PointCloudDeviationDistributionButton"));
    deviation_button->setProperty("role", QStringLiteral("primary"));
    deviation_layout->addWidget(deviation_help);
    deviation_layout->addWidget(deviation_button);
    inspect_layout->addWidget(deviation_group);
    auto* section_group = new QGroupBox(tr("三维剖线分析"));
    section_group->setObjectName(QStringLiteral("PointCloudSectionGroup"));
    auto* section_layout = new QVBoxLayout(section_group);
    section_list_ = new QListWidget;
    section_list_->setObjectName(QStringLiteral("PointCloudSectionList"));
    section_list_->setMinimumHeight(92);
    section_list_->setToolTip(tr("勾选控制显示；当前剖线以黄色高亮"));
    section_layout->addWidget(section_list_);
    begin_section_button_ = new QPushButton(tr("在点云上绘制剖线"));
    begin_section_button_->setObjectName(QStringLiteral("PointCloudBeginSectionButton"));
    begin_section_button_->setProperty("role", QStringLiteral("primary"));
    duplicate_section_button_ = new QPushButton(tr("复制"));
    offset_section_button_ = new QPushButton(tr("平行偏移…"));
    delete_section_button_ = new QPushButton(tr("删除"));
    clear_sections_button_ = new QPushButton(tr("清空"));
    duplicate_section_button_->setObjectName(QStringLiteral("PointCloudDuplicateSectionButton"));
    offset_section_button_->setObjectName(QStringLiteral("PointCloudOffsetSectionButton"));
    delete_section_button_->setObjectName(QStringLiteral("PointCloudDeleteSectionButton"));
    clear_sections_button_->setObjectName(QStringLiteral("PointCloudClearSectionsButton"));
    delete_section_button_->setProperty("role", QStringLiteral("danger"));
    clear_sections_button_->setProperty("role", QStringLiteral("danger"));
    section_layout->addWidget(begin_section_button_);
    section_layout->addWidget(rowOf({duplicate_section_button_, offset_section_button_,
        delete_section_button_, clear_sections_button_}));

    auto* section_form = new QFormLayout;
    section_visible_check_ = new QCheckBox(tr("显示"));
    section_report_check_ = new QCheckBox(tr("纳入报告"));
    section_form->addRow(tr("状态"), rowOf({section_visible_check_, section_report_check_}));
    auto* start_row = new QWidget;
    auto* start_layout = new QHBoxLayout(start_row);
    start_layout->setContentsMargins(0, 0, 0, 0);
    auto* end_row = new QWidget;
    auto* end_layout = new QHBoxLayout(end_row);
    end_layout->setContentsMargins(0, 0, 0, 0);
    const QStringList coordinate_names{QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")};
    for (int axis = 0; axis < 3; ++axis) {
        section_endpoint_spins_[axis] = coordinateSpin(
            QStringLiteral("PointCloudSectionStart%1").arg(coordinate_names[axis]));
        section_endpoint_spins_[axis + 3] = coordinateSpin(
            QStringLiteral("PointCloudSectionEnd%1").arg(coordinate_names[axis]));
        section_endpoint_spins_[axis]->setPrefix(coordinate_names[axis] + QStringLiteral(" "));
        section_endpoint_spins_[axis + 3]->setPrefix(coordinate_names[axis] + QStringLiteral(" "));
        start_layout->addWidget(section_endpoint_spins_[axis]);
        end_layout->addWidget(section_endpoint_spins_[axis + 3]);
    }
    section_form->addRow(tr("起点"), start_row);
    section_form->addRow(tr("终点"), end_row);
    section_width_spin_ = new QDoubleSpinBox;
    section_width_spin_->setObjectName(QStringLiteral("PointCloudSectionBandWidth"));
    section_width_spin_->setDecimals(6);
    section_width_spin_->setRange(0.0, 1e12);
    section_width_spin_->setSpecialValueText(tr("自动"));
    section_spacing_spin_ = new QDoubleSpinBox;
    section_spacing_spin_->setObjectName(QStringLiteral("PointCloudSectionSpacing"));
    section_spacing_spin_->setDecimals(6);
    section_spacing_spin_->setRange(0.0, 1e12);
    section_spacing_spin_->setSpecialValueText(tr("自动"));
    section_reference_combo_ = new QComboBox;
    section_reference_combo_->setObjectName(QStringLiteral("PointCloudSectionReference"));
    section_reference_combo_->addItem(tr("世界 Z"), static_cast<int>(PointCloudSectionReference::WorldZ));
    section_reference_combo_->addItem(tr("当前参考平面"),
        static_cast<int>(PointCloudSectionReference::ReferencePlane));
    section_form->addRow(tr("物理带宽"), section_width_spin_);
    section_form->addRow(tr("采样间距"), section_spacing_spin_);
    section_form->addRow(tr("高度参考"), section_reference_combo_);

    section_median_window_spin_ = new QSpinBox;
    section_median_window_spin_->setRange(1, 31);
    section_median_window_spin_->setSingleStep(2);
    section_median_window_spin_->setValue(5);
    section_smoothing_window_spin_ = new QSpinBox;
    section_smoothing_window_spin_->setRange(3, 101);
    section_smoothing_window_spin_->setSingleStep(2);
    section_smoothing_window_spin_->setValue(11);
    section_gap_spin_ = new QSpinBox;
    section_gap_spin_->setRange(0, 20);
    section_gap_spin_->setValue(3);
    section_sensitivity_spin_ = new QDoubleSpinBox;
    section_sensitivity_spin_->setRange(1.0, 10.0);
    section_sensitivity_spin_->setValue(3.5);
    section_plateau_spin_ = new QSpinBox;
    section_plateau_spin_->setRange(3, 100);
    section_plateau_spin_->setValue(5);
    section_form->addRow(tr("中值窗口"), section_median_window_spin_);
    section_form->addRow(tr("稳健平滑"), section_smoothing_window_spin_);
    section_form->addRow(tr("最大插值缺口"), section_gap_spin_);
    section_form->addRow(tr("检测灵敏度"), section_sensitivity_spin_);
    section_form->addRow(tr("最小平台点"), section_plateau_spin_);
    section_layout->addLayout(section_form);

    section_feature_list_ = new QListWidget;
    section_feature_list_->setObjectName(QStringLiteral("PointCloudSectionFeatureList"));
    section_feature_list_->setMaximumHeight(110);
    section_feature_type_combo_ = new QComboBox;
    section_feature_type_combo_->addItem(tr("台阶"), static_cast<int>(PointCloudSectionFeatureType::Step));
    section_feature_type_combo_->addItem(tr("沟槽"), static_cast<int>(PointCloudSectionFeatureType::Groove));
    section_feature_type_combo_->addItem(tr("峰值"), static_cast<int>(PointCloudSectionFeatureType::Peak));
    section_feature_start_spin_ = coordinateSpin(QStringLiteral("PointCloudSectionFeatureStart"));
    section_feature_end_spin_ = coordinateSpin(QStringLiteral("PointCloudSectionFeatureEnd"));
    auto* apply_feature = new QPushButton(tr("应用人工边界"));
    apply_feature->setObjectName(QStringLiteral("PointCloudSectionApplyFeatureButton"));
    auto* add_feature = new QPushButton(tr("添加特征"));
    add_feature->setObjectName(QStringLiteral("PointCloudSectionAddFeatureButton"));
    auto* delete_feature = new QPushButton(tr("删除特征"));
    auto* reset_features = new QPushButton(tr("恢复自动识别"));
    section_layout->addWidget(new QLabel(tr("识别特征")));
    section_layout->addWidget(section_feature_list_);
    section_layout->addWidget(rowOf({section_feature_type_combo_,
        section_feature_start_spin_, section_feature_end_spin_, apply_feature}));
    section_layout->addWidget(rowOf({add_feature, delete_feature, reset_features}));
    section_status_ = new QLabel(tr("绘制三维剖线，分析结果会持续保留在下方工作区。"));
    section_status_->setObjectName(QStringLiteral("PointCloudSectionStatus"));
    section_status_->setWordWrap(true);
    section_layout->addWidget(section_status_);
    inspect_layout->addWidget(section_group);

    auto* report_group = new QGroupBox(tr("报告与导出"));
    report_group->setObjectName(QStringLiteral("PointCloudReportGroup"));
    auto* report_layout = new QVBoxLayout(report_group);
    auto* report_hint = new QLabel(tr(
        "导出当前测量和剖线结果；勾选“纳入报告”的剖线会同步到主检测报告。"));
    report_hint->setWordWrap(true);
    auto* report_measurements = new QPushButton(tr("导出测量 CSV…"));
    report_measurements->setObjectName(QStringLiteral("PointCloudReportMeasurementsButton"));
    report_measurements->setProperty("requiresMeasurements", true);
    auto* report_section_csv = new QPushButton(tr("导出剖线 CSV…"));
    report_section_csv->setObjectName(QStringLiteral("PointCloudReportSectionCsvButton"));
    report_section_csv->setProperty("requiresSections", true);
    auto* report_section_png = new QPushButton(tr("导出剖线图 PNG…"));
    report_section_png->setObjectName(QStringLiteral("PointCloudReportSectionPngButton"));
    report_section_png->setProperty("requiresSections", true);
    auto* report_section_html = new QPushButton(tr("生成剖线检测报告…"));
    report_section_html->setObjectName(QStringLiteral("PointCloudReportSectionHtmlButton"));
    report_section_html->setProperty("requiresSections", true);
    report_section_html->setProperty("role", QStringLiteral("primary"));
    report_layout->addWidget(report_hint);
    report_layout->addWidget(report_measurements);
    report_layout->addWidget(rowOf({report_section_csv, report_section_png}));
    report_layout->addWidget(report_section_html);
    connect(report_measurements, &QPushButton::clicked,
        this, &PointCloudDialog::exportMeasurements);
    connect(report_section_csv, &QPushButton::clicked,
        this, &PointCloudDialog::exportSectionCsv);
    connect(report_section_png, &QPushButton::clicked,
        this, &PointCloudDialog::exportSectionPng);
    connect(report_section_html, &QPushButton::clicked,
        this, &PointCloudDialog::exportSectionReport);
    inspect_layout->addWidget(report_group);
    inspect_layout->addStretch();
    inspect_page->setWidget(inspect_content);
    configureAccordion(this, QStringLiteral("analysis"),
        {tolerance_group, deviation_group, section_group, report_group}, 0);
    tabs_->addTab(inspect_page, style()->standardIcon(QStyle::SP_FileDialogContentsView), tr("分析"));
    tabs_->setTabToolTip(3, tr("分析与报告"));

    workspace_splitter_->addWidget(side_panel_);
    workspace_splitter_->setStretchFactor(0, 1);
    workspace_splitter_->setStretchFactor(1, 0);
    workspace_splitter_->setSizes(wide_workspace_sizes_);
    root->addWidget(workspace_splitter_, 1);

    bindPushButton(open_button_, open_action_);
    connect(open_action_, &QAction::triggered, this, &PointCloudDialog::openCloud);
    connect(browse_action_, &QAction::triggered, this, [this] {
        setInteractionMode(PointCloudInteractionMode::Browse);
    });
    connect(select_action_, &QAction::triggered, this, [this] {
        setInteractionMode(PointCloudInteractionMode::Select);
    });
    connect(clear_selection_action_, &QAction::triggered, this, [this] {
        const bool resume_selection = interaction_mode_ == PointCloudInteractionMode::Select;
        clearInteractiveCrop();
        if (resume_selection && !current_cloud_.Empty()) {
            setInteractionMode(PointCloudInteractionMode::Select);
        }
        updateSelectionPresentation();
    });
    connect(undo_action_, &QAction::triggered, this, &PointCloudDialog::undoProcessing);
    connect(redo_action_, &QAction::triggered, this, &PointCloudDialog::redoProcessing);
    connect(section_action_, &QAction::triggered, this, [this] {
        setWorkspacePage(PointCloudWorkspacePage::AnalysisReport);
        if (auto* group = findChild<QGroupBox*>(QStringLiteral("PointCloudSectionGroup"))) {
            group->setChecked(true);
        }
        beginSectionAnalysis();
    });
    connect(quick_plane, &QAction::triggered, this, [this] {
        setWorkspacePage(PointCloudWorkspacePage::GeometryMeasurement);
        if (auto* group = findChild<QGroupBox*>(QStringLiteral("PointCloudFitGroup"))) {
            group->setChecked(true);
        }
        fit_status_->setText(tr("已选择平面拟合，请确认作用范围后开始拟合。"));
        fit_plane_model_button_->setFocus();
    });
    connect(quick_sphere, &QAction::triggered, this, [this] {
        setWorkspacePage(PointCloudWorkspacePage::GeometryMeasurement);
        if (auto* group = findChild<QGroupBox*>(QStringLiteral("PointCloudFitGroup"))) {
            group->setChecked(true);
        }
        fit_status_->setText(tr("已选择球拟合，请确认作用范围和半径约束。"));
        fit_sphere_button_->setFocus();
    });
    connect(quick_cylinder, &QAction::triggered, this, [this] {
        setWorkspacePage(PointCloudWorkspacePage::GeometryMeasurement);
        if (auto* group = findChild<QGroupBox*>(QStringLiteral("PointCloudFitGroup"))) {
            group->setChecked(true);
        }
        fit_status_->setText(tr("已选择圆柱拟合，请确认轴向和半径约束。"));
        fit_cylinder_button_->setFocus();
    });
    connect(reset_view_action, &QAction::triggered, cloud_widget_, &PointCloudWidget::resetView);
    connect(drawer_toggle_button_, &QToolButton::clicked, this,
        [this] { setCompactDrawerOpen(!compact_drawer_open_); });
    connect(task_cancel_button_, &QToolButton::clicked, this, [this] {
        if (fit_running_ && !active_task_cancel_token_) {
            cancelActiveFit(tr("已取消拟合；后台结果将被安全丢弃。"));
            return;
        }
        if (active_task_cancel_token_) {
            active_task_cancel_token_->store(true, std::memory_order_relaxed);
            task_cancel_button_->setEnabled(false);
            setInlineStatus(task_label_, tr("正在取消当前任务…"), QStringLiteral("warning"));
        }
    });
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
        setWorkspacePage(static_cast<PointCloudWorkspacePage>(std::clamp(index, 0, 3)), false);
    });
    connect(export_button, &QPushButton::clicked, this, &PointCloudDialog::exportCloud);
    connect(reset_view, &QPushButton::clicked, cloud_widget_, &PointCloudWidget::resetView);
    connect(view_preset_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        cloud_widget_->setViewPreset(static_cast<PointCloudViewPreset>(
            view_preset_combo_->currentData().toInt()));
    });
    connect(unit_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        setMeasureMode(PointCloudMeasureMode::Navigate);
        current_cloud_.unit = static_cast<PointCloudUnit>(unit_combo_->currentData().toInt());
        original_cloud_.unit = current_cloud_.unit;
        for (PointCloud& cloud : undo_stack_) cloud.unit = current_cloud_.unit;
        measurements_.clear();
        pending_points_.clear();
        cloud_widget_->setHighlightedIndices({});
        resetInspectionResults();
        if (fitted_plane_.valid) fitPlane();
        updateCloudPresentation();
        refreshMeasurementList();
        updateSectionPresentation();
    });
    connect(color_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        const auto mode = static_cast<PointCloudColorMode>(color_combo_->currentData().toInt());
        cloud_widget_->setColorMode(mode);
        texture_enhance_check_->setEnabled(
            current_cloud_.HasTextureSurface() && mode == PointCloudColorMode::Texture);
    });
    connect(texture_enhance_check_, &QCheckBox::toggled,
        cloud_widget_, &PointCloudWidget::setTextureEnhancementEnabled);
    connect(point_size_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
        cloud_widget_, &PointCloudWidget::setPointSize);
    connect(axes_check_, &QCheckBox::toggled, cloud_widget_, &PointCloudWidget::setAxesVisible);
    connect(cloud_widget_, &PointCloudWidget::renderBackendChanged, this,
        [this](const QString& description, bool hardware) {
            backend_label_->setText(hardware
                ? tr("%1（硬件加速）").arg(description)
                : tr("%1（软件回退）").arg(description));
        });
    connect(cloud_widget_, &PointCloudWidget::renderStatisticsChanged, this,
        [this](int displayed, bool interactive) {
            if (current_cloud_.Empty()) return;
            last_rendered_point_count_ = displayed;
            workspace_status_->setText(interactive
                ? tr("交互预览 · %1 / %2 点 · %3")
                      .arg(displayed).arg(current_cloud_.Size()).arg(unitLabel())
                : tr("%1 点 · 显示 %2 · %3")
                      .arg(current_cloud_.Size()).arg(displayed).arg(unitLabel()));
        });
    connect(cloud_widget_, &PointCloudWidget::interactionCancelled, this, [this] {
        setInteractionMode(PointCloudInteractionMode::Browse);
        workspace_status_->setText(tr("已取消当前交互；已有选择保持不变"));
    });
    connect(voxel_apply, &QPushButton::clicked, this, &PointCloudDialog::applyVoxelDownsample);
    connect(outlier_apply, &QPushButton::clicked, this, &PointCloudDialog::applyOutlierRemoval);
    connect(smart_filter_apply, &QPushButton::clicked,
        this, &PointCloudDialog::applySmartDenoise);
    connect(hole_repair_apply, &QPushButton::clicked,
        this, &PointCloudDialog::applyHoleRepair);
    connect(begin_crop_button_, &QPushButton::clicked,
        this, &PointCloudDialog::beginInteractiveCrop);
    connect(keep_crop_button_, &QPushButton::clicked,
        this, [this] { applyInteractiveCrop(true); });
    connect(remove_crop_button_, &QPushButton::clicked,
        this, [this] { applyInteractiveCrop(false); });
    connect(cloud_widget_, &PointCloudWidget::boxSelectionFinished,
        this, &PointCloudDialog::acceptBoxSelection);
    connect(fit_button, &QPushButton::clicked, this, &PointCloudDialog::fitPlane);
    connect(show_plane_check_, &QCheckBox::toggled,
        cloud_widget_, &PointCloudWidget::setFittedPlaneVisible);
    connect(level_button_, &QPushButton::clicked, this, &PointCloudDialog::levelCloud);
    connect(restore_button, &QPushButton::clicked, this, &PointCloudDialog::restoreOriginal);
    connect(cloud_widget_, &PointCloudWidget::pointPicked,
        this, &PointCloudDialog::acceptPickedPoint);
    connect(delete_measurement_button_, &QPushButton::clicked, this, [this] {
        const int row = measurement_list_->currentRow();
        if (row < 0 || row >= static_cast<int>(measurements_.size())) return;
        measurements_.erase(measurements_.begin() + row);
        refreshMeasurementList();
        if (!measurements_.empty()) {
            measurement_list_->setCurrentRow(std::min(row,
                static_cast<int>(measurements_.size()) - 1));
        }
    });
    connect(clear_measurements_button_, &QPushButton::clicked, this, [this] {
        measurements_.clear();
        pending_points_.clear();
        cloud_widget_->setHighlightedIndices({});
        refreshMeasurementList();
    });
    connect(export_measurements_button_, &QPushButton::clicked,
        this, &PointCloudDialog::exportMeasurements);
    connect(measurement_list_, &QListWidget::currentRowChanged,
        this, &PointCloudDialog::selectMeasurementRow);
    connect(evaluate_tolerances, &QPushButton::clicked,
        this, &PointCloudDialog::evaluateTolerances);
    connect(deviation_button, &QPushButton::clicked,
        this, &PointCloudDialog::showDeviationDistribution);
    connect(begin_section_button_, &QPushButton::clicked,
        this, &PointCloudDialog::beginSectionAnalysis);
    connect(section_list_, &QListWidget::currentRowChanged,
        this, &PointCloudDialog::selectSectionRow);
    connect(section_list_, &QListWidget::itemDoubleClicked, this,
        [this](QListWidgetItem* item) {
            const int row = section_list_->row(item);
            if (row < 0 || row >= static_cast<int>(section_profiles_.size())) return;
            bool accepted = false;
            const QString current = QString::fromStdWString(
                section_profiles_[static_cast<std::size_t>(row)].definition.name);
            const QString name = QInputDialog::getText(this, tr("重命名剖线"),
                tr("名称"), QLineEdit::Normal, current, &accepted).trimmed();
            if (!accepted || name.isEmpty()) return;
            section_profiles_[static_cast<std::size_t>(row)].definition.name = name.toStdWString();
            refreshSectionList(row);
        });
    connect(section_list_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        if (section_editor_updating_ || !item) return;
        const int row = section_list_->row(item);
        if (row < 0 || row >= static_cast<int>(section_profiles_.size())) return;
        section_profiles_[static_cast<std::size_t>(row)].definition.visible =
            item->checkState() == Qt::Checked;
        updateSectionPresentation();
    });
    connect(duplicate_section_button_, &QPushButton::clicked,
        this, &PointCloudDialog::duplicateSection);
    connect(offset_section_button_, &QPushButton::clicked,
        this, &PointCloudDialog::offsetSection);
    connect(delete_section_button_, &QPushButton::clicked,
        this, &PointCloudDialog::deleteSection);
    connect(clear_sections_button_, &QPushButton::clicked,
        this, &PointCloudDialog::clearSections);
    connect(export_section_csv_button_, &QPushButton::clicked,
        this, &PointCloudDialog::exportSectionCsv);
    connect(export_section_png_button_, &QPushButton::clicked,
        this, &PointCloudDialog::exportSectionPng);
    connect(export_section_report_button_, &QPushButton::clicked,
        this, &PointCloudDialog::exportSectionReport);
    connect(section_plot_, &PointCloudSectionPlotWidget::cursorPositionsChanged,
        this, &PointCloudDialog::updateSectionCursorMetrics);
    connect(section_visible_check_, &QCheckBox::toggled,
        this, &PointCloudDialog::scheduleSectionReanalysis);
    connect(section_report_check_, &QCheckBox::toggled,
        this, &PointCloudDialog::scheduleSectionReanalysis);
    for (QDoubleSpinBox* spin : section_endpoint_spins_) {
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &PointCloudDialog::scheduleSectionReanalysis);
    }
    for (QDoubleSpinBox* spin : {section_width_spin_, section_spacing_spin_,
            section_sensitivity_spin_}) {
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &PointCloudDialog::scheduleSectionReanalysis);
    }
    for (QSpinBox* spin : {section_median_window_spin_, section_smoothing_window_spin_,
            section_gap_spin_, section_plateau_spin_}) {
        connect(spin, qOverload<int>(&QSpinBox::valueChanged),
            this, &PointCloudDialog::scheduleSectionReanalysis);
    }
    connect(section_reference_combo_, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &PointCloudDialog::scheduleSectionReanalysis);
    connect(section_feature_list_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) return;
        const int section_row = section_list_->currentRow();
        if (section_row < 0 || section_row >= static_cast<int>(section_profiles_.size())) return;
        const auto& features = section_profiles_[static_cast<std::size_t>(section_row)].features;
        if (row >= static_cast<int>(features.size())) return;
        const auto& feature = features[static_cast<std::size_t>(row)];
        const QSignalBlocker type_blocker(section_feature_type_combo_);
        const QSignalBlocker start_blocker(section_feature_start_spin_);
        const QSignalBlocker end_blocker(section_feature_end_spin_);
        section_feature_type_combo_->setCurrentIndex(section_feature_type_combo_->findData(
            static_cast<int>(feature.type)));
        section_feature_start_spin_->setValue(feature.start_distance);
        section_feature_end_spin_->setValue(feature.end_distance);
    });
    connect(apply_feature, &QPushButton::clicked,
        this, &PointCloudDialog::applySectionFeatureEdit);
    connect(add_feature, &QPushButton::clicked, this, [this] {
        const int row = section_list_->currentRow();
        if (row < 0 || row >= static_cast<int>(section_profiles_.size())) return;
        auto& profile = section_profiles_[static_cast<std::size_t>(row)];
        PointCloudSectionFeature feature;
        feature.id = profile.features.empty() ? 1 : profile.features.back().id + 1;
        feature.type = static_cast<PointCloudSectionFeatureType>(
            section_feature_type_combo_->currentData().toInt());
        feature.start_distance = profile.width * 0.35;
        feature.end_distance = profile.width * 0.65;
        feature.automatic = false;
        profile.features.push_back(feature);
        refreshSectionFeatureList();
        section_feature_list_->setCurrentRow(static_cast<int>(profile.features.size()) - 1);
        applySectionFeatureEdit();
    });
    connect(delete_feature, &QPushButton::clicked, this, [this] {
        const int section_row = section_list_->currentRow();
        const int feature_row = section_feature_list_->currentRow();
        if (section_row < 0 || feature_row < 0) return;
        auto& features = section_profiles_[static_cast<std::size_t>(section_row)].features;
        if (feature_row >= static_cast<int>(features.size())) return;
        features.erase(features.begin() + feature_row);
        refreshSectionFeatureList(); updateSectionPresentation();
    });
    connect(reset_features, &QPushButton::clicked, this, [this] {
        const int row = section_list_->currentRow();
        if (row < 0 || row >= static_cast<int>(section_profiles_.size())) return;
        PointCloudSectionAnalysisOptions options;
        options.median_window = static_cast<std::size_t>(section_median_window_spin_->value());
        options.smoothing_window = static_cast<std::size_t>(section_smoothing_window_spin_->value());
        options.maximum_gap_bins = static_cast<std::size_t>(section_gap_spin_->value());
        options.detection_sensitivity = section_sensitivity_spin_->value();
        options.minimum_plateau_bins = static_cast<std::size_t>(section_plateau_spin_->value());
        PointCloudSectionAnalyzer::DetectFeatures(
            section_profiles_[static_cast<std::size_t>(row)], options);
        refreshSectionFeatureList(); updateSectionPresentation();
    });
    connect(fit_plane_model_button_, &QPushButton::clicked, this,
        [this] { fitGeometricModel(PointCloudGeometricModelType::Plane); });
    connect(fit_sphere_button_, &QPushButton::clicked, this,
        [this] { fitGeometricModel(PointCloudGeometricModelType::Sphere); });
    connect(fit_cylinder_button_, &QPushButton::clicked, this,
        [this] { fitGeometricModel(PointCloudGeometricModelType::Cylinder); });
    connect(cancel_fit_button_, &QPushButton::clicked, this, [this] {
        cancelActiveFit(tr("已取消拟合；后台计算结果将被安全丢弃。"));
    });
    connect(fit_scope_combo_, qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this] { updateActionStates(); });
    connect(model_list_, &QListWidget::currentRowChanged,
        this, &PointCloudDialog::selectModelRow);
    connect(show_model_button_, &QPushButton::clicked, this,
        [this] { setSelectedModelVisible(true); });
    connect(hide_model_button_, &QPushButton::clicked, this,
        [this] { setSelectedModelVisible(false); });
    connect(delete_model_button_, &QPushButton::clicked, this,
        &PointCloudDialog::deleteSelectedModel);
    connect(clear_models_button_, &QPushButton::clicked, this,
        &PointCloudDialog::clearGeometricModels);
    connect(model_list_, &QListWidget::itemDoubleClicked, this, [this] {
        const int row = model_list_->currentRow();
        if (row < 0 || row >= static_cast<int>(geometric_models_.size())) return;
        setSelectedModelVisible(!geometric_models_[static_cast<std::size_t>(row)].visible);
    });
    connect(residual_coloring_check_, &QCheckBox::toggled,
        cloud_widget_, &PointCloudWidget::setResidualColoringEnabled);
    connect(cloud_widget_, &PointCloudWidget::sectionSelectionFinished,
        this, &PointCloudDialog::acceptSectionSelection);
    connect(cloud_widget_, &PointCloudWidget::sectionDefinitionEdited, this,
        [this](const PointCloudSectionDefinition& definition, bool finished) {
            const auto profile = std::find_if(section_profiles_.begin(), section_profiles_.end(),
                [&definition](const auto& value) { return value.definition.id == definition.id; });
            if (profile == section_profiles_.end()) return;
            profile->definition = definition;
            active_section_id_ = definition.id;
            interaction_mode_ = finished
                ? PointCloudInteractionMode::Browse : PointCloudInteractionMode::SectionEdit;
            if (finished) {
                browse_action_->setChecked(true);
                select_action_->setChecked(false);
            }
            if (toolbar_section_button_) {
                toolbar_section_button_->setProperty("activeTool", !finished);
                toolbar_section_button_->style()->unpolish(toolbar_section_button_);
                toolbar_section_button_->style()->polish(toolbar_section_button_);
            }
            updateSectionEditors();
            updateSectionPresentation();
            if (finished) runSectionAnalysis(definition);
        });
    for (QWidget* action : std::initializer_list<QWidget*>{export_button, reset_view,
             voxel_apply, outlier_apply, smart_filter_apply, hole_repair_apply,
             begin_crop_button_, fit_button, restore_button, evaluate_tolerances,
             deviation_button, begin_section_button_, fit_plane_model_button_, fit_sphere_button_,
             fit_cylinder_button_}) {
        action->setProperty("requiresCloud", true);
    }
    resetInspectionResults();
    updateCloudPresentation({}, true);
    QTimer::singleShot(0, this, [this] {
        loadSettings();
        updateResponsiveLayout(true);
    });
}

void PointCloudDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    updateResponsiveLayout();
}

void PointCloudDialog::setWorkspacePage(PointCloudWorkspacePage page, bool reveal)
{
    workspace_page_ = page;
    const int index = static_cast<int>(page);
    if (tabs_ && tabs_->currentIndex() != index) tabs_->setCurrentIndex(index);
    if (task_page_title_) {
        const QStringList titles{
            tr("数据与显示"), tr("清理与处理"), tr("几何测量"), tr("分析与报告")};
        task_page_title_->setText(titles.value(index));
    }
    if (reveal && compact_layout_) setCompactDrawerOpen(true, false);
}

void PointCloudDialog::setInteractionMode(PointCloudInteractionMode mode)
{
    if (!cloud_widget_) return;
    if (mode == PointCloudInteractionMode::Select && current_cloud_.Empty()) mode = PointCloudInteractionMode::Browse;
    if (mode == PointCloudInteractionMode::Browse) {
        setMeasureMode(PointCloudMeasureMode::Navigate);
        cloud_widget_->setFreeSelectionEnabled(false);
        cloud_widget_->setSectionSelectionEnabled(false);
    } else if (mode == PointCloudInteractionMode::Select) {
        setMeasureMode(PointCloudMeasureMode::Navigate);
        interaction_mode_ = PointCloudInteractionMode::Select;
        cloud_widget_->setSelectionPreviewIndices(crop_selection_);
        cloud_widget_->setFreeSelectionEnabled(true);
        setWorkspacePage(PointCloudWorkspacePage::Processing);
        workspace_status_->setText(
            tr("自由选择 · Shift 添加 · Ctrl 移除 · Esc 取消绘制"));
    } else if (mode == PointCloudInteractionMode::SectionDraw) {
        setMeasureMode(PointCloudMeasureMode::Navigate);
        interaction_mode_ = PointCloudInteractionMode::SectionDraw;
        cloud_widget_->setSectionSelectionEnabled(true, 10.0);
    }
    interaction_mode_ = mode;
    if (browse_action_) browse_action_->setChecked(mode == PointCloudInteractionMode::Browse);
    if (select_action_) select_action_->setChecked(mode == PointCloudInteractionMode::Select);
    if (navigation_button_) navigation_button_->setChecked(mode == PointCloudInteractionMode::Browse);
    if (free_selection_button_) free_selection_button_->setChecked(mode == PointCloudInteractionMode::Select);
    if (toolbar_section_button_) {
        toolbar_section_button_->setProperty("activeTool",
            mode == PointCloudInteractionMode::SectionDraw ||
                mode == PointCloudInteractionMode::SectionEdit);
        toolbar_section_button_->style()->unpolish(toolbar_section_button_);
        toolbar_section_button_->style()->polish(toolbar_section_button_);
    }
    updateToolbarPresentation();
}

void PointCloudDialog::updateToolbarPresentation()
{
    const bool icons_only = compact_layout_;
    for (QToolButton* button : {toolbar_open_button_, toolbar_fit_button_,
            toolbar_measure_button_, toolbar_section_button_, toolbar_view_button_}) {
        if (button) {
            button->setIconSize(QSize(20, 20));
            button->setToolButtonStyle(
                icons_only ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
        }
    }
    for (QPushButton* button : {navigation_button_, free_selection_button_,
            clear_selection_button_, undo_button_, redo_button_}) {
        if (button) button->setIconSize(QSize(20, 20));
    }
    if (navigation_button_) navigation_button_->setText(icons_only ? QString() : browse_action_->text());
    if (free_selection_button_) free_selection_button_->setText(icons_only ? QString() : select_action_->text());
    if (clear_selection_button_) clear_selection_button_->setText(
        icons_only ? QString() : clear_selection_action_->text());
    if (undo_button_) undo_button_->setText(icons_only ? QString() : undo_action_->text());
    if (redo_button_) redo_button_->setText(icons_only ? QString() : redo_action_->text());
    if (texture_status_label_) texture_status_label_->setVisible(!compact_layout_);
    if (workspace_status_) workspace_status_->setVisible(!compact_layout_ || task_running_);
}

void PointCloudDialog::moveSectionWorkspace(bool intoDrawer)
{
    if (!section_workspace_ || !compact_drawer_tabs_ || !view_section_splitter_) return;
    if (intoDrawer) {
        if (compact_drawer_tabs_->indexOf(section_workspace_) >= 0) return;
        section_workspace_->setParent(nullptr);
        compact_drawer_tabs_->addTab(section_workspace_, tr("剖线图"));
    } else {
        const int drawer_index = compact_drawer_tabs_->indexOf(section_workspace_);
        if (drawer_index < 0) return;
        compact_drawer_tabs_->removeTab(drawer_index);
        section_workspace_->setParent(nullptr);
        view_section_splitter_->addWidget(section_workspace_);
        view_section_splitter_->setStretchFactor(0, 1);
        view_section_splitter_->setStretchFactor(1, 0);
        view_section_splitter_->setSizes(wide_section_sizes_);
    }
}

void PointCloudDialog::setCompactDrawerOpen(bool open, bool showSection)
{
    if (!compact_layout_) {
        if (showSection && view_section_splitter_) {
            const int available = std::max(view_section_splitter_->height(), 500);
            view_section_splitter_->setSizes({std::max(260, available - 260), 260});
        }
        return;
    }
    compact_drawer_open_ = open;
    if (compact_drawer_tabs_) {
        compact_drawer_tabs_->setVisible(open);
        if (open) compact_drawer_tabs_->setCurrentIndex(showSection ? 1 : 0);
    }
    if (drawer_toggle_button_) {
        drawer_toggle_button_->setText(open ? tr("收起") : tr("展开"));
        drawer_toggle_button_->setToolTip(open ? tr("收起工具抽屉") : tr("展开工具抽屉"));
    }
    if (workspace_splitter_) {
        const int available = std::max(workspace_splitter_->height(), height() - 130);
        const int drawer = open ? std::min(300, std::max(220, available * 45 / 100)) : 42;
        workspace_splitter_->setSizes({std::max(260, available - drawer), drawer});
    }
}

void PointCloudDialog::updateResponsiveLayout(bool force)
{
    if (!workspace_splitter_ || !side_panel_ || !view_section_splitter_) return;
    const bool compact = width() < 1180;
    if (!force && compact == compact_layout_) {
        updateToolbarPresentation();
        return;
    }
    if (compact) {
        if (!compact_layout_) {
            wide_workspace_sizes_ = workspace_splitter_->sizes();
            wide_section_sizes_ = view_section_splitter_->sizes();
        }
        compact_layout_ = true;
        moveSectionWorkspace(true);
        workspace_splitter_->setOrientation(Qt::Vertical);
        cloud_widget_->setMinimumSize(480, 250);
        side_panel_->setMinimumWidth(0);
        side_panel_->setMaximumWidth(QWIDGETSIZE_MAX);
        side_panel_->setMinimumHeight(42);
        side_panel_->setMaximumHeight(QWIDGETSIZE_MAX);
        drawer_toggle_button_->setVisible(true);
        compact_drawer_tabs_->setTabBarAutoHide(false);
        setCompactDrawerOpen(false);
    } else {
        compact_layout_ = false;
        compact_drawer_open_ = true;
        moveSectionWorkspace(false);
        workspace_splitter_->setOrientation(Qt::Horizontal);
        cloud_widget_->setMinimumSize(480, 360);
        side_panel_->setMinimumHeight(0);
        side_panel_->setMaximumHeight(QWIDGETSIZE_MAX);
        side_panel_->setMinimumWidth(336);
        side_panel_->setMaximumWidth(480);
        drawer_toggle_button_->setVisible(false);
        compact_drawer_tabs_->setVisible(true);
        compact_drawer_tabs_->setCurrentIndex(0);
        compact_drawer_tabs_->setTabBarAutoHide(true);
        workspace_splitter_->setSizes(wide_workspace_sizes_.isEmpty()
            ? QList<int>{900, 380} : wide_workspace_sizes_);
    }
    updateToolbarPresentation();
}

void PointCloudDialog::beginInlineTask(const QString& operation, bool determinate)
{
    task_running_ = true;
    task_bar_->setVisible(true);
    task_cancel_button_->setVisible(true);
    task_cancel_button_->setEnabled(true);
    task_progress_->setRange(0, determinate ? 1000 : 0);
    if (determinate) task_progress_->setValue(0);
    setInlineStatus(task_label_, tr("正在%1…").arg(operation), QStringLiteral("busy"));
    updateActionStates();
    updateToolbarPresentation();
}

void PointCloudDialog::updateInlineTaskProgress(int value, int maximum)
{
    if (!task_progress_) return;
    task_progress_->setRange(0, maximum);
    task_progress_->setValue(std::clamp(value, 0, maximum));
}

void PointCloudDialog::endInlineTask(const QString& message, const QString& status)
{
    task_running_ = false;
    active_task_cancel_token_.reset();
    task_cancel_button_->setEnabled(false);
    task_cancel_button_->setVisible(false);
    task_progress_->setRange(0, 1);
    task_progress_->setValue(status == QStringLiteral("error") ? 0 : 1);
    setInlineStatus(task_label_, message, status);
    updateActionStates();
    updateToolbarPresentation();
    QTimer::singleShot(2400, this, [this] {
        if (!task_running_ && task_bar_) task_bar_->setVisible(false);
    });
}

void PointCloudDialog::setCloud(const PointCloud& cloud)
{
    cancelActiveFit(tr("点云已替换，原拟合任务已取消。"));
    setMeasureMode(PointCloudMeasureMode::Navigate);
    clearInteractiveCrop();
    original_cloud_ = current_cloud_ = cloud;
    ++cloud_revision_;
    clearSections();
    if (!current_cloud_.Empty() && !current_cloud_.bounds.valid) {
        original_cloud_.RecalculateBounds();
        current_cloud_ = original_cloud_;
    }
    undo_stack_.clear();
    redo_stack_.clear();
    measurements_.clear();
    pending_points_.clear();
    clearFittedPlane();
    clearGeometricModels();
    const int unit_index = unit_combo_->findData(static_cast<int>(current_cloud_.unit));
    if (unit_index >= 0) unit_combo_->setCurrentIndex(unit_index);
    const int texture_index = color_combo_->findData(
        static_cast<int>(PointCloudColorMode::Texture));
    if (current_cloud_.HasTextureSurface() && texture_index >= 0) {
        color_combo_->setCurrentIndex(texture_index);
    } else if (color_combo_->currentData().toInt() ==
        static_cast<int>(PointCloudColorMode::Texture)) {
        color_combo_->setCurrentIndex(color_combo_->findData(
            static_cast<int>(PointCloudColorMode::Height)));
    }
    resetInspectionResults();
    updateCloudPresentation({}, true);
    updateProcessingDefaults();
    refreshMeasurementList();
}

void PointCloudDialog::openCloud()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("打开 3D 点云"), {},
        tr("点云文件 (*.h3d *.ply *.pcd *.xyz *.txt *.csv);;Motic H3D (*.h3d);;PLY (*.ply);;PCD (*.pcd);;XYZ/文本 (*.xyz *.txt *.csv);;所有文件 (*)"));
    if (path.isEmpty()) return;
    startCloudLoad(path);
}

void PointCloudDialog::startCloudLoad(const QString& path)
{
    if (task_running_) return;
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    auto progress_value = std::make_shared<std::atomic_int>(0);
    active_task_cancel_token_ = cancelled;
    beginInlineTask(tr("读取点云"), true);
    open_action_->setEnabled(false);

    auto* watcher = new QFutureWatcher<PointCloudLoadResult>(this);
    auto* progress_timer = new QTimer(watcher);
    progress_timer->setInterval(40);
    connect(progress_timer, &QTimer::timeout, this,
        [this, progress_value] {
            updateInlineTaskProgress(progress_value->load(std::memory_order_relaxed));
        });
    progress_timer->start();
    connect(watcher, &QFutureWatcher<PointCloudLoadResult>::finished, this,
        [this, watcher] {
            const PointCloudLoadResult result = watcher->result();
            watcher->deleteLater();
            open_action_->setEnabled(true);
            if (result.cancelled) {
                endInlineTask(tr("已取消点云导入"), QStringLiteral("warning"));
                return;
            }
            if (!result.loaded) {
                endInlineTask(tr("点云导入失败：%1").arg(errorText(result.error)),
                    QStringLiteral("error"));
                return;
            }
            setCloud(result.cloud);
            endInlineTask(tr("点云载入完成"));
        });
    const std::filesystem::path native_path(path.toStdWString());
    const PointCloudUnit unit = static_cast<PointCloudUnit>(unit_combo_->currentData().toInt());
    watcher->setFuture(QtConcurrent::run(
        [native_path, unit, cancelled, progress_value]() mutable {
            PointCloudLoadResult result;
            result.loaded = PointCloudIO::Load(native_path, result.cloud, result.error, unit,
                [cancelled, progress_value](std::uint64_t bytes, std::uint64_t total) {
                    if (cancelled->load(std::memory_order_relaxed)) return false;
                    const int progress = total > 0
                        ? static_cast<int>(std::min<std::uint64_t>(1000, bytes * 1000 / total))
                        : 0;
                    progress_value->store(progress, std::memory_order_relaxed);
                    return true;
                });
            result.cancelled = cancelled->load(std::memory_order_relaxed) ||
                result.error == L"Point-cloud import was cancelled.";
            return result;
        }));
}

void PointCloudDialog::exportCloud()
{
    if (current_cloud_.Empty()) {
        QMessageBox::information(this, tr("导出点云"), tr("当前没有可导出的点云。"));
        return;
    }
    QString path = QFileDialog::getSaveFileName(
        this, tr("导出处理后的点云"), QStringLiteral("CameraView-point-cloud.ply"),
        tr("PLY 点云 (*.ply);;XYZ 点云 (*.xyz)"));
    if (path.isEmpty()) return;
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix.isEmpty()) path += QStringLiteral(".ply");
    std::wstring error;
    const bool saved = QFileInfo(path).suffix().compare(QStringLiteral("xyz"), Qt::CaseInsensitive) == 0
        ? PointCloudIO::SaveXyz(std::filesystem::path(path.toStdWString()), current_cloud_, error)
        : PointCloudIO::SavePly(std::filesystem::path(path.toStdWString()), current_cloud_, error);
    if (!saved) QMessageBox::warning(this, tr("点云导出失败"), errorText(error));
}

void PointCloudDialog::updateCloudPresentation(const QString& operation, bool reset_view)
{
    cloud_widget_->setCloud(current_cloud_, reset_view);
    const bool has_cloud = !current_cloud_.Empty();
    for (QWidget* widget : findChildren<QWidget*>()) {
        if (widget->property("requiresCloud").toBool()) widget->setEnabled(has_cloud);
    }
    source_label_->setText(current_cloud_.Empty()
        ? tr("尚未载入点云")
        : tr("%1%2%3").arg(QString::fromStdWString(current_cloud_.name),
            current_cloud_.format_name.empty() ? QString()
                : tr(" · %1").arg(QString::fromStdWString(current_cloud_.format_name)),
            operation.isEmpty() ? QString() : tr(" · %1").arg(operation)));
    source_label_->setToolTip(source_label_->text());
    const int texture_index = color_combo_->findData(
        static_cast<int>(PointCloudColorMode::Texture));
    if (auto* model = qobject_cast<QStandardItemModel*>(color_combo_->model());
        model && texture_index >= 0 && model->item(texture_index)) {
        model->item(texture_index)->setEnabled(current_cloud_.HasTextureSurface());
    }
    if (has_cloud && !current_cloud_.HasTextureSurface() &&
        color_combo_->currentData().toInt() == static_cast<int>(PointCloudColorMode::Texture)) {
        const int original_index = color_combo_->findData(
            static_cast<int>(PointCloudColorMode::Original));
        if (original_index >= 0) color_combo_->setCurrentIndex(original_index);
    }
    if (current_cloud_.HasTextureSurface()) {
        texture_status_label_->setText(tr("● 结构化网格 %1 × %2 · 纹理就绪")
            .arg(current_cloud_.organized_width).arg(current_cloud_.organized_height));
        texture_status_label_->setProperty("status", QStringLiteral("ok"));
    } else if (has_cloud) {
        texture_status_label_->setText(tr("● 非结构化点云 · 点模式"));
        texture_status_label_->setProperty("status", QStringLiteral("neutral"));
    } else {
        texture_status_label_->setText(tr("等待载入数据"));
        texture_status_label_->setProperty("status", QStringLiteral("neutral"));
    }
    texture_enhance_check_->setEnabled(current_cloud_.HasTextureSurface() &&
        color_combo_->currentData().toInt() == static_cast<int>(PointCloudColorMode::Texture));
    texture_status_label_->style()->unpolish(texture_status_label_);
    texture_status_label_->style()->polish(texture_status_label_);
    if (current_cloud_.Empty()) {
        statistics_label_->setText(tr("点数 0"));
        statistics_label_->setToolTip({});
    } else {
        const auto center = current_cloud_.Centroid();
        statistics_label_->setText(tr("点数 %1").arg(current_cloud_.Size()));
        statistics_label_->setToolTip(
            tr("范围：X %1，Y %2，Z %3 %4\n质心：(%5, %6, %7)%8")
                .arg(current_cloud_.bounds.Width(), 0, 'g', 7)
                .arg(current_cloud_.bounds.Depth(), 0, 'g', 7)
                .arg(current_cloud_.bounds.Height(), 0, 'g', 7)
                .arg(unitLabel())
                .arg(center.x, 0, 'g', 7)
                .arg(center.y, 0, 'g', 7)
                .arg(center.z, 0, 'g', 7)
                .arg(current_cloud_.HasTextureSurface()
                    ? tr("\n纹理：%1 × %2，24 位真彩")
                          .arg(current_cloud_.organized_width)
                          .arg(current_cloud_.organized_height)
                    : QString()));
    }
    undo_action_->setEnabled(!undo_stack_.empty() && !task_running_);
    redo_action_->setEnabled(!redo_stack_.empty() && !task_running_);
    level_button_->setEnabled(fitted_plane_.valid && has_cloud);
    workspace_status_->setText(has_cloud
        ? tr("%1 点 · 显示 %2 · %3")
              .arg(current_cloud_.Size()).arg(cloud_widget_->renderedPointCount()).arg(unitLabel())
        : tr("未载入点云"));
    updateSelectionPresentation();
    updateActionStates();
    updateToolbarPresentation();
}

void PointCloudDialog::resetInspectionResults()
{
    if (tolerance_summary_) {
        tolerance_summary_->setText(tr("设置各项公差上限后开始评级。"));
    }
    if (section_status_ && section_profiles_.empty()) {
        section_status_->setText(tr("绘制三维剖线，分析结果会持续保留在下方工作区。"));
    }
}

void PointCloudDialog::updateProcessingDefaults()
{
    if (!current_cloud_.bounds.valid) return;
    const double extent = std::max({current_cloud_.bounds.Width(), current_cloud_.bounds.Depth(),
        current_cloud_.bounds.Height(), 1e-6});
    voxel_spin_->setValue(extent / 100.0);
    outlier_radius_spin_->setValue(extent / 50.0);
    const double nominal_spacing = PointCloudProcessor::EstimateNominalSpacing(current_cloud_);
    if (nominal_spacing > 0.0) {
        smart_radius_spin_->setValue(nominal_spacing * 2.5);
        smart_deviation_spin_->setValue(nominal_spacing * 0.2);
        repair_spacing_spin_->setValue(nominal_spacing);
        smart_filter_report_->setText(tr("估计点间距：%1 %2")
            .arg(nominal_spacing, 0, 'g', 7).arg(unitLabel()));
    }
}

void PointCloudDialog::pushProcessedCloud(PointCloud cloud, const QString& operation)
{
    if (cloud.Empty()) {
        QMessageBox::information(this, tr("点云处理"), tr("该参数会移除所有点，请调整后重试。"));
        return;
    }
    cancelActiveFit(tr("点云数据已改变，原拟合任务已取消。"));
    setMeasureMode(PointCloudMeasureMode::Navigate);
    clearInteractiveCrop();
    undo_stack_.push_back(current_cloud_);
    redo_stack_.clear();
    current_cloud_ = std::move(cloud);
    ++cloud_revision_;
    clearSections();
    clearFittedPlane();
    clearGeometricModels();
    measurements_.clear();
    pending_points_.clear();
    resetInspectionResults();
    updateCloudPresentation(operation);
    updateProcessingDefaults();
    refreshMeasurementList();
}

void PointCloudDialog::runCloudTask(
    const QString& operation,
    std::function<PointCloud()> task,
    std::function<void()> completed)
{
    if (current_cloud_.Empty() || task_running_) return;
    cancelActiveFit(tr("已开始点云处理，原拟合任务已取消。"));
    const std::uint64_t revision = cloud_revision_;
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    active_task_cancel_token_ = cancelled;
    beginInlineTask(operation);
    workspace_status_->setText(tr("正在%1；可取消").arg(operation));
    auto* watcher = new QFutureWatcher<PointCloud>(this);
    connect(watcher, &QFutureWatcher<PointCloud>::finished, this,
        [this, watcher, cancelled, revision, operation,
            completed = std::move(completed)]() mutable {
            PointCloud result = watcher->result();
            const bool was_cancelled = cancelled->load(std::memory_order_relaxed);
            watcher->deleteLater();
            if (was_cancelled) {
                workspace_status_->setText(tr("已取消%1").arg(operation));
                endInlineTask(tr("已取消%1").arg(operation), QStringLiteral("warning"));
                return;
            }
            if (revision != cloud_revision_) {
                workspace_status_->setText(tr("点云已改变，已丢弃过期结果"));
                endInlineTask(tr("点云已改变，已丢弃过期结果"), QStringLiteral("warning"));
                return;
            }
            pushProcessedCloud(std::move(result), operation);
            if (completed) completed();
            endInlineTask(tr("%1完成").arg(operation));
        });
    watcher->setFuture(QtConcurrent::run(std::move(task)));
}

void PointCloudDialog::applyVoxelDownsample()
{
    const PointCloud cloud = current_cloud_;
    const double voxel = voxel_spin_->value();
    runCloudTask(tr("体素降采样"), [cloud, voxel] {
        return PointCloudProcessor::VoxelDownsample(cloud, voxel);
    });
}

void PointCloudDialog::applyOutlierRemoval()
{
    const PointCloud cloud = current_cloud_;
    const double radius = outlier_radius_spin_->value();
    const std::size_t neighbors = static_cast<std::size_t>(outlier_neighbors_spin_->value());
    runCloudTask(tr("离群点过滤"), [cloud, radius, neighbors] {
        return PointCloudProcessor::RemoveRadiusOutliers(cloud, radius, neighbors);
    });
}

void PointCloudDialog::applySmartDenoise()
{
    if (current_cloud_.Empty()) return;
    PointCloudDenoiseOptions options;
    options.neighbor_radius = smart_radius_spin_->value();
    options.minimum_neighbors = static_cast<std::size_t>(smart_neighbors_spin_->value());
    options.spike_sigma = smart_sigma_spin_->value();
    options.minimum_height_deviation = smart_deviation_spin_->value();
    options.smoothing_strength = smart_smoothing_spin_->value();
    const PointCloud cloud = current_cloud_;
    auto report = std::make_shared<PointCloudDenoiseReport>();
    runCloudTask(tr("智能滤波去噪"), [cloud, options, report] {
        return PointCloudProcessor::SmartDenoise(cloud, options, report.get());
    }, [this, report] {
        smart_filter_report_->setText(tr("飞点 %1，毛刺 %2，保边平滑 %3 点")
            .arg(report->removed_isolated).arg(report->removed_spikes).arg(report->smoothed_points));
    });
}

void PointCloudDialog::applyHoleRepair()
{
    if (current_cloud_.Empty()) return;
    PointCloudHoleRepairOptions options;
    options.grid_spacing = repair_spacing_spin_->value();
    options.maximum_hole_cells = static_cast<std::size_t>(repair_max_cells_spin_->value());
    options.search_radius_cells = repair_search_spin_->value();
    const PointCloud cloud = current_cloud_;
    auto report = std::make_shared<PointCloudHoleRepairReport>();
    runCloudTask(tr("死角空洞修复"), [cloud, options, report] {
        return PointCloudProcessor::RepairHoles(cloud, options, report.get());
    }, [this, report] {
        hole_repair_report_->setText(report->applicable
            ? tr("检测 %1 处，修复 %2 处，新增 %3 点，跳过大空洞 %4 处")
                  .arg(report->detected_holes).arg(report->filled_holes)
                  .arg(report->filled_points).arg(report->skipped_large_holes)
            : errorText(report->message));
    });
}

void PointCloudDialog::beginInteractiveCrop()
{
    if (current_cloud_.Empty()) {
        begin_crop_button_->setChecked(false);
        QMessageBox::information(this, tr("交互式裁剪"), tr("请先打开点云数据。"));
        return;
    }
    const bool active = begin_crop_button_->isChecked();
    if (active) {
        setInteractionMode(PointCloudInteractionMode::Select);
        begin_crop_button_->setChecked(true);
    } else {
        setInteractionMode(PointCloudInteractionMode::Browse);
    }
    cloud_widget_->setSelectionPreviewIndices(crop_selection_);
    cloud_widget_->setFreeSelectionEnabled(active);
    if (active && crop_selection_.isEmpty()) {
        crop_selection_label_->setText(
            tr("在点云视图中按住左键沿目标轮廓绘制自由选区。"));
    }
    updateSelectionPresentation();
    updateActionStates();
}

void PointCloudDialog::acceptBoxSelection(const QVector<int>& indices)
{
    const Qt::KeyboardModifiers modifiers = cloud_widget_->selectionModifiers();
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        QSet<int> combined;
        for (int index : crop_selection_) combined.insert(index);
        for (int index : indices) combined.insert(index);
        crop_selection_.clear();
        crop_selection_.reserve(combined.size());
        for (int index : combined) crop_selection_.push_back(index);
        std::sort(crop_selection_.begin(), crop_selection_.end());
    } else if (modifiers.testFlag(Qt::ControlModifier)) {
        QSet<int> removed;
        for (int index : indices) removed.insert(index);
        crop_selection_.erase(std::remove_if(crop_selection_.begin(), crop_selection_.end(),
            [&removed](int index) { return removed.contains(index); }), crop_selection_.end());
    } else {
        crop_selection_ = indices;
    }
    cloud_widget_->setSelectionPreviewIndices(crop_selection_);
    begin_crop_button_->setChecked(false);
    cloud_widget_->setBoxSelectionEnabled(false);
    const bool continue_selecting = free_selection_button_ && free_selection_button_->isChecked();
    cloud_widget_->setFreeSelectionEnabled(continue_selecting);
    const bool valid = !crop_selection_.isEmpty();
    keep_crop_button_->setEnabled(valid);
    remove_crop_button_->setEnabled(valid && crop_selection_.size() < current_cloud_.Size());
    crop_selection_label_->setText(valid
        ? tr("已选择 %1 / %2 个点；Shift 添加，Ctrl 移除。")
              .arg(crop_selection_.size()).arg(current_cloud_.Size())
        : tr("选区中没有点，请重新拖框。"));
    updateSelectionPresentation();
}

void PointCloudDialog::applyInteractiveCrop(bool keep_selected)
{
    if (crop_selection_.isEmpty()) return;
    std::vector<std::size_t> indices;
    indices.reserve(crop_selection_.size());
    for (int index : crop_selection_) {
        if (index >= 0) indices.push_back(static_cast<std::size_t>(index));
    }
    const PointCloud cloud = current_cloud_;
    const QString operation = keep_selected ? tr("保留选区内点") : tr("保留选区外点");
    runCloudTask(operation, [cloud, indices, keep_selected] {
        return PointCloudProcessor::SelectIndices(cloud, indices, keep_selected);
    });
}

void PointCloudDialog::clearInteractiveCrop()
{
    crop_selection_.clear();
    if (cloud_widget_) {
        cloud_widget_->setBoxSelectionEnabled(false);
        cloud_widget_->setFreeSelectionEnabled(false);
        cloud_widget_->setSelectionPreviewIndices({});
    }
    if (begin_crop_button_) begin_crop_button_->setChecked(false);
    if (navigation_button_ && free_selection_button_) {
        const QSignalBlocker navigation_blocker(navigation_button_);
        const QSignalBlocker selection_blocker(free_selection_button_);
        navigation_button_->setChecked(true);
        free_selection_button_->setChecked(false);
    }
    if (browse_action_ && select_action_) {
        browse_action_->setChecked(true);
        select_action_->setChecked(false);
    }
    if (interaction_mode_ == PointCloudInteractionMode::Select) {
        interaction_mode_ = PointCloudInteractionMode::Browse;
    }
    if (keep_crop_button_) keep_crop_button_->setEnabled(false);
    if (remove_crop_button_) remove_crop_button_->setEnabled(false);
    if (crop_selection_label_) crop_selection_label_->setText(tr("尚未选择点"));
    updateSelectionPresentation();
}

void PointCloudDialog::updateSelectionPresentation()
{
    if (selection_status_) {
        selection_status_->setText(tr("选择 %1 / %2")
            .arg(crop_selection_.size()).arg(current_cloud_.Size()));
    }
    if (fit_scope_combo_) {
        fit_scope_combo_->setItemText(1, tr("当前选择（%1 点）").arg(crop_selection_.size()));
    }
    if (crop_selection_label_) {
        if (!crop_selection_.isEmpty()) {
            crop_selection_label_->setText(
                tr("已选择 %1 / %2 个点；Shift 添加，Ctrl 移除。")
                    .arg(crop_selection_.size()).arg(current_cloud_.Size()));
        } else if (cloud_widget_ && cloud_widget_->freeSelectionEnabled()) {
            crop_selection_label_->setText(
                tr("在点云视图中按住左键沿目标轮廓绘制自由选区。"));
        } else {
            crop_selection_label_->setText(tr("尚未选择点"));
        }
    }
    updateActionStates();
}

void PointCloudDialog::updateActionStates()
{
    const bool has_cloud = !current_cloud_.Empty();
    const bool can_operate = has_cloud && !task_running_;
    const int selection_count = crop_selection_.size();
    if (open_action_) open_action_->setEnabled(!task_running_);
    if (select_action_) select_action_->setEnabled(can_operate);
    if (clear_selection_action_) {
        clear_selection_action_->setEnabled(selection_count > 0 && !task_running_);
    }
    if (undo_action_) undo_action_->setEnabled(!undo_stack_.empty() && !task_running_);
    if (redo_action_) redo_action_->setEnabled(!redo_stack_.empty() && !task_running_);
    if (section_action_) section_action_->setEnabled(can_operate);
    if (toolbar_fit_button_) toolbar_fit_button_->setEnabled(can_operate && !fit_running_);
    if (toolbar_measure_button_) toolbar_measure_button_->setEnabled(can_operate);
    if (toolbar_section_button_) toolbar_section_button_->setEnabled(can_operate);
    const QString unavailable_reason = task_running_
        ? tr("后台任务完成后可用") : tr("请先打开点云数据");
    if (select_action_) select_action_->setToolTip(can_operate
        ? tr("绘制自由选区；Shift 添加，Ctrl 移除") : unavailable_reason);
    if (toolbar_fit_button_) toolbar_fit_button_->setToolTip(can_operate
        ? tr("选择平面、球或圆柱拟合") : unavailable_reason);
    if (toolbar_measure_button_) toolbar_measure_button_->setToolTip(can_operate
        ? tr("选择三维测量工具") : unavailable_reason);
    if (section_action_) section_action_->setToolTip(can_operate
        ? tr("在点云表面绘制一条持久化三维剖线") : unavailable_reason);
    for (QWidget* widget : findChildren<QWidget*>()) {
        if (widget->property("requiresCloud").toBool()) widget->setEnabled(can_operate);
        if (widget->property("requiresMeasurements").toBool()) {
            widget->setEnabled(!measurements_.empty() && !task_running_);
        }
        if (widget->property("requiresSections").toBool()) {
            widget->setEnabled(!section_profiles_.empty() && !task_running_);
        }
    }
    if (clear_selection_button_) clear_selection_button_->setEnabled(selection_count > 0 && !task_running_);
    if (keep_crop_button_) keep_crop_button_->setEnabled(selection_count > 0 && can_operate);
    if (remove_crop_button_) {
        remove_crop_button_->setEnabled(selection_count > 0 && can_operate &&
            selection_count < static_cast<int>(current_cloud_.Size()));
    }

    const bool selection_scope = fit_scope_combo_ &&
        static_cast<PointCloudFitScope>(fit_scope_combo_->currentData().toInt()) ==
            PointCloudFitScope::Selection;
    const std::size_t fit_sample_count = selection_scope
        ? static_cast<std::size_t>(selection_count) : current_cloud_.Size();
    auto update_fit_button = [this, has_cloud, can_operate, selection_scope, fit_sample_count](
                                 QPushButton* button, std::size_t minimum_points,
                                 const QString& model_name) {
        if (!button) return;
        const bool enough_points = fit_sample_count >= minimum_points;
        button->setEnabled(can_operate && enough_points && !fit_running_);
        if (!has_cloud) {
            button->setToolTip(tr("请先打开点云数据"));
        } else if (!enough_points) {
            button->setToolTip(selection_scope
                ? tr("拟合%1至少需要选择 %2 个点").arg(model_name).arg(minimum_points)
                : tr("拟合%1至少需要 %2 个有效点").arg(model_name).arg(minimum_points));
        } else {
            button->setToolTip(selection_scope
                ? tr("使用当前选择的 %1 个点拟合%2").arg(fit_sample_count).arg(model_name)
                : tr("使用整个点云拟合%1").arg(model_name));
        }
    };
    update_fit_button(fit_plane_model_button_, 3, tr("平面"));
    update_fit_button(fit_sphere_button_, 4, tr("球"));
    update_fit_button(fit_cylinder_button_, 6, tr("圆柱"));
    if (cancel_fit_button_) {
        cancel_fit_button_->setVisible(fit_running_);
        cancel_fit_button_->setEnabled(fit_running_);
    }

    const int model_row = model_list_ ? model_list_->currentRow() : -1;
    const bool model_selected = model_row >= 0 &&
        model_row < static_cast<int>(geometric_models_.size());
    const bool model_visible = model_selected &&
        geometric_models_[static_cast<std::size_t>(model_row)].visible;
    if (show_model_button_) show_model_button_->setEnabled(model_selected && !model_visible && !task_running_);
    if (hide_model_button_) hide_model_button_->setEnabled(model_selected && model_visible && !task_running_);
    if (delete_model_button_) delete_model_button_->setEnabled(model_selected && !task_running_);
    if (clear_models_button_) clear_models_button_->setEnabled(!geometric_models_.empty() && !task_running_);
    if (residual_coloring_check_) {
        residual_coloring_check_->setEnabled(model_selected);
        if (!model_selected && residual_coloring_check_->isChecked()) {
            const QSignalBlocker blocker(residual_coloring_check_);
            residual_coloring_check_->setChecked(false);
            cloud_widget_->setResidualColoringEnabled(false);
        }
    }

    const int measurement_row = measurement_list_ ? measurement_list_->currentRow() : -1;
    const bool measurement_selected = measurement_row >= 0 &&
        measurement_row < static_cast<int>(measurements_.size());
    if (delete_measurement_button_) {
        delete_measurement_button_->setEnabled(measurement_selected && !task_running_);
    }
    if (clear_measurements_button_) {
        clear_measurements_button_->setEnabled(!measurements_.empty() && !task_running_);
    }
    if (export_measurements_button_) {
        export_measurements_button_->setEnabled(!measurements_.empty() && !task_running_);
    }
}

void PointCloudDialog::clearFittedPlane()
{
    fitted_plane_ = {};
    reference_plane_model_id_ = 0;
    if (cloud_widget_) {
        cloud_widget_->setFittedPlane({});
        cloud_widget_->setFittedPlaneVisible(false);
    }
    if (show_plane_check_) {
        const QSignalBlocker blocker(show_plane_check_);
        show_plane_check_->setChecked(false);
        show_plane_check_->setEnabled(false);
    }
    if (plane_label_) plane_label_->setText(tr("尚未拟合参考平面"));
    if (level_button_) level_button_->setEnabled(false);
}

std::vector<std::size_t> PointCloudDialog::selectedSourceIndices() const
{
    std::vector<std::size_t> indices;
    indices.reserve(crop_selection_.size());
    for (int index : crop_selection_) {
        if (index >= 0 && index < static_cast<int>(current_cloud_.Size())) {
            indices.push_back(static_cast<std::size_t>(index));
        }
    }
    return indices;
}

void PointCloudDialog::clearGeometricModels()
{
    const bool clears_reference_plane = reference_plane_model_id_ != 0;
    for (auto& profile : section_profiles_) {
        if (profile.definition.reference == PointCloudSectionReference::ReferencePlane &&
            profile.definition.reference_model_id != 0) {
            profile.definition.reference_detached = true;
            profile.definition.reference_model_id = 0;
        }
    }
    geometric_models_.clear();
    active_model_id_ = 0;
    plane_model_count_ = sphere_model_count_ = cylinder_model_count_ = 0;
    if (cloud_widget_) {
        cloud_widget_->setGeometricModels({});
        cloud_widget_->setActiveGeometricModel(0);
    }
    if (clears_reference_plane) clearFittedPlane();
    refreshModelList();
    updateActionStates();
    updateSectionPresentation();
}

void PointCloudDialog::fitGeometricModel(PointCloudGeometricModelType type)
{
    if (current_cloud_.Empty() || fit_running_) return;
    PointCloudFitOptions options;
    options.scope = static_cast<PointCloudFitScope>(fit_scope_combo_->currentData().toInt());
    options.cylinder_axis = static_cast<PointCloudCylinderAxisConstraint>(
        cylinder_axis_combo_->currentData().toInt());
    options.inlier_threshold = fit_threshold_spin_->value();
    options.minimum_radius = minimum_radius_spin_->value();
    options.maximum_radius = maximum_radius_spin_->value();
    const std::vector<std::size_t> indices = options.scope == PointCloudFitScope::Selection
        ? selectedSourceIndices() : std::vector<std::size_t>{};
    const std::size_t minimum_points = type == PointCloudGeometricModelType::Sphere ? 4
        : type == PointCloudGeometricModelType::Cylinder ? 6 : 3;
    if (options.scope == PointCloudFitScope::Selection && indices.size() < minimum_points) {
        setInlineStatus(fit_status_,
            tr("当前选择不足：该模型至少需要 %1 个点。").arg(minimum_points),
            QStringLiteral("error"));
        return;
    }
    if (options.minimum_radius > 0.0 && options.maximum_radius > 0.0 &&
        options.minimum_radius > options.maximum_radius) {
        setInlineStatus(fit_status_,
            tr("半径约束冲突：最小半径不能大于最大半径。"),
            QStringLiteral("error"));
        return;
    }
    const PointCloud cloud = current_cloud_;
    const std::uint64_t revision = cloud_revision_;
    const std::uint64_t request_id = ++fit_request_id_;
    fit_running_ = true;
    beginInlineTask(tr("拟合几何模型"));
    setInlineStatus(fit_status_, tr("正在后台拟合；可继续浏览点云或取消任务…"),
        QStringLiteral("working"));
    updateActionStates();
    auto* watcher = new QFutureWatcher<PointCloudFitResult>(this);
    connect(watcher, &QFutureWatcher<PointCloudFitResult>::finished, this,
        [this, watcher, revision, request_id] {
            PointCloudFitResult result = watcher->result();
            watcher->deleteLater();
            if (request_id != fit_request_id_) return;
            fit_running_ = false;
            updateActionStates();
            const bool succeeded = result.valid;
            acceptFitResult(std::move(result), revision);
            endInlineTask(succeeded ? tr("几何拟合完成") : tr("几何拟合失败"),
                succeeded ? QStringLiteral("ok") : QStringLiteral("error"));
        });
    watcher->setFuture(QtConcurrent::run([cloud, indices, options, type] {
        if (type == PointCloudGeometricModelType::Plane) {
            return PointCloudGeometricFitter::FitPlane(cloud, indices, options);
        }
        if (type == PointCloudGeometricModelType::Sphere) {
            return PointCloudGeometricFitter::FitSphere(cloud, indices, options);
        }
        return PointCloudGeometricFitter::FitCylinder(cloud, indices, options);
    }));
}

void PointCloudDialog::cancelActiveFit(const QString& message)
{
    if (!fit_running_) return;
    ++fit_request_id_;
    fit_running_ = false;
    setInlineStatus(fit_status_, message, QStringLiteral("warning"));
    if (task_running_ && !active_task_cancel_token_) {
        endInlineTask(message, QStringLiteral("warning"));
    }
    updateActionStates();
}

void PointCloudDialog::acceptFitResult(PointCloudFitResult result, std::uint64_t revision)
{
    if (revision != cloud_revision_) {
        setInlineStatus(fit_status_, tr("点云已改变，本次拟合结果已丢弃。"),
            QStringLiteral("warning"));
        return;
    }
    if (!result.valid) {
        setInlineStatus(fit_status_, tr("拟合失败：%1").arg(errorText(result.error)),
            QStringLiteral("error"));
        return;
    }
    result.model.id = next_model_id_++;
    int number = 0;
    QString type_name;
    if (result.model.type == PointCloudGeometricModelType::Plane) {
        number = ++plane_model_count_; type_name = tr("平面");
    } else if (result.model.type == PointCloudGeometricModelType::Sphere) {
        number = ++sphere_model_count_; type_name = tr("球");
    } else {
        number = ++cylinder_model_count_; type_name = tr("圆柱");
    }
    result.model.name = tr("%1 %2").arg(type_name).arg(number).toStdWString();
    geometric_models_.push_back(std::move(result.model));
    active_model_id_ = geometric_models_.back().id;
    if (geometric_models_.back().type == PointCloudGeometricModelType::Plane) {
        fitted_plane_ = geometric_models_.back().plane;
        reference_plane_model_id_ = geometric_models_.back().id;
        cloud_widget_->setFittedPlane(fitted_plane_);
        cloud_widget_->setFittedPlaneVisible(false);
        if (show_plane_check_) {
            const QSignalBlocker blocker(show_plane_check_);
            show_plane_check_->setChecked(false);
            show_plane_check_->setEnabled(false);
        }
        if (plane_label_) {
            plane_label_->setText(tr("当前参考：%1\nRMS：%2 %3")
                .arg(QString::fromStdWString(geometric_models_.back().name))
                .arg(fitted_plane_.rms, 0, 'g', 7).arg(unitLabel()));
        }
        if (level_button_) level_button_->setEnabled(true);
    }
    cloud_widget_->setGeometricModels(geometric_models_);
    cloud_widget_->setActiveGeometricModel(active_model_id_);
    refreshModelList();
    model_list_->setCurrentRow(static_cast<int>(geometric_models_.size()) - 1);
    setInlineStatus(fit_status_, tr("拟合完成：内点 %1/%2，RMS %3 %4")
        .arg(geometric_models_.back().quality.inlier_count)
        .arg(geometric_models_.back().quality.sample_count)
        .arg(geometric_models_.back().quality.rms, 0, 'g', 7).arg(unitLabel()),
        QStringLiteral("ok"));
    updateActionStates();
}

void PointCloudDialog::refreshModelList()
{
    if (!model_list_) return;
    const int old_row = model_list_->currentRow();
    const QSignalBlocker blocker(model_list_);
    model_list_->clear();
    for (const auto& model : geometric_models_) {
        const QString status = model.visible ? tr("显示") : tr("隐藏");
        const QString quality_badge = !model.quality.valid ? QStringLiteral("●")
            : model.quality.inlier_ratio >= 0.9 ? QStringLiteral("●")
            : model.quality.inlier_ratio >= 0.65 ? QStringLiteral("▲")
            : QStringLiteral("●");
        auto* item = new QListWidgetItem(tr("%1 %2 · %3 · RMS %4")
            .arg(quality_badge, QString::fromStdWString(model.name), status)
            .arg(model.quality.rms, 0, 'g', 5));
        item->setForeground(!model.quality.valid || model.quality.inlier_ratio < 0.65
            ? QColor(QStringLiteral("#ff7272"))
            : model.quality.inlier_ratio < 0.9
                ? QColor(QStringLiteral("#f6be5c"))
                : QColor(QStringLiteral("#8ed8ff")));
        model_list_->addItem(item);
    }
    int selected_row = -1;
    if (!geometric_models_.empty()) {
        selected_row = std::clamp(old_row, 0,
            static_cast<int>(geometric_models_.size()) - 1);
        model_list_->setCurrentRow(selected_row);
    } else if (model_details_) {
        model_details_->setText(tr("尚未创建模型"));
    }
    selectModelRow(selected_row);
}

void PointCloudDialog::selectModelRow(int row)
{
    if (row < 0 || row >= static_cast<int>(geometric_models_.size())) {
        active_model_id_ = 0;
        cloud_widget_->setActiveGeometricModel(0);
        if (model_details_) model_details_->setText(tr("尚未选择模型"));
        updateActionStates();
        return;
    }
    const PointCloudGeometricModel& model = geometric_models_[static_cast<std::size_t>(row)];
    active_model_id_ = model.id;
    cloud_widget_->setActiveGeometricModel(active_model_id_);
    QString parameters;
    if (model.type == PointCloudGeometricModelType::Plane) {
        fitted_plane_ = model.plane;
        reference_plane_model_id_ = model.id;
        cloud_widget_->setFittedPlane(fitted_plane_);
        cloud_widget_->setFittedPlaneVisible(false);
        if (show_plane_check_) {
            const QSignalBlocker blocker(show_plane_check_);
            show_plane_check_->setChecked(false);
            show_plane_check_->setEnabled(false);
        }
        if (level_button_) level_button_->setEnabled(true);
        if (plane_label_) {
            plane_label_->setText(tr("当前参考：%1\nRMS：%2 %3")
                .arg(QString::fromStdWString(model.name))
                .arg(model.plane.rms, 0, 'g', 7).arg(unitLabel()));
        }
        parameters = tr("平面：%1x + %2y + %3z + %4 = 0")
            .arg(model.plane.nx, 0, 'g', 7).arg(model.plane.ny, 0, 'g', 7)
            .arg(model.plane.nz, 0, 'g', 7).arg(model.plane.d, 0, 'g', 7);
    } else if (model.type == PointCloudGeometricModelType::Sphere) {
        parameters = tr("球心：(%1, %2, %3)\n半径：%4 %5")
            .arg(model.sphere.center.x, 0, 'g', 7).arg(model.sphere.center.y, 0, 'g', 7)
            .arg(model.sphere.center.z, 0, 'g', 7).arg(model.sphere.radius, 0, 'g', 7).arg(unitLabel());
    } else {
        parameters = tr("轴点：(%1, %2, %3)\n轴向：(%4, %5, %6)\n半径：%7 %8")
            .arg(model.cylinder.axis_point.x, 0, 'g', 7)
            .arg(model.cylinder.axis_point.y, 0, 'g', 7)
            .arg(model.cylinder.axis_point.z, 0, 'g', 7)
            .arg(model.cylinder.axis_direction[0], 0, 'g', 6)
            .arg(model.cylinder.axis_direction[1], 0, 'g', 6)
            .arg(model.cylinder.axis_direction[2], 0, 'g', 6)
            .arg(model.cylinder.radius, 0, 'g', 7).arg(unitLabel());
    }
    model_details_->setText(tr("%1\n%2\n内点：%3/%4（%5%）\n均值 %6，σ %7，RMS %8 %9")
        .arg(QString::fromStdWString(model.name), parameters)
        .arg(model.quality.inlier_count).arg(model.quality.sample_count)
        .arg(model.quality.inlier_ratio * 100.0, 0, 'f', 1)
        .arg(model.quality.mean, 0, 'g', 6).arg(model.quality.standard_deviation, 0, 'g', 6)
        .arg(model.quality.rms, 0, 'g', 6).arg(unitLabel()));
    updateActionStates();
}

void PointCloudDialog::setSelectedModelVisible(bool visible)
{
    const int row = model_list_ ? model_list_->currentRow() : -1;
    if (row < 0 || row >= static_cast<int>(geometric_models_.size())) return;
    geometric_models_[static_cast<std::size_t>(row)].visible = visible;
    cloud_widget_->setGeometricModels(geometric_models_);
    refreshModelList();
    model_list_->setCurrentRow(row);
    updateActionStates();
}

void PointCloudDialog::deleteSelectedModel()
{
    const int row = model_list_ ? model_list_->currentRow() : -1;
    if (row < 0 || row >= static_cast<int>(geometric_models_.size())) return;
    const std::uint64_t deleted_id = geometric_models_[static_cast<std::size_t>(row)].id;
    const bool deletes_reference_plane = deleted_id == reference_plane_model_id_;
    for (auto& profile : section_profiles_) {
        if (profile.definition.reference_model_id == deleted_id) {
            profile.definition.reference_model_id = 0;
            profile.definition.reference_detached = true;
            profile.warnings.push_back(L"The reference model was removed; its plane snapshot is being used.");
        }
    }
    geometric_models_.erase(geometric_models_.begin() + row);
    if (deletes_reference_plane) clearFittedPlane();
    active_model_id_ = 0;
    cloud_widget_->setGeometricModels(geometric_models_);
    refreshModelList();
    updateActionStates();
    updateSectionPresentation();
}

void PointCloudDialog::evaluateTolerances()
{
    if (current_cloud_.Empty()) return;
    setMeasureMode(PointCloudMeasureMode::Navigate);
    PointCloudToleranceLimits limits;
    limits.flatness = flatness_tolerance_->value();
    limits.cylindricity = cylindricity_tolerance_->value();
    limits.circularity = circularity_tolerance_->value();
    limits.warpage = warpage_tolerance_->value();
    limits.profile = profile_tolerance_->value();
    const PointCloudToleranceReport report =
        PointCloudMetrology::EvaluateTolerances(current_cloud_, limits);
    QStringList lines;
    const QStringList names{tr("平面度"), tr("圆柱度"), tr("真圆度"),
        tr("翘曲度"), tr("轮廓度")};
    measurements_.erase(std::remove_if(measurements_.begin(), measurements_.end(),
        [&names](const PointCloudMeasurementRecord& record) {
            return names.contains(record.type);
        }), measurements_.end());
    for (int index = 0; index < static_cast<int>(report.metrics.size()); ++index) {
        const auto& metric = report.metrics[static_cast<std::size_t>(index)];
        const QString color = !metric.valid ? QStringLiteral("#8b9aae")
            : metric.passed ? QStringLiteral("#55d98d") : QStringLiteral("#ff7272");
        const QString status = !metric.valid ? tr("数据不适用")
            : metric.passed ? tr("合格") : tr("超差");
        lines << (metric.valid
            ? tr("<span style='color:%1'>●</span> <b>%2</b>　%3 / %4 %5　%6")
                  .arg(color, names.value(index)).arg(metric.measured, 0, 'g', 8)
                  .arg(metric.tolerance, 0, 'g', 8).arg(unitLabel(), status)
            : tr("<span style='color:%1'>●</span> <b>%2</b>　%3")
                  .arg(color, names.value(index), status));
        PointCloudMeasurementRecord record;
        record.type = names.value(index);
        record.value = metric.valid
            ? tr("%1 %2，公差 %3，%4")
                  .arg(metric.measured, 0, 'g', 9).arg(unitLabel())
                  .arg(metric.tolerance, 0, 'g', 9)
                  .arg(metric.passed ? tr("合格") : tr("超差"))
            : tr("数据不适用");
        measurements_.push_back(std::move(record));
    }
    tolerance_summary_->setText(lines.join(QStringLiteral("<br>")));
    refreshMeasurementList();
}

void PointCloudDialog::showDeviationDistribution()
{
    if (current_cloud_.Empty()) return;
    setMeasureMode(PointCloudMeasureMode::Navigate);
    PointCloudDeviationDistribution distribution;
    const auto active = std::find_if(geometric_models_.begin(), geometric_models_.end(),
        [this](const auto& model) { return model.id == active_model_id_; });
    if (active != geometric_models_.end()) {
        const std::vector<std::size_t> indices = crop_selection_.isEmpty()
            ? active->source_indices : selectedSourceIndices();
        distribution = PointCloudDeviationAnalyzer::Analyze(current_cloud_, *active, indices);
    } else {
        if (!fitted_plane_.valid) fitPlane();
        if (!fitted_plane_.valid) {
            tolerance_summary_->setText(tr("请先在“拟合”页选择参考模型。"));
            return;
        }
        distribution = PointCloudDeviationAnalyzer::Analyze(current_cloud_, fitted_plane_);
    }
    if (!distribution.valid) {
        QMessageBox::information(this, tr("偏差高斯分布"),
            tr("有效偏差样本不足，无法生成分布。"));
        return;
    }
    auto* dialog = new PointCloudDeviationDialog(
        std::move(distribution), unitLabel(), this);
    dialog->show();
    dialog->raise();
}

void PointCloudDialog::beginSectionAnalysis()
{
    if (current_cloud_.Empty()) {
        setInlineStatus(section_status_, tr("请先打开点云数据。"), QStringLiteral("warning"));
        return;
    }
    setInteractionMode(PointCloudInteractionMode::SectionDraw);
    setInlineStatus(section_status_,
        tr("选择中：请从点云表面按住左键拉出三维剖线，Esc 可取消。"),
        QStringLiteral("busy"));
}

void PointCloudDialog::acceptSectionSelection(
    const QVector<int>& indices,
    const QPointF& first,
    const QPointF& second)
{
    int first_index = cloud_widget_->pickNearest(first, 32.0);
    int second_index = cloud_widget_->pickNearest(second, 32.0);
    if ((first_index < 0 || second_index < 0) && !indices.isEmpty()) {
        double first_distance = std::numeric_limits<double>::infinity();
        double second_distance = std::numeric_limits<double>::infinity();
        for (int index : indices) {
            if (index < 0 || index >= static_cast<int>(current_cloud_.points.size())) continue;
            const QPointF screen = cloud_widget_->screenPosition(index);
            const double first_candidate = QLineF(screen, first).length();
            const double second_candidate = QLineF(screen, second).length();
            if (first_candidate < first_distance) {
                first_distance = first_candidate; first_index = index;
            }
            if (second_candidate < second_distance) {
                second_distance = second_candidate; second_index = index;
            }
        }
    }
    if (first_index < 0 || second_index < 0 || first_index == second_index) {
        setInlineStatus(section_status_,
            tr("无法可靠吸附剖线端点，请从可见点云表面重新绘制。"),
            QStringLiteral("warning"));
        setInteractionMode(PointCloudInteractionMode::SectionDraw);
        return;
    }
    PointCloudSectionDefinition definition;
    definition.id = next_section_id_++;
    definition.name = tr("剖线 %1").arg(definition.id).toStdWString();
    definition.start = current_cloud_.points[static_cast<std::size_t>(first_index)];
    definition.end = current_cloud_.points[static_cast<std::size_t>(second_index)];
    definition.band_width = section_width_spin_->value();
    definition.sample_spacing = section_spacing_spin_->value();
    definition.reference = static_cast<PointCloudSectionReference>(
        section_reference_combo_->currentData().toInt());
    const std::array<std::uint32_t, 6> colors{
        0x4da6ff, 0xffb34d, 0x65d58b, 0xc38cff, 0xff7185, 0x55d5cf};
    definition.color_rgb = colors[(definition.id - 1) % colors.size()];
    if (definition.reference == PointCloudSectionReference::ReferencePlane) {
        const auto model = std::find_if(geometric_models_.begin(), geometric_models_.end(),
            [this](const auto& value) {
                return value.id == active_model_id_ &&
                    value.type == PointCloudGeometricModelType::Plane && value.plane.valid;
            });
        if (model != geometric_models_.end()) {
            definition.reference_model_id = model->id;
            definition.reference_plane = model->plane;
        } else if (fitted_plane_.valid) {
            definition.reference_plane = fitted_plane_;
        } else {
            definition.reference = PointCloudSectionReference::WorldZ;
            section_reference_combo_->setCurrentIndex(section_reference_combo_->findData(
                static_cast<int>(PointCloudSectionReference::WorldZ)));
            setInlineStatus(section_status_,
                tr("当前没有有效平面，已改用世界 Z 作为高度参考。"),
                QStringLiteral("warning"));
        }
    }
    active_section_id_ = definition.id;
    setInteractionMode(PointCloudInteractionMode::Browse);
    setCompactDrawerOpen(true, true);
    runSectionAnalysis(std::move(definition));
}

void PointCloudDialog::runSectionAnalysis(PointCloudSectionDefinition definition, bool preview)
{
    if (current_cloud_.Empty()) return;
    if (section_cancel_token_) section_cancel_token_->store(true, std::memory_order_relaxed);
    section_cancel_token_ = std::make_shared<std::atomic_bool>(false);
    const auto cancel = section_cancel_token_;
    const std::uint64_t request_id = ++section_request_id_;
    const std::uint64_t revision = cloud_revision_;
    PointCloudSectionAnalysisOptions options;
    options.median_window = static_cast<std::size_t>(section_median_window_spin_->value() | 1);
    options.smoothing_window = static_cast<std::size_t>(section_smoothing_window_spin_->value() | 1);
    options.maximum_gap_bins = static_cast<std::size_t>(section_gap_spin_->value());
    options.detection_sensitivity = section_sensitivity_spin_->value();
    options.minimum_plateau_bins = static_cast<std::size_t>(section_plateau_spin_->value());
    options.preview = preview;
    const auto existing = std::find_if(section_profiles_.begin(), section_profiles_.end(),
        [&definition](const auto& value) { return value.definition.id == definition.id; });
    if (existing == section_profiles_.end()) {
        PointCloudSectionProfile pending;
        pending.definition = definition;
        section_profiles_.push_back(std::move(pending));
        refreshSectionList(static_cast<int>(section_profiles_.size()) - 1);
    } else {
        existing->definition = definition;
    }
    setInlineStatus(section_status_, tr("正在分析剖线；可继续旋转和缩放点云…"),
        QStringLiteral("busy"));
    updateSectionPresentation();
    // A stable snapshot avoids races when the user replaces or processes the cloud.
    auto cloud = std::make_shared<PointCloud>(current_cloud_);
    auto* watcher = new QFutureWatcher<PointCloudSectionProfile>(this);
    connect(watcher, &QFutureWatcher<PointCloudSectionProfile>::finished, this,
        [this, watcher, revision, request_id] {
            PointCloudSectionProfile result = watcher->result();
            watcher->deleteLater();
            acceptSectionProfile(std::move(result), revision, request_id);
        });
    watcher->setFuture(QtConcurrent::run([cloud, definition, options, cancel] {
        return PointCloudSectionAnalyzer::Analyze(*cloud, definition, options, cancel.get());
    }));
}

void PointCloudDialog::acceptSectionProfile(
    PointCloudSectionProfile profile, std::uint64_t revision, std::uint64_t request_id)
{
    if (revision != cloud_revision_ || request_id != section_request_id_ || profile.cancelled) return;
    const auto existing = std::find_if(section_profiles_.begin(), section_profiles_.end(),
        [&profile](const auto& value) { return value.definition.id == profile.definition.id; });
    if (existing == section_profiles_.end()) return;
    *existing = std::move(profile);
    const int row = static_cast<int>(std::distance(section_profiles_.begin(), existing));
    refreshSectionList(row);
    if (!existing->valid) {
        setInlineStatus(section_status_,
            tr("剖线分析失败：带宽内有效点不足或端点退化。"), QStringLiteral("error"));
        return;
    }
    section_width_spin_->setSuffix(QStringLiteral(" ") + unitLabel());
    section_spacing_spin_->setSuffix(QStringLiteral(" ") + unitLabel());
    setInlineStatus(section_status_,
        tr("已分析 %1 个原始点、%2 个采样格，识别 %3 个特征。")
            .arg(existing->raw_points.size()).arg(existing->samples.size())
            .arg(existing->features.size()), QStringLiteral("ok"));
    setCompactDrawerOpen(true, true);
    updateSectionEditors();
    refreshSectionFeatureList();
    updateSectionPresentation();
}

void PointCloudDialog::refreshSectionList(int preferred_row)
{
    if (!section_list_) return;
    if (preferred_row < 0) preferred_row = section_list_->currentRow();
    section_editor_updating_ = true;
    const QSignalBlocker blocker(section_list_);
    section_list_->clear();
    for (const auto& profile : section_profiles_) {
        QString state = profile.valid
            ? tr("%1 点 · %2 特征").arg(profile.raw_points.size()).arg(profile.features.size())
            : tr("等待分析");
        if (profile.definition.reference_detached) state += tr(" · 参考已分离");
        auto* item = new QListWidgetItem(
            tr("%1  —  %2").arg(QString::fromStdWString(profile.definition.name), state));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(profile.definition.visible ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(profile.definition.id));
        section_list_->addItem(item);
    }
    if (!section_profiles_.empty()) {
        if (preferred_row < 0) {
            const auto active = std::find_if(section_profiles_.begin(), section_profiles_.end(),
                [this](const auto& value) { return value.definition.id == active_section_id_; });
            preferred_row = active == section_profiles_.end() ? 0
                : static_cast<int>(std::distance(section_profiles_.begin(), active));
        }
        section_list_->setCurrentRow(std::clamp(preferred_row, 0,
            static_cast<int>(section_profiles_.size()) - 1));
    }
    section_editor_updating_ = false;
    updateSectionPresentation();
}

void PointCloudDialog::selectSectionRow(int row)
{
    if (row < 0 || row >= static_cast<int>(section_profiles_.size())) {
        active_section_id_ = 0;
    } else {
        active_section_id_ = section_profiles_[static_cast<std::size_t>(row)].definition.id;
    }
    updateSectionEditors();
    refreshSectionFeatureList();
    updateSectionPresentation();
}

void PointCloudDialog::updateSectionEditors()
{
    const int row = section_list_ ? section_list_->currentRow() : -1;
    const bool selected = row >= 0 && row < static_cast<int>(section_profiles_.size());
    section_editor_updating_ = true;
    if (selected) {
        const auto& definition = section_profiles_[static_cast<std::size_t>(row)].definition;
        const std::array<double, 6> values{definition.start.x, definition.start.y, definition.start.z,
            definition.end.x, definition.end.y, definition.end.z};
        for (std::size_t index = 0; index < values.size(); ++index)
            section_endpoint_spins_[index]->setValue(values[index]);
        section_width_spin_->setValue(definition.band_width);
        section_spacing_spin_->setValue(definition.sample_spacing);
        section_reference_combo_->setCurrentIndex(section_reference_combo_->findData(
            static_cast<int>(definition.reference)));
        section_visible_check_->setChecked(definition.visible);
        section_report_check_->setChecked(definition.include_in_report);
    }
    for (QDoubleSpinBox* spin : section_endpoint_spins_) spin->setEnabled(selected);
    section_width_spin_->setEnabled(selected || !current_cloud_.Empty());
    section_spacing_spin_->setEnabled(selected || !current_cloud_.Empty());
    section_reference_combo_->setEnabled(selected || !current_cloud_.Empty());
    section_visible_check_->setEnabled(selected);
    section_report_check_->setEnabled(selected);
    section_editor_updating_ = false;
}

void PointCloudDialog::scheduleSectionReanalysis()
{
    if (section_editor_updating_) return;
    const int row = section_list_ ? section_list_->currentRow() : -1;
    if (row < 0 || row >= static_cast<int>(section_profiles_.size())) return;
    auto& definition = section_profiles_[static_cast<std::size_t>(row)].definition;
    definition.start = {section_endpoint_spins_[0]->value(), section_endpoint_spins_[1]->value(),
        section_endpoint_spins_[2]->value()};
    definition.end = {section_endpoint_spins_[3]->value(), section_endpoint_spins_[4]->value(),
        section_endpoint_spins_[5]->value()};
    definition.band_width = section_width_spin_->value();
    definition.sample_spacing = section_spacing_spin_->value();
    definition.visible = section_visible_check_->isChecked();
    definition.include_in_report = section_report_check_->isChecked();
    const auto requested_reference = static_cast<PointCloudSectionReference>(
        section_reference_combo_->currentData().toInt());
    if (requested_reference == PointCloudSectionReference::ReferencePlane) {
        const auto model = std::find_if(geometric_models_.begin(), geometric_models_.end(),
            [this](const auto& value) {
                return value.id == active_model_id_ &&
                    value.type == PointCloudGeometricModelType::Plane && value.plane.valid;
            });
        if (model != geometric_models_.end()) {
            definition.reference = requested_reference;
            definition.reference_model_id = model->id;
            definition.reference_plane = model->plane;
            definition.reference_detached = false;
        } else if (!definition.reference_plane.valid && fitted_plane_.valid) {
            definition.reference = requested_reference;
            definition.reference_plane = fitted_plane_;
        } else if (!definition.reference_plane.valid) {
            section_editor_updating_ = true;
            section_reference_combo_->setCurrentIndex(section_reference_combo_->findData(
                static_cast<int>(PointCloudSectionReference::WorldZ)));
            section_editor_updating_ = false;
            definition.reference = PointCloudSectionReference::WorldZ;
            setInlineStatus(section_status_, tr("没有可用的参考平面，继续使用世界 Z。"),
                QStringLiteral("warning"));
        }
    } else {
        definition.reference = PointCloudSectionReference::WorldZ;
        definition.reference_model_id = 0;
        definition.reference_detached = false;
    }
    updateSectionPresentation();
    const PointCloudSectionDefinition scheduled = definition;
    const std::uint64_t id = definition.id;
    QTimer::singleShot(250, this, [this, scheduled, id] {
        if (active_section_id_ != id) return;
        const auto current = std::find_if(section_profiles_.begin(), section_profiles_.end(),
            [id](const auto& value) { return value.definition.id == id; });
        if (current == section_profiles_.end()) return;
        if (current->definition.start.x != scheduled.start.x ||
            current->definition.start.y != scheduled.start.y ||
            current->definition.start.z != scheduled.start.z ||
            current->definition.end.x != scheduled.end.x ||
            current->definition.end.y != scheduled.end.y ||
            current->definition.end.z != scheduled.end.z ||
            current->definition.band_width != scheduled.band_width ||
            current->definition.sample_spacing != scheduled.sample_spacing ||
            current->definition.reference != scheduled.reference) return;
        runSectionAnalysis(scheduled);
    });
}

void PointCloudDialog::duplicateSection()
{
    const int row = section_list_->currentRow();
    if (row < 0 || row >= static_cast<int>(section_profiles_.size())) return;
    PointCloudSectionDefinition definition = section_profiles_[static_cast<std::size_t>(row)].definition;
    definition.id = next_section_id_++;
    definition.name = tr("剖线 %1").arg(definition.id).toStdWString();
    const std::array<std::uint32_t, 6> colors{0x4da6ff, 0xffb34d, 0x65d58b, 0xc38cff, 0xff7185, 0x55d5cf};
    definition.color_rgb = colors[(definition.id - 1) % colors.size()];
    active_section_id_ = definition.id;
    runSectionAnalysis(std::move(definition));
}

void PointCloudDialog::offsetSection()
{
    const int row = section_list_->currentRow();
    if (row < 0 || row >= static_cast<int>(section_profiles_.size())) return;
    bool accepted = false;
    const double offset = QInputDialog::getDouble(this, tr("平行偏移剖线"),
        tr("偏移距离 (%1)，正值向剖线左侧").arg(unitLabel()),
        section_profiles_[static_cast<std::size_t>(row)].definition.band_width,
        -1e12, 1e12, 6, &accepted);
    if (!accepted) return;
    PointCloudSectionDefinition definition = section_profiles_[static_cast<std::size_t>(row)].definition;
    definition.id = next_section_id_++;
    definition.name = tr("剖线 %1").arg(definition.id).toStdWString();
    double nx = 0.0, ny = 0.0, nz = 1.0;
    if (definition.reference == PointCloudSectionReference::ReferencePlane) {
        nx = definition.reference_plane.nx; ny = definition.reference_plane.ny;
        nz = definition.reference_plane.nz;
    }
    double dx = definition.end.x - definition.start.x;
    double dy = definition.end.y - definition.start.y;
    double dz = definition.end.z - definition.start.z;
    const double normal_length = std::sqrt(nx * nx + ny * ny + nz * nz);
    nx /= normal_length; ny /= normal_length; nz /= normal_length;
    const double normal_component = dx * nx + dy * ny + dz * nz;
    dx -= normal_component * nx; dy -= normal_component * ny; dz -= normal_component * nz;
    const double direction_length = std::sqrt(dx * dx + dy * dy + dz * dz);
    dx /= direction_length; dy /= direction_length; dz /= direction_length;
    const double tx = (ny * dz - nz * dy) * offset;
    const double ty = (nz * dx - nx * dz) * offset;
    const double tz = (nx * dy - ny * dx) * offset;
    definition.start.x += tx; definition.start.y += ty; definition.start.z += tz;
    definition.end.x += tx; definition.end.y += ty; definition.end.z += tz;
    active_section_id_ = definition.id;
    runSectionAnalysis(std::move(definition));
}

void PointCloudDialog::deleteSection()
{
    const int row = section_list_->currentRow();
    if (row < 0 || row >= static_cast<int>(section_profiles_.size())) return;
    if (section_cancel_token_) section_cancel_token_->store(true, std::memory_order_relaxed);
    section_profiles_.erase(section_profiles_.begin() + row);
    active_section_id_ = section_profiles_.empty() ? 0
        : section_profiles_[std::min<std::size_t>(row, section_profiles_.size() - 1)].definition.id;
    refreshSectionList(std::min(row, static_cast<int>(section_profiles_.size()) - 1));
    refreshSectionFeatureList();
}

void PointCloudDialog::clearSections()
{
    if (section_cancel_token_) section_cancel_token_->store(true, std::memory_order_relaxed);
    ++section_request_id_;
    section_profiles_.clear(); active_section_id_ = 0;
    if (section_list_) section_list_->clear();
    if (section_feature_list_) section_feature_list_->clear();
    if (section_status_) setInlineStatus(section_status_,
        tr("绘制三维剖线，分析结果会持续保留在下方工作区。"), QStringLiteral("neutral"));
    updateSectionPresentation();
}

void PointCloudDialog::updateSectionPresentation()
{
    if (!cloud_widget_ || !section_plot_) return;
    std::vector<PointCloudSectionDefinition> definitions;
    definitions.reserve(section_profiles_.size());
    for (const auto& profile : section_profiles_) definitions.push_back(profile.definition);
    cloud_widget_->setSectionDefinitions(definitions, active_section_id_);
    section_plot_->setProfiles(section_profiles_, active_section_id_, unitLabel());
    const bool selected = std::any_of(section_profiles_.begin(), section_profiles_.end(),
        [this](const auto& value) { return value.definition.id == active_section_id_; });
    duplicate_section_button_->setEnabled(selected);
    offset_section_button_->setEnabled(selected);
    delete_section_button_->setEnabled(selected);
    clear_sections_button_->setEnabled(!section_profiles_.empty());
    const bool has_valid = std::any_of(section_profiles_.begin(), section_profiles_.end(),
        [](const auto& value) { return value.valid; });
    export_section_csv_button_->setEnabled(selected && has_valid);
    export_section_png_button_->setEnabled(selected && has_valid);
    export_section_report_button_->setEnabled(has_valid);
    updateActionStates();
    emit sectionReportChanged(buildSectionReportHtml(false));
}

void PointCloudDialog::refreshSectionFeatureList()
{
    section_feature_list_->clear();
    const int row = section_list_ ? section_list_->currentRow() : -1;
    if (row < 0 || row >= static_cast<int>(section_profiles_.size())) return;
    const auto& profile = section_profiles_[static_cast<std::size_t>(row)];
    for (const auto& feature : profile.features) {
        section_feature_list_->addItem(tr("%1 · %2 %3 · 宽 %4 %3%5")
            .arg(sectionFeatureLabel(feature.type))
            .arg(feature.depth_or_height, 0, 'g', 6).arg(unitLabel())
            .arg(feature.opening_width, 0, 'g', 6)
            .arg(feature.automatic ? QString() : tr(" · 人工")));
    }
    if (!profile.features.empty()) section_feature_list_->setCurrentRow(0);
}

void PointCloudDialog::applySectionFeatureEdit()
{
    const int section_row = section_list_->currentRow();
    const int feature_row = section_feature_list_->currentRow();
    if (section_row < 0 || feature_row < 0) return;
    auto& profile = section_profiles_[static_cast<std::size_t>(section_row)];
    if (feature_row >= static_cast<int>(profile.features.size())) return;
    auto& feature = profile.features[static_cast<std::size_t>(feature_row)];
    feature.type = static_cast<PointCloudSectionFeatureType>(
        section_feature_type_combo_->currentData().toInt());
    feature.start_distance = std::clamp(std::min(section_feature_start_spin_->value(),
        section_feature_end_spin_->value()), 0.0, profile.width);
    feature.end_distance = std::clamp(std::max(section_feature_start_spin_->value(),
        section_feature_end_spin_->value()), 0.0, profile.width);
    const auto measurement = PointCloudSectionAnalyzer::MeasureCursors(
        profile, feature.start_distance, feature.end_distance);
    feature.opening_width = measurement.distance_difference;
    feature.area = 0.0;
    if (feature.type == PointCloudSectionFeatureType::Step) {
        feature.signed_height = measurement.height_difference;
        feature.depth_or_height = std::abs(measurement.height_difference);
        feature.half_height_width = feature.opening_width;
        feature.slope_angle_degrees = measurement.slope_angle_degrees;
    } else {
        double extreme = feature.type == PointCloudSectionFeatureType::Groove
            ? std::numeric_limits<double>::infinity()
            : -std::numeric_limits<double>::infinity();
        double half_first = 0.0, half_last = 0.0;
        bool half_started = false;
        for (std::size_t index = 0; index < profile.samples.size(); ++index) {
            const auto& sample = profile.samples[index];
            if (!sample.valid || sample.distance < feature.start_distance ||
                sample.distance > feature.end_distance) continue;
            const double ratio = (sample.distance - feature.start_distance) /
                std::max(feature.opening_width, 1e-12);
            const double baseline = measurement.first_height +
                (measurement.second_height - measurement.first_height) * ratio;
            const double deviation = sample.height - baseline;
            extreme = feature.type == PointCloudSectionFeatureType::Groove
                ? std::min(extreme, deviation) : std::max(extreme, deviation);
            if (index > 0) feature.area += std::abs(deviation) *
                profile.definition.sample_spacing;
        }
        feature.signed_height = std::isfinite(extreme) ? extreme : 0.0;
        feature.depth_or_height = std::abs(feature.signed_height);
        const double half = feature.depth_or_height * 0.5;
        for (const auto& sample : profile.samples) {
            if (!sample.valid || sample.distance < feature.start_distance ||
                sample.distance > feature.end_distance) continue;
            const double ratio = (sample.distance - feature.start_distance) /
                std::max(feature.opening_width, 1e-12);
            const double baseline = measurement.first_height +
                (measurement.second_height - measurement.first_height) * ratio;
            if (std::abs(sample.height - baseline) >= half) {
                if (!half_started) { half_first = sample.distance; half_started = true; }
                half_last = sample.distance;
            }
        }
        feature.half_height_width = half_started ? half_last - half_first : 0.0;
        feature.slope_angle_degrees = 0.0;
    }
    feature.automatic = false;
    refreshSectionFeatureList();
    section_feature_list_->setCurrentRow(feature_row);
    updateSectionPresentation();
}

void PointCloudDialog::updateSectionCursorMetrics(double first_distance, double second_distance)
{
    const auto active = std::find_if(section_profiles_.begin(), section_profiles_.end(),
        [this](const auto& value) { return value.definition.id == active_section_id_; });
    if (active == section_profiles_.end()) return;
    const auto measurement = PointCloudSectionAnalyzer::MeasureCursors(
        *active, first_distance, second_distance);
    section_cursor_status_->setText(measurement.valid
        ? tr("A/B  ΔS %1 %4 · ΔH %2 %4 · 角度 %3°")
              .arg(measurement.distance_difference, 0, 'g', 6)
              .arg(measurement.height_difference, 0, 'g', 6)
              .arg(measurement.slope_angle_degrees, 0, 'f', 2).arg(unitLabel())
        : tr("游标 A/B：—"));
}

void PointCloudDialog::exportSectionCsv()
{
    const int row = section_list_ ? section_list_->currentRow() : -1;
    if (row < 0 || row >= static_cast<int>(section_profiles_.size()) ||
        !section_profiles_[static_cast<std::size_t>(row)].valid) return;
    QString path = QFileDialog::getSaveFileName(this, tr("导出剖线数据"),
        QStringLiteral("point-cloud-sections.csv"), tr("CSV 文件 (*.csv)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".csv");
    const bool all_visible = QMessageBox::question(this, tr("导出范围"),
        tr("是否导出所有可见剖线？选择“否”只导出当前剖线。"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setInlineStatus(section_status_, tr("CSV 导出失败：%1").arg(file.errorString()),
            QStringLiteral("error"));
        return;
    }
    QTextStream output(&file); output.setEncoding(QStringConverter::Utf8);
    output << "record_type,profile_id,profile_name,distance,raw_height,filtered_height,mad,"
              "source_count,confidence,interpolated,feature_type,start_distance,end_distance,"
              "signed_height,width,area,unit\n";
    for (const auto& profile : section_profiles_) {
        if (!profile.valid || (!all_visible && profile.definition.id != active_section_id_) ||
            (all_visible && !profile.definition.visible)) continue;
        const QString name = QString::fromStdWString(profile.definition.name);
        for (const auto& sample : profile.samples) {
            output << "sample," << profile.definition.id << ',' << name << ','
                   << sample.distance << ',' << sample.raw_height << ',' << sample.filtered_height
                   << ',' << sample.mad << ',' << sample.source_count << ',' << sample.confidence
                   << ',' << (sample.interpolated ? 1 : 0) << ",,,,,,," << unitLabel() << '\n';
        }
        for (const auto& feature : profile.features) {
            output << "feature," << profile.definition.id << ',' << name << ",,,,,,,,'"
                   << sectionFeatureLabel(feature.type) << "'," << feature.start_distance << ','
                   << feature.end_distance << ',' << feature.signed_height << ','
                   << feature.opening_width << ',' << feature.area << ',' << unitLabel() << '\n';
        }
    }
    setInlineStatus(section_status_, tr("剖线数据已导出：%1").arg(path), QStringLiteral("ok"));
}

void PointCloudDialog::exportSectionPng()
{
    if (!section_plot_ || active_section_id_ == 0) return;
    QString path = QFileDialog::getSaveFileName(this, tr("导出剖线图表"),
        QStringLiteral("point-cloud-section.png"), tr("PNG 图像 (*.png)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".png");
    const QSize target_size = section_plot_->size() * 2;
    QImage image(target_size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.scale(2.0, 2.0);
    section_plot_->render(&painter);
    painter.end();
    if (!image.save(path, "PNG")) {
        setInlineStatus(section_status_, tr("PNG 导出失败。"), QStringLiteral("error"));
        return;
    }
    setInlineStatus(section_status_, tr("剖线图表已导出：%1").arg(path), QStringLiteral("ok"));
}

QString PointCloudDialog::buildSectionReportHtml(bool complete_document) const
{
    QString rows;
    QString feature_rows;
    int included = 0;
    for (const auto& profile : section_profiles_) {
        if (!profile.valid || !profile.definition.include_in_report) continue;
        ++included;
        rows += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td>"
                               "<td>%5</td><td>%6</td></tr>")
            .arg(htmlEscape(QString::fromStdWString(profile.definition.name)))
            .arg(profile.width, 0, 'g', 8)
            .arg(profile.signed_step_height, 0, 'g', 8)
            .arg(profile.groove_depth, 0, 'g', 8)
            .arg(profile.total_peak_to_valley, 0, 'g', 8)
            .arg(profile.features.size());
        for (const auto& feature : profile.features) {
            feature_rows += QStringLiteral(
                "<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td>"
                "<td>%6</td><td>%7</td></tr>")
                .arg(htmlEscape(QString::fromStdWString(profile.definition.name)),
                    htmlEscape(sectionFeatureLabel(feature.type)))
                .arg(feature.start_distance, 0, 'g', 8)
                .arg(feature.end_distance, 0, 'g', 8)
                .arg(feature.depth_or_height, 0, 'g', 8)
                .arg(feature.opening_width, 0, 'g', 8)
                .arg(feature.area, 0, 'g', 8);
        }
    }
    if (included == 0) return {};
    QByteArray png;
    if (section_plot_ && section_plot_->width() > 0 && section_plot_->height() > 0) {
        QImage image(section_plot_->size() * 2, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image); painter.scale(2.0, 2.0); section_plot_->render(&painter); painter.end();
        QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); image.save(&buffer, "PNG");
    }
    const QString section = QStringLiteral(
        "<section class=\"point-cloud-sections\"><h2>点云剖线分析</h2>"
        "<p>数据源：%1 · 单位：%2 · 已纳入 %3 条剖线</p>"
        "<img class=\"section-plot\" src=\"data:image/png;base64,%4\" alt=\"点云剖线图表\">"
        "<table><thead><tr><th>剖线</th><th>长度</th><th>台阶差</th><th>沟槽深度</th>"
        "<th>峰谷值</th><th>特征数</th></tr></thead><tbody>%5</tbody></table>"
        "<h3>识别与人工修正特征</h3><table><thead><tr><th>剖线</th><th>类型</th>"
        "<th>起点</th><th>终点</th><th>高度/深度</th><th>宽度</th><th>面积</th>"
        "</tr></thead><tbody>%6</tbody></table></section>")
        .arg(htmlEscape(source_label_ ? source_label_->text() : QString()), htmlEscape(unitLabel()))
        .arg(included).arg(QString::fromLatin1(png.toBase64()), rows, feature_rows);
    if (!complete_document) return section;
    return QStringLiteral(
        "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
        "<title>CameraView 点云剖线报告</title><style>"
        "body{font-family:'Segoe UI','Microsoft YaHei',sans-serif;margin:32px;color:#182431;}"
        "h1,h2{color:#1769aa}.meta{color:#607080}table{border-collapse:collapse;width:100%;margin-top:16px}"
        "th,td{border:1px solid #ccd6df;padding:8px;text-align:left}th{background:#eef5fb}"
        ".section-plot{display:block;width:100%;max-width:1200px;background:#0a1119;margin:16px 0}"
        "@media print{body{margin:12mm}}</style></head><body>"
        "<h1>CameraView 点云剖线检测报告</h1><p class=\"meta\">当前会话分析结果</p>%1</body></html>")
        .arg(section);
}

void PointCloudDialog::exportSectionReport()
{
    const QString html = buildSectionReportHtml(true);
    if (html.isEmpty()) return;
    QString path = QFileDialog::getSaveFileName(this, tr("导出点云剖线报告"),
        QStringLiteral("CameraView-point-cloud-section-report.html"), tr("HTML 报告 (*.html)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".html");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setInlineStatus(section_status_, tr("报告导出失败：%1").arg(file.errorString()),
            QStringLiteral("error"));
        return;
    }
    QTextStream stream(&file); stream.setEncoding(QStringConverter::Utf8); stream << html;
    setInlineStatus(section_status_, tr("点云剖线报告已导出：%1").arg(path), QStringLiteral("ok"));
}

void PointCloudDialog::fitPlane()
{
    fitted_plane_ = PointCloudProcessor::FitPlane(current_cloud_);
    reference_plane_model_id_ = 0;
    if (!fitted_plane_.valid) {
        cloud_widget_->setFittedPlane({});
        show_plane_check_->setEnabled(false);
        plane_label_->setText(tr("至少需要三个非退化点才能拟合平面。"));
        level_button_->setEnabled(false);
        return;
    }
    plane_label_->setText(tr("%1x + %2y + %3z + %4 = 0\nRMS：%5 %6")
        .arg(fitted_plane_.nx, 0, 'g', 7)
        .arg(fitted_plane_.ny, 0, 'g', 7)
        .arg(fitted_plane_.nz, 0, 'g', 7)
        .arg(fitted_plane_.d, 0, 'g', 7)
        .arg(fitted_plane_.rms, 0, 'g', 7)
        .arg(unitLabel()));
    show_plane_check_->setEnabled(true);
    show_plane_check_->setChecked(true);
    cloud_widget_->setFittedPlaneVisible(true);
    cloud_widget_->setFittedPlane(fitted_plane_);
    level_button_->setEnabled(true);
}

void PointCloudDialog::levelCloud()
{
    if (!fitted_plane_.valid) return;
    pushProcessedCloud(PointCloudProcessor::LevelToPlane(current_cloud_, fitted_plane_),
        tr("平面校平"));
}

void PointCloudDialog::undoProcessing()
{
    if (undo_stack_.empty()) return;
    cancelActiveFit(tr("点云已撤销，原拟合任务已取消。"));
    setMeasureMode(PointCloudMeasureMode::Navigate);
    clearInteractiveCrop();
    redo_stack_.push_back(current_cloud_);
    current_cloud_ = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    ++cloud_revision_;
    clearSections();
    clearFittedPlane();
    clearGeometricModels();
    measurements_.clear();
    pending_points_.clear();
    resetInspectionResults();
    updateCloudPresentation(tr("已撤销"));
    updateProcessingDefaults();
    refreshMeasurementList();
}

void PointCloudDialog::redoProcessing()
{
    if (redo_stack_.empty()) return;
    cancelActiveFit(tr("点云已重做，原拟合任务已取消。"));
    setMeasureMode(PointCloudMeasureMode::Navigate);
    clearInteractiveCrop();
    undo_stack_.push_back(current_cloud_);
    current_cloud_ = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    ++cloud_revision_;
    clearSections();
    clearFittedPlane();
    clearGeometricModels();
    measurements_.clear();
    pending_points_.clear();
    resetInspectionResults();
    updateCloudPresentation(tr("已重做"));
    updateProcessingDefaults();
    refreshMeasurementList();
}

void PointCloudDialog::restoreOriginal()
{
    if (original_cloud_.Empty()) return;
    cancelActiveFit(tr("已恢复原始点云，原拟合任务已取消。"));
    setMeasureMode(PointCloudMeasureMode::Navigate);
    clearInteractiveCrop();
    undo_stack_.clear();
    redo_stack_.clear();
    current_cloud_ = original_cloud_;
    ++cloud_revision_;
    clearSections();
    clearFittedPlane();
    clearGeometricModels();
    measurements_.clear();
    pending_points_.clear();
    resetInspectionResults();
    updateCloudPresentation(tr("原始数据"));
    updateProcessingDefaults();
    refreshMeasurementList();
}

void PointCloudDialog::setMeasureMode(PointCloudMeasureMode mode)
{
    cloud_widget_->setBoxSelectionEnabled(false);
    cloud_widget_->setFreeSelectionEnabled(false);
    cloud_widget_->setSectionSelectionEnabled(false);
    if (free_selection_button_) {
        const QSignalBlocker navigation_blocker(navigation_button_);
        const QSignalBlocker selection_blocker(free_selection_button_);
        navigation_button_->setChecked(true);
        free_selection_button_->setChecked(false);
    }
    measure_mode_ = mode;
    interaction_mode_ = mode == PointCloudMeasureMode::Navigate
        ? PointCloudInteractionMode::Browse : PointCloudInteractionMode::Measure;
    if (browse_action_) browse_action_->setChecked(mode == PointCloudMeasureMode::Navigate);
    if (select_action_) select_action_->setChecked(false);
    if (measurement_tool_group_) {
        if (auto* button = measurement_tool_group_->button(static_cast<int>(mode))) {
            button->setChecked(true);
        }
    }
    pending_points_.clear();
    const int selected_measurement = measurement_list_ ? measurement_list_->currentRow() : -1;
    if (selected_measurement >= 0 &&
        selected_measurement < static_cast<int>(measurements_.size())) {
        cloud_widget_->setHighlightedIndices(
            measurements_[static_cast<std::size_t>(selected_measurement)].point_indices);
    } else {
        cloud_widget_->setHighlightedIndices({});
    }
    cloud_widget_->setPickingEnabled(mode != PointCloudMeasureMode::Navigate);
    QString hint;
    QString toolbar_measure_text = tr("测量");
    switch (mode) {
    case PointCloudMeasureMode::Point:
        hint = tr("点击一个点读取 XYZ 坐标。"); toolbar_measure_text = tr("测量 · 点"); break;
    case PointCloudMeasureMode::Distance:
        hint = tr("依次点击两个点测量三维距离。"); toolbar_measure_text = tr("测量 · 距离"); break;
    case PointCloudMeasureMode::HeightDifference:
        hint = tr("依次点击两个点测量有符号 Z 高度差。"); toolbar_measure_text = tr("测量 · 高度"); break;
    case PointCloudMeasureMode::Angle:
        hint = tr("依次点击端点、顶点、端点测量三维角度。"); toolbar_measure_text = tr("测量 · 角度"); break;
    case PointCloudMeasureMode::PointToPlane:
        hint = fitted_plane_.valid ? tr("点击一点测量到参考平面的垂直距离。")
                                   : tr("请先在“几何测量”中选择平面模型，或在“清理与处理”中拟合参考平面。");
        toolbar_measure_text = tr("测量 · 点到面");
        break;
    case PointCloudMeasureMode::PlaneAngle:
        hint = tr("依次点选第一平面 3 点和第二平面 3 点。"); toolbar_measure_text = tr("测量 · 面角");
        break;
    case PointCloudMeasureMode::LineIntersection:
        hint = tr("依次点选第一直线 2 点和第二直线 2 点。"); toolbar_measure_text = tr("测量 · 交点");
        break;
    case PointCloudMeasureMode::Navigate:
    default: hint = tr("浏览模式：左键旋转、右键平移、滚轮缩放。"); break;
    }
    measurement_hint_->setText(hint);
    if (toolbar_measure_button_) {
        toolbar_measure_button_->setText(toolbar_measure_text);
        toolbar_measure_button_->setProperty("activeTool", mode != PointCloudMeasureMode::Navigate);
        toolbar_measure_button_->style()->unpolish(toolbar_measure_button_);
        toolbar_measure_button_->style()->polish(toolbar_measure_button_);
    }
    updateToolbarPresentation();
}

void PointCloudDialog::acceptPickedPoint(int index)
{
    if (index < 0 || index >= static_cast<int>(current_cloud_.points.size()) ||
        measure_mode_ == PointCloudMeasureMode::Navigate) return;
    if (measure_mode_ == PointCloudMeasureMode::PointToPlane && !fitted_plane_.valid) {
        measurement_hint_->setText(
            tr("请先在“几何测量”中选择平面模型，或在“清理与处理”中拟合参考平面。"));
        return;
    }
    pending_points_.push_back(index);
    cloud_widget_->setHighlightedIndices(pending_points_);
    const int required = measure_mode_ == PointCloudMeasureMode::PlaneAngle ? 6
        : measure_mode_ == PointCloudMeasureMode::LineIntersection ? 4
        : measure_mode_ == PointCloudMeasureMode::Angle ? 3
        : measure_mode_ == PointCloudMeasureMode::Distance ||
                measure_mode_ == PointCloudMeasureMode::HeightDifference ? 2 : 1;
    measurement_hint_->setText(tr("已选择 %1/%2 个点").arg(pending_points_.size()).arg(required));
    if (pending_points_.size() >= required) finishMeasurement();
}

void PointCloudDialog::finishMeasurement()
{
    if (pending_points_.isEmpty()) return;
    const auto& first = current_cloud_.points[static_cast<std::size_t>(pending_points_[0])];
    PointCloudMeasurementRecord record;
    record.point_indices = pending_points_;
    switch (measure_mode_) {
    case PointCloudMeasureMode::Point:
        record.type = tr("点坐标");
        record.value = tr("X %1, Y %2, Z %3 %4")
            .arg(first.x, 0, 'g', 9).arg(first.y, 0, 'g', 9)
            .arg(first.z, 0, 'g', 9).arg(unitLabel());
        break;
    case PointCloudMeasureMode::Distance: {
        const auto& second = current_cloud_.points[static_cast<std::size_t>(pending_points_[1])];
        const auto delta = PointCloudMeasurement::Delta(first, second);
        record.type = tr("三维距离");
        record.value = tr("%1 %2（ΔX %3, ΔY %4, ΔZ %5）")
            .arg(PointCloudMeasurement::Distance(first, second), 0, 'g', 9)
            .arg(unitLabel()).arg(delta.x, 0, 'g', 7)
            .arg(delta.y, 0, 'g', 7).arg(delta.z, 0, 'g', 7);
        break;
    }
    case PointCloudMeasureMode::HeightDifference: {
        const auto& second = current_cloud_.points[static_cast<std::size_t>(pending_points_[1])];
        record.type = tr("高度差");
        record.value = tr("%1 %2").arg(
            PointCloudMeasurement::HeightDifference(first, second), 0, 'g', 9).arg(unitLabel());
        break;
    }
    case PointCloudMeasureMode::Angle: {
        const auto& vertex = current_cloud_.points[static_cast<std::size_t>(pending_points_[1])];
        const auto& third = current_cloud_.points[static_cast<std::size_t>(pending_points_[2])];
        record.type = tr("三点角度");
        record.value = tr("%1°").arg(
            PointCloudMeasurement::AngleDegrees(first, vertex, third), 0, 'f', 4);
        break;
    }
    case PointCloudMeasureMode::PointToPlane:
        record.type = tr("点到平面");
        record.value = tr("%1 %2").arg(
            PointCloudMeasurement::PointToPlaneDistance(first, fitted_plane_), 0, 'g', 9)
            .arg(unitLabel());
        break;
    case PointCloudMeasureMode::PlaneAngle: {
        const auto& p1 = current_cloud_.points[static_cast<std::size_t>(pending_points_[0])];
        const auto& p2 = current_cloud_.points[static_cast<std::size_t>(pending_points_[1])];
        const auto& p3 = current_cloud_.points[static_cast<std::size_t>(pending_points_[2])];
        const auto& p4 = current_cloud_.points[static_cast<std::size_t>(pending_points_[3])];
        const auto& p5 = current_cloud_.points[static_cast<std::size_t>(pending_points_[4])];
        const auto& p6 = current_cloud_.points[static_cast<std::size_t>(pending_points_[5])];
        const PointCloudPlane first_plane = PointCloudMetrology::PlaneFromPoints(p1, p2, p3);
        const PointCloudPlane second_plane = PointCloudMetrology::PlaneFromPoints(p4, p5, p6);
        const double angle = PointCloudMetrology::PlaneAngleDegrees(first_plane, second_plane);
        if (!first_plane.valid || !second_plane.valid || !std::isfinite(angle)) {
            measurement_hint_->setText(tr("平面取点退化，请重新选择。"));
            pending_points_.clear();
            cloud_widget_->setHighlightedIndices({});
            return;
        }
        record.type = tr("两平面夹角");
        record.value = tr("%1°").arg(angle, 0, 'f', 5);
        break;
    }
    case PointCloudMeasureMode::LineIntersection: {
        const auto& p1 = current_cloud_.points[static_cast<std::size_t>(pending_points_[0])];
        const auto& p2 = current_cloud_.points[static_cast<std::size_t>(pending_points_[1])];
        const auto& p3 = current_cloud_.points[static_cast<std::size_t>(pending_points_[2])];
        const auto& p4 = current_cloud_.points[static_cast<std::size_t>(pending_points_[3])];
        const PointCloudLineIntersection intersection = PointCloudMetrology::IntersectLines(
            PointCloudMetrology::LineFromPoints(p1, p2),
            PointCloudMetrology::LineFromPoints(p3, p4), 1e-6);
        if (!intersection.valid) {
            measurement_hint_->setText(tr("两条直线平行或退化，无唯一交点。"));
            pending_points_.clear();
            cloud_widget_->setHighlightedIndices({});
            return;
        }
        record.type = tr("空间直线交点");
        record.value = intersection.intersects
            ? tr("X %1, Y %2, Z %3 %4")
                  .arg(intersection.point.x, 0, 'g', 9)
                  .arg(intersection.point.y, 0, 'g', 9)
                  .arg(intersection.point.z, 0, 'g', 9).arg(unitLabel())
            : tr("异面直线：最近距离 %1 %2")
                  .arg(intersection.separation, 0, 'g', 9).arg(unitLabel());
        break;
    }
    case PointCloudMeasureMode::Navigate: return;
    }
    const PointCloudMeasureMode completed_mode = measure_mode_;
    const QString completed_type = record.type;
    measurements_.push_back(std::move(record));
    pending_points_.clear();
    setMeasureMode(completed_mode);
    refreshMeasurementList();
    measurement_list_->setCurrentRow(static_cast<int>(measurements_.size()) - 1);
    measurement_hint_->setText(tr("已添加“%1”；工具保持启用，可继续点击测量。")
        .arg(completed_type));
}

void PointCloudDialog::refreshMeasurementList()
{
    const int old_row = measurement_list_->currentRow();
    int selected_row = -1;
    {
        const QSignalBlocker blocker(measurement_list_);
        measurement_list_->clear();
        for (std::size_t index = 0; index < measurements_.size(); ++index) {
            measurement_list_->addItem(tr("%1. %2：%3")
                .arg(index + 1).arg(measurements_[index].type, measurements_[index].value));
        }
        if (old_row >= 0 && !measurements_.empty()) {
            selected_row = std::min(old_row, static_cast<int>(measurements_.size()) - 1);
            measurement_list_->setCurrentRow(selected_row);
        }
    }
    selectMeasurementRow(selected_row);
    updateActionStates();
}

void PointCloudDialog::selectMeasurementRow(int row)
{
    if (!pending_points_.isEmpty()) {
        updateActionStates();
        return;
    }
    if (row < 0 || row >= static_cast<int>(measurements_.size())) {
        cloud_widget_->setHighlightedIndices({});
        updateActionStates();
        return;
    }
    const PointCloudMeasurementRecord& record = measurements_[static_cast<std::size_t>(row)];
    cloud_widget_->setHighlightedIndices(record.point_indices);
    measurement_hint_->setText(record.point_indices.isEmpty()
        ? tr("已选择“%1”结果。").arg(record.type)
        : tr("已在点云中标出“%1”的 %2 个取样点；当前工具仍保持启用。")
              .arg(record.type).arg(record.point_indices.size()));
    updateActionStates();
}

void PointCloudDialog::exportMeasurements()
{
    if (measurements_.empty()) {
        QMessageBox::information(this, tr("导出测量"), tr("当前没有点云测量结果。"));
        return;
    }
    QString path = QFileDialog::getSaveFileName(
        this, tr("导出点云测量"), QStringLiteral("CameraView-point-cloud-measurements.csv"),
        tr("CSV 文件 (*.csv)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".csv");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("导出失败"), file.errorString());
        return;
    }
    QTextStream output(&file);
    output.setEncoding(QStringConverter::Utf8);
    output << "index,type,value,point_indices\n";
    for (std::size_t index = 0; index < measurements_.size(); ++index) {
        QStringList points;
        for (int point : measurements_[index].point_indices) points << QString::number(point);
        QString value = measurements_[index].value;
        value.replace('"', QStringLiteral("\"\""));
        output << index + 1 << ",\"" << measurements_[index].type << "\",\""
               << value << "\",\"" << points.join(';') << "\"\n";
    }
}

QString PointCloudDialog::unitLabel() const
{
    return QString::fromStdWString(PointCloudUnitLabel(
        static_cast<PointCloudUnit>(unit_combo_->currentData().toInt())));
}

void PointCloudDialog::loadSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("PointCloudWorkspace"));
    const bool automated_test = QCoreApplication::applicationName().contains(
        QStringLiteral("Test"), Qt::CaseInsensitive);
    const int inspector_width = std::clamp(
        settings.value(QStringLiteral("wideInspectorWidth"), 380).toInt(), 336, 480);
    const int section_height = std::clamp(
        settings.value(QStringLiteral("sectionHeight"), 0).toInt(), 0, 420);
    wide_workspace_sizes_ = {std::max(700, width() - inspector_width - 32), inspector_width};
    wide_section_sizes_ = {std::max(360, height() - section_height - 120), section_height};
    if (!automated_test) {
        PointCloudWorkspacePage page = PointCloudWorkspacePage::DataDisplay;
        const QString page_id = settings.value(QStringLiteral("workspacePageId")).toString();
        if (page_id == QStringLiteral("processing")) {
            page = PointCloudWorkspacePage::Processing;
        } else if (page_id == QStringLiteral("geometry")) {
            page = PointCloudWorkspacePage::GeometryMeasurement;
        } else if (page_id == QStringLiteral("analysis")) {
            page = PointCloudWorkspacePage::AnalysisReport;
        } else if (page_id.isEmpty()) {
            const int legacy_tab = settings.value(QStringLiteral("tab"), 0).toInt();
            page = legacy_tab <= 0 ? PointCloudWorkspacePage::DataDisplay
                : legacy_tab == 1 ? PointCloudWorkspacePage::Processing
                : legacy_tab <= 3 ? PointCloudWorkspacePage::GeometryMeasurement
                                  : PointCloudWorkspacePage::AnalysisReport;
        }
        setWorkspacePage(page, false);
    }
    if (point_size_spin_) point_size_spin_->setValue(settings.value(QStringLiteral("pointSize"), 2.5).toDouble());
    if (color_combo_) {
        int color_index = std::clamp(settings.value(QStringLiteral("colorMode"), 0).toInt(),
            0, color_combo_->count() - 1);
        const int texture_index = color_combo_->findData(
            static_cast<int>(PointCloudColorMode::Texture));
        if (current_cloud_.HasTextureSurface()) {
            color_index = texture_index;
        } else if (color_index == texture_index) {
            color_index = color_combo_->findData(static_cast<int>(PointCloudColorMode::Height));
        }
        color_combo_->setCurrentIndex(color_index);
    }
    if (axes_check_) axes_check_->setChecked(settings.value(QStringLiteral("axes"), true).toBool());
    if (texture_enhance_check_) texture_enhance_check_->setChecked(
        settings.value(QStringLiteral("textureEnhancement"), true).toBool());
    if (view_preset_combo_) {
        view_preset_combo_->setCurrentIndex(std::clamp(
            settings.value(QStringLiteral("viewPreset"), 0).toInt(),
            0, view_preset_combo_->count() - 1));
    }
    if (cylinder_axis_combo_) cylinder_axis_combo_->setCurrentIndex(settings.value(QStringLiteral("cylinderAxis"), 0).toInt());
    if (fit_threshold_spin_) fit_threshold_spin_->setValue(settings.value(QStringLiteral("fitThreshold"), 0.0).toDouble());
    if (section_width_spin_) section_width_spin_->setValue(settings.value(QStringLiteral("sectionBandWidth"), 0.0).toDouble());
    if (section_spacing_spin_) section_spacing_spin_->setValue(settings.value(QStringLiteral("sectionSpacing"), 0.0).toDouble());
    if (section_reference_combo_) section_reference_combo_->setCurrentIndex(
        settings.value(QStringLiteral("sectionReference"), 0).toInt());
    if (section_median_window_spin_) section_median_window_spin_->setValue(
        settings.value(QStringLiteral("sectionMedianWindow"), 5).toInt());
    if (section_smoothing_window_spin_) section_smoothing_window_spin_->setValue(
        settings.value(QStringLiteral("sectionSmoothingWindow"), 11).toInt());
    if (section_gap_spin_) section_gap_spin_->setValue(settings.value(QStringLiteral("sectionGap"), 3).toInt());
    if (section_sensitivity_spin_) section_sensitivity_spin_->setValue(
        settings.value(QStringLiteral("sectionSensitivity"), 3.5).toDouble());
    if (section_plateau_spin_) section_plateau_spin_->setValue(
        settings.value(QStringLiteral("sectionPlateau"), 5).toInt());
    settings.endGroup();
}

void PointCloudDialog::saveSettings() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("PointCloudWorkspace"));
    const QList<int> workspace_sizes = compact_layout_ ? wide_workspace_sizes_
        : workspace_splitter_ ? workspace_splitter_->sizes() : QList<int>{};
    if (workspace_sizes.size() >= 2) {
        settings.setValue(QStringLiteral("wideInspectorWidth"), workspace_sizes[1]);
    }
    const QList<int> section_sizes = compact_layout_ ? wide_section_sizes_
        : view_section_splitter_ ? view_section_splitter_->sizes() : QList<int>{};
    if (section_sizes.size() >= 2) {
        settings.setValue(QStringLiteral("sectionHeight"), section_sizes[1]);
    }
    const QStringList page_ids{
        QStringLiteral("data"), QStringLiteral("processing"),
        QStringLiteral("geometry"), QStringLiteral("analysis")};
    settings.setValue(QStringLiteral("workspacePageId"),
        page_ids.value(static_cast<int>(workspace_page_), QStringLiteral("data")));
    settings.remove(QStringLiteral("tab"));
    settings.remove(QStringLiteral("splitterState"));
    settings.remove(QStringLiteral("sectionSplitterState"));
    if (point_size_spin_) settings.setValue(QStringLiteral("pointSize"), point_size_spin_->value());
    if (color_combo_) settings.setValue(QStringLiteral("colorMode"), color_combo_->currentIndex());
    if (axes_check_) settings.setValue(QStringLiteral("axes"), axes_check_->isChecked());
    if (texture_enhance_check_) settings.setValue(
        QStringLiteral("textureEnhancement"), texture_enhance_check_->isChecked());
    if (view_preset_combo_) settings.setValue(QStringLiteral("viewPreset"), view_preset_combo_->currentIndex());
    if (cylinder_axis_combo_) settings.setValue(QStringLiteral("cylinderAxis"), cylinder_axis_combo_->currentIndex());
    if (fit_threshold_spin_) settings.setValue(QStringLiteral("fitThreshold"), fit_threshold_spin_->value());
    if (section_width_spin_) settings.setValue(QStringLiteral("sectionBandWidth"), section_width_spin_->value());
    if (section_spacing_spin_) settings.setValue(QStringLiteral("sectionSpacing"), section_spacing_spin_->value());
    if (section_reference_combo_) settings.setValue(QStringLiteral("sectionReference"), section_reference_combo_->currentIndex());
    if (section_median_window_spin_) settings.setValue(QStringLiteral("sectionMedianWindow"), section_median_window_spin_->value());
    if (section_smoothing_window_spin_) settings.setValue(QStringLiteral("sectionSmoothingWindow"), section_smoothing_window_spin_->value());
    if (section_gap_spin_) settings.setValue(QStringLiteral("sectionGap"), section_gap_spin_->value());
    if (section_sensitivity_spin_) settings.setValue(QStringLiteral("sectionSensitivity"), section_sensitivity_spin_->value());
    if (section_plateau_spin_) settings.setValue(QStringLiteral("sectionPlateau"), section_plateau_spin_->value());
    settings.endGroup();
}

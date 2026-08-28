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

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QProgressDialog>
#include <QSettings>
#include <QSignalBlocker>
#include <QSet>
#include <QScrollArea>
#include <QSplitter>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextStream>
#include <QStringConverter>
#include <QTimer>
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
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);
    auto* toolbar = new QWidget;
    toolbar->setObjectName(QStringLiteral("PointCloudWorkspaceToolbar"));
    auto* toolbar_layout = new QHBoxLayout(toolbar);
    toolbar_layout->setContentsMargins(8, 6, 8, 6);
    navigation_button_ = new QPushButton(tr("浏览"));
    navigation_button_->setObjectName(QStringLiteral("PointCloudWorkspaceNavigateButton"));
    navigation_button_->setCheckable(true);
    navigation_button_->setChecked(true);
    free_selection_button_ = new QPushButton(tr("自由选择"));
    free_selection_button_->setObjectName(QStringLiteral("PointCloudFreeSelectionButton"));
    free_selection_button_->setCheckable(true);
    free_selection_button_->setProperty("requiresCloud", true);
    free_selection_button_->setToolTip(
        tr("按住左键绘制任意轮廓；Shift 添加，Ctrl 移除"));
    auto* workspace_mode_group = new QButtonGroup(this);
    workspace_mode_group->setExclusive(true);
    workspace_mode_group->addButton(navigation_button_);
    workspace_mode_group->addButton(free_selection_button_);
    auto* clear_selection_button = new QPushButton(tr("清除选择"));
    undo_button_ = new QPushButton(tr("撤销"));
    undo_button_->setObjectName(QStringLiteral("PointCloudUndoButton"));
    redo_button_ = new QPushButton(tr("重做"));
    redo_button_->setObjectName(QStringLiteral("PointCloudRedoButton"));
    selection_status_ = new QLabel(tr("选择 0"));
    workspace_status_ = new QLabel(tr("未载入点云"));
    toolbar_layout->addWidget(navigation_button_);
    toolbar_layout->addWidget(free_selection_button_);
    toolbar_layout->addWidget(clear_selection_button);
    toolbar_layout->addWidget(undo_button_);
    toolbar_layout->addWidget(redo_button_);
    toolbar_layout->addStretch();
    toolbar_layout->addWidget(selection_status_);
    toolbar_layout->addWidget(workspace_status_);
    root->addWidget(toolbar);
    workspace_splitter_ = new QSplitter(Qt::Horizontal);
    workspace_splitter_->setObjectName(QStringLiteral("PointCloudWorkspaceSplitter"));
    workspace_splitter_->setChildrenCollapsible(false);
    cloud_widget_ = new PointCloudWidget;
    workspace_splitter_->addWidget(cloud_widget_);

    auto* side = new QWidget;
    side->setMinimumWidth(320);
    auto* side_layout = new QVBoxLayout(side);
    side_layout->setContentsMargins(0, 0, 0, 0);
    source_label_ = new QLabel(tr("尚未载入点云"));
    source_label_->setObjectName(QStringLiteral("PointCloudSourceLabel"));
    source_label_->setWordWrap(true);
    source_label_->setProperty("role", QStringLiteral("summary"));
    side_layout->addWidget(source_label_);
    tabs_ = new QTabWidget;
    tabs_->setObjectName(QStringLiteral("PointCloudToolTabs"));
    side_layout->addWidget(tabs_, 1);

    auto* data_page = new QWidget;
    auto* data_layout = new QVBoxLayout(data_page);
    open_button_ = new QPushButton(tr("打开点云…"));
    open_button_->setObjectName(QStringLiteral("PointCloudOpenButton"));
    open_button_->setProperty("role", QStringLiteral("primary"));
    auto* export_button = new QPushButton(tr("导出处理结果…"));
    export_button->setObjectName(QStringLiteral("PointCloudExportButton"));
    data_layout->addWidget(rowOf({open_button_, export_button}));
    statistics_label_ = new QLabel(tr("点数：0"));
    statistics_label_->setObjectName(QStringLiteral("PointCloudStatistics"));
    statistics_label_->setWordWrap(true);
    data_layout->addWidget(statistics_label_);
    auto* display_group = new QGroupBox(tr("显示"));
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
    display_form->addRow(tr("着色"), color_combo_);
    display_form->addRow(tr("点大小"), point_size_spin_);
    display_form->addRow({}, axes_check_);
    display_form->addRow(tr("渲染后端"), backend_label_);
    data_layout->addWidget(display_group);
    auto* reset_view = new QPushButton(tr("复位视角"));
    reset_view->setObjectName(QStringLiteral("PointCloudResetViewButton"));
    data_layout->addWidget(reset_view);
    data_layout->addStretch();
    tabs_->addTab(data_page, tr("数据"));

    auto* process_page = new QScrollArea;
    process_page->setObjectName(QStringLiteral("PointCloudProcessingScrollArea"));
    process_page->setWidgetResizable(true);
    process_page->setFrameShape(QFrame::NoFrame);
    process_page->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* process_content = new QWidget;
    auto* process_layout = new QVBoxLayout(process_content);
    auto* interactive_crop_group = new QGroupBox(tr("交互式框选裁剪"));
    auto* interactive_crop_layout = new QVBoxLayout(interactive_crop_group);
    crop_selection_label_ = new QLabel(tr("尚未选择点"));
    crop_selection_label_->setObjectName(QStringLiteral("PointCloudCropSelectionLabel"));
    crop_selection_label_->setWordWrap(true);
    begin_crop_button_ = new QPushButton(tr("在视图中拖框选择"));
    begin_crop_button_->setObjectName(QStringLiteral("PointCloudBeginInteractiveCropButton"));
    begin_crop_button_->setCheckable(true);
    keep_crop_button_ = new QPushButton(tr("保留框内"));
    keep_crop_button_->setObjectName(QStringLiteral("PointCloudKeepSelectionButton"));
    remove_crop_button_ = new QPushButton(tr("移除框内"));
    remove_crop_button_->setObjectName(QStringLiteral("PointCloudRemoveSelectionButton"));
    keep_crop_button_->setEnabled(false);
    remove_crop_button_->setEnabled(false);
    interactive_crop_layout->addWidget(crop_selection_label_);
    interactive_crop_layout->addWidget(begin_crop_button_);
    interactive_crop_layout->addWidget(rowOf({keep_crop_button_, remove_crop_button_}));
    process_layout->addWidget(interactive_crop_group);
    auto* smart_filter_group = new QGroupBox(tr("智能滤波与去噪"));
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
    tabs_->addTab(process_page, tr("处理"));

    auto* fit_page = new QScrollArea;
    fit_page->setObjectName(QStringLiteral("PointCloudFitScrollArea"));
    fit_page->setWidgetResizable(true);
    fit_page->setFrameShape(QFrame::NoFrame);
    fit_page->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* fit_content = new QWidget;
    auto* fit_layout = new QVBoxLayout(fit_content);
    auto* fit_group = new QGroupBox(tr("几何拟合"));
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
    auto* fit_plane_model = new QPushButton(tr("拟合平面"));
    auto* fit_sphere = new QPushButton(tr("拟合球"));
    auto* fit_cylinder = new QPushButton(tr("拟合圆柱"));
    fit_plane_model->setObjectName(QStringLiteral("PointCloudFitPlaneModelButton"));
    fit_sphere->setObjectName(QStringLiteral("PointCloudFitSphereButton"));
    fit_cylinder->setObjectName(QStringLiteral("PointCloudFitCylinderButton"));
    fit_sphere->setProperty("role", QStringLiteral("primary"));
    fit_cylinder->setProperty("role", QStringLiteral("primary"));
    fit_status_ = new QLabel(tr("选择拟合类型，结果将加入模型列表。"));
    fit_status_->setObjectName(QStringLiteral("PointCloudFitStatus"));
    fit_status_->setWordWrap(true);
    fit_form->addRow(tr("作用范围"), fit_scope_combo_);
    fit_form->addRow(tr("内点阈值"), fit_threshold_spin_);
    fit_form->addRow(tr("圆柱轴向"), cylinder_axis_combo_);
    fit_form->addRow(tr("最小半径"), minimum_radius_spin_);
    fit_form->addRow(tr("最大半径"), maximum_radius_spin_);
    fit_form->addRow({}, rowOf({fit_plane_model, fit_sphere, fit_cylinder}));
    fit_form->addRow({}, fit_status_);
    fit_layout->addWidget(fit_group);
    auto* model_group = new QGroupBox(tr("模型管理"));
    auto* model_layout = new QVBoxLayout(model_group);
    model_list_ = new QListWidget;
    model_list_->setObjectName(QStringLiteral("PointCloudModelList"));
    model_details_ = new QLabel(tr("尚未创建模型"));
    model_details_->setWordWrap(true);
    residual_coloring_check_ = new QCheckBox(tr("按当前模型显示残差颜色"));
    auto* show_model = new QPushButton(tr("显示"));
    auto* hide_model = new QPushButton(tr("隐藏"));
    auto* delete_model = new QPushButton(tr("删除"));
    auto* clear_models_button = new QPushButton(tr("清空模型"));
    model_layout->addWidget(model_list_);
    model_layout->addWidget(model_details_);
    model_layout->addWidget(residual_coloring_check_);
    model_layout->addWidget(rowOf({show_model, hide_model, delete_model, clear_models_button}));
    fit_layout->addWidget(model_group);
    fit_layout->addStretch();
    fit_page->setWidget(fit_content);
    tabs_->addTab(fit_page, tr("拟合"));

    auto* measure_page = new QWidget;
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
        button->setMinimumHeight(42);
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
    measure_layout->addWidget(measurement_list_, 1);
    auto* delete_measurement = new QPushButton(tr("删除选中"));
    delete_measurement->setObjectName(QStringLiteral("PointCloudDeleteMeasurementButton"));
    auto* clear_measurements = new QPushButton(tr("清空测量"));
    clear_measurements->setObjectName(QStringLiteral("PointCloudClearMeasurementsButton"));
    auto* export_measurements = new QPushButton(tr("导出 CSV…"));
    export_measurements->setObjectName(QStringLiteral("PointCloudExportMeasurementsButton"));
    measure_layout->addWidget(rowOf({
        delete_measurement, clear_measurements, export_measurements}));
    tabs_->addTab(measure_page, tr("测量"));

    auto* inspect_page = new QScrollArea;
    inspect_page->setObjectName(QStringLiteral("PointCloudInspectionScrollArea"));
    inspect_page->setWidgetResizable(true);
    inspect_page->setFrameShape(QFrame::NoFrame);
    inspect_page->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* inspect_content = new QWidget;
    auto* inspect_layout = new QVBoxLayout(inspect_content);
    auto* tolerance_group = new QGroupBox(tr("形位公差一键评级"));
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
    auto* deviation_layout = new QVBoxLayout(deviation_group);
    auto* deviation_help = new QLabel(tr(
        "统计全部点到最佳拟合平面的有符号垂直偏差，显示直方图、理论高斯曲线和 σ 覆盖率。"));
    deviation_help->setWordWrap(true);
    auto* deviation_button = new QPushButton(tr("分析偏差高斯分布…"));
    deviation_button->setObjectName(QStringLiteral("PointCloudDeviationDistributionButton"));
    deviation_button->setProperty("role", QStringLiteral("primary"));
    deviation_layout->addWidget(deviation_help);
    deviation_layout->addWidget(deviation_button);
    inspect_layout->addWidget(deviation_group);
    auto* section_group = new QGroupBox(tr("任意截面分析"));
    auto* section_form = new QFormLayout(section_group);
    section_width_spin_ = new QDoubleSpinBox;
    section_width_spin_->setObjectName(QStringLiteral("PointCloudSectionBandWidth"));
    section_width_spin_->setRange(2.0, 80.0);
    section_width_spin_->setValue(10.0);
    section_width_spin_->setSuffix(tr(" px"));
    auto* begin_section = new QPushButton(tr("在点云上拉出截面线"));
    begin_section->setObjectName(QStringLiteral("PointCloudBeginSectionButton"));
    begin_section->setProperty("role", QStringLiteral("primary"));
    section_status_ = new QLabel(tr("自动生成 Z 高度剖面，计算阶梯差、截面宽度和凹槽深度。"));
    section_status_->setObjectName(QStringLiteral("PointCloudSectionStatus"));
    section_status_->setWordWrap(true);
    section_form->addRow(tr("屏幕采样半宽"), section_width_spin_);
    section_form->addRow({}, begin_section);
    section_form->addRow({}, section_status_);
    inspect_layout->addWidget(section_group);
    inspect_layout->addStretch();
    inspect_page->setWidget(inspect_content);
    tabs_->addTab(inspect_page, tr("分析"));

    auto* close_button = new QPushButton(tr("关闭"));
    side_layout->addWidget(close_button);
    workspace_splitter_->addWidget(side);
    workspace_splitter_->setStretchFactor(0, 1);
    workspace_splitter_->setStretchFactor(1, 0);
    workspace_splitter_->setSizes({900, 360});
    root->addWidget(workspace_splitter_, 1);

    connect(open_button_, &QPushButton::clicked, this, &PointCloudDialog::openCloud);
    connect(navigation_button_, &QPushButton::clicked, this, [this] {
        setMeasureMode(PointCloudMeasureMode::Navigate);
        cloud_widget_->setBoxSelectionEnabled(false);
        cloud_widget_->setFreeSelectionEnabled(false);
    });
    connect(free_selection_button_, &QPushButton::clicked, this, [this] {
        setMeasureMode(PointCloudMeasureMode::Navigate);
        {
            const QSignalBlocker navigation_blocker(navigation_button_);
            const QSignalBlocker selection_blocker(free_selection_button_);
            navigation_button_->setChecked(false);
            free_selection_button_->setChecked(true);
        }
        cloud_widget_->setFreeSelectionEnabled(true);
        workspace_status_->setText(
            tr("按住左键绘制自由选区；选择会保留供拟合、裁剪和分析复用"));
    });
    connect(clear_selection_button, &QPushButton::clicked, this, [this] {
        clearInteractiveCrop();
        updateSelectionPresentation();
    });
    connect(export_button, &QPushButton::clicked, this, &PointCloudDialog::exportCloud);
    connect(reset_view, &QPushButton::clicked, cloud_widget_, &PointCloudWidget::resetView);
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
    });
    connect(color_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        cloud_widget_->setColorMode(static_cast<PointCloudColorMode>(
            color_combo_->currentData().toInt()));
    });
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
            workspace_status_->setText(interactive
                ? tr("交互预览 · %1 / %2 点 · %3")
                      .arg(displayed).arg(current_cloud_.Size()).arg(unitLabel())
                : tr("%1 点 · 显示 %2 · %3")
                      .arg(current_cloud_.Size()).arg(displayed).arg(unitLabel()));
        });
    connect(cloud_widget_, &PointCloudWidget::interactionCancelled, this, [this] {
        setMeasureMode(PointCloudMeasureMode::Navigate);
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
    connect(undo_button_, &QPushButton::clicked, this, &PointCloudDialog::undoProcessing);
    connect(redo_button_, &QPushButton::clicked, this, &PointCloudDialog::redoProcessing);
    connect(restore_button, &QPushButton::clicked, this, &PointCloudDialog::restoreOriginal);
    connect(cloud_widget_, &PointCloudWidget::pointPicked,
        this, &PointCloudDialog::acceptPickedPoint);
    connect(delete_measurement, &QPushButton::clicked, this, [this] {
        const int row = measurement_list_->currentRow();
        if (row < 0 || row >= static_cast<int>(measurements_.size())) return;
        measurements_.erase(measurements_.begin() + row);
        refreshMeasurementList();
    });
    connect(clear_measurements, &QPushButton::clicked, this, [this] {
        measurements_.clear();
        pending_points_.clear();
        cloud_widget_->setHighlightedIndices({});
        refreshMeasurementList();
    });
    connect(export_measurements, &QPushButton::clicked,
        this, &PointCloudDialog::exportMeasurements);
    connect(evaluate_tolerances, &QPushButton::clicked,
        this, &PointCloudDialog::evaluateTolerances);
    connect(deviation_button, &QPushButton::clicked,
        this, &PointCloudDialog::showDeviationDistribution);
    connect(begin_section, &QPushButton::clicked,
        this, &PointCloudDialog::beginSectionAnalysis);
    connect(fit_plane_model, &QPushButton::clicked, this,
        [this] { fitGeometricModel(PointCloudGeometricModelType::Plane); });
    connect(fit_sphere, &QPushButton::clicked, this,
        [this] { fitGeometricModel(PointCloudGeometricModelType::Sphere); });
    connect(fit_cylinder, &QPushButton::clicked, this,
        [this] { fitGeometricModel(PointCloudGeometricModelType::Cylinder); });
    connect(model_list_, &QListWidget::currentRowChanged,
        this, &PointCloudDialog::selectModelRow);
    connect(show_model, &QPushButton::clicked, this,
        [this] { setSelectedModelVisible(true); });
    connect(hide_model, &QPushButton::clicked, this,
        [this] { setSelectedModelVisible(false); });
    connect(delete_model, &QPushButton::clicked, this,
        &PointCloudDialog::deleteSelectedModel);
    connect(clear_models_button, &QPushButton::clicked, this,
        &PointCloudDialog::clearGeometricModels);
    connect(residual_coloring_check_, &QCheckBox::toggled,
        cloud_widget_, &PointCloudWidget::setResidualColoringEnabled);
    connect(cloud_widget_, &PointCloudWidget::sectionSelectionFinished,
        this, &PointCloudDialog::acceptSectionSelection);
    connect(close_button, &QPushButton::clicked, this, &QDialog::close);

    for (QWidget* action : std::initializer_list<QWidget*>{export_button, reset_view,
             voxel_apply, outlier_apply, smart_filter_apply, hole_repair_apply,
             begin_crop_button_, fit_button, restore_button, evaluate_tolerances,
             deviation_button, begin_section, fit_plane_model, fit_sphere, fit_cylinder}) {
        action->setProperty("requiresCloud", true);
    }
    resetInspectionResults();
    updateCloudPresentation({}, true);
    QTimer::singleShot(0, this, [this] { loadSettings(); });
}

void PointCloudDialog::setCloud(const PointCloud& cloud)
{
    setMeasureMode(PointCloudMeasureMode::Navigate);
    clearInteractiveCrop();
    original_cloud_ = current_cloud_ = cloud;
    ++cloud_revision_;
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
    resetInspectionResults();
    updateCloudPresentation({}, true);
    updateProcessingDefaults();
    refreshMeasurementList();
}

void PointCloudDialog::openCloud()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("打开 3D 点云"), {},
        tr("点云文件 (*.ply *.pcd *.xyz *.txt *.csv);;PLY (*.ply);;PCD (*.pcd);;XYZ/文本 (*.xyz *.txt *.csv);;所有文件 (*)"));
    if (path.isEmpty()) return;
    startCloudLoad(path);
}

void PointCloudDialog::startCloudLoad(const QString& path)
{
    auto* progress_dialog = new QProgressDialog(
        tr("正在读取点云…"), tr("取消"), 0, 1000, this);
    progress_dialog->setObjectName(QStringLiteral("PointCloudLoadProgressDialog"));
    progress_dialog->setWindowTitle(tr("导入 3D 点云"));
    progress_dialog->setWindowModality(Qt::WindowModal);
    progress_dialog->setMinimumDuration(0);
    progress_dialog->setAutoClose(false);
    progress_dialog->setAutoReset(false);
    progress_dialog->setValue(0);
    open_button_->setEnabled(false);

    auto cancelled = std::make_shared<std::atomic_bool>(false);
    auto progress_value = std::make_shared<std::atomic_int>(0);
    connect(progress_dialog, &QProgressDialog::canceled, this,
        [cancelled] { cancelled->store(true, std::memory_order_relaxed); });
    auto* progress_timer = new QTimer(progress_dialog);
    progress_timer->setInterval(40);
    connect(progress_timer, &QTimer::timeout, progress_dialog,
        [progress_dialog, progress_value] {
            progress_dialog->setValue(progress_value->load(std::memory_order_relaxed));
        });
    progress_timer->start();
    auto* watcher = new QFutureWatcher<PointCloudLoadResult>(this);
    connect(watcher, &QFutureWatcher<PointCloudLoadResult>::finished, this,
        [this, watcher, progress_dialog] {
            const PointCloudLoadResult result = watcher->result();
            watcher->deleteLater();
            progress_dialog->close();
            progress_dialog->deleteLater();
            open_button_->setEnabled(true);
            if (result.cancelled) return;
            if (!result.loaded) {
                QMessageBox::warning(this, tr("点云导入失败"), errorText(result.error));
                return;
            }
            setCloud(result.cloud);
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
        : tr("%1%2").arg(QString::fromStdWString(current_cloud_.name),
            operation.isEmpty() ? QString() : tr(" · %1").arg(operation)));
    if (current_cloud_.Empty()) {
        statistics_label_->setText(tr("点数：0"));
    } else {
        const auto center = current_cloud_.Centroid();
        statistics_label_->setText(
            tr("点数：%1\n范围：X %2，Y %3，Z %4 %5\n质心：(%6, %7, %8)")
                .arg(current_cloud_.Size())
                .arg(current_cloud_.bounds.Width(), 0, 'g', 7)
                .arg(current_cloud_.bounds.Depth(), 0, 'g', 7)
                .arg(current_cloud_.bounds.Height(), 0, 'g', 7)
                .arg(unitLabel())
                .arg(center.x, 0, 'g', 7)
                .arg(center.y, 0, 'g', 7)
                .arg(center.z, 0, 'g', 7));
    }
    undo_button_->setEnabled(!undo_stack_.empty());
    redo_button_->setEnabled(!redo_stack_.empty());
    level_button_->setEnabled(fitted_plane_.valid && has_cloud);
    workspace_status_->setText(has_cloud
        ? tr("%1 点 · 显示 %2 · %3")
              .arg(current_cloud_.Size()).arg(cloud_widget_->renderedPointCount()).arg(unitLabel())
        : tr("未载入点云"));
    updateSelectionPresentation();
}

void PointCloudDialog::resetInspectionResults()
{
    if (tolerance_summary_) {
        tolerance_summary_->setText(tr("设置各项公差上限后开始评级。"));
    }
    if (section_status_) {
        section_status_->setText(tr("自动生成 Z 高度剖面，计算阶梯差、截面宽度和凹槽深度。"));
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
    setMeasureMode(PointCloudMeasureMode::Navigate);
    undo_stack_.push_back(current_cloud_);
    redo_stack_.clear();
    current_cloud_ = std::move(cloud);
    ++cloud_revision_;
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
    if (current_cloud_.Empty()) return;
    const std::uint64_t revision = cloud_revision_;
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    auto* progress = new QProgressDialog(
        tr("正在%1…").arg(operation), tr("取消"), 0, 0, this);
    progress->setObjectName(QStringLiteral("PointCloudTaskProgressDialog"));
    progress->setWindowTitle(tr("点云处理"));
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(250);
    progress->setAutoClose(false);
    connect(progress, &QProgressDialog::canceled, this,
        [cancelled] { cancelled->store(true, std::memory_order_relaxed); });
    tabs_->setEnabled(false);
    workspace_status_->setText(tr("正在%1；可取消").arg(operation));
    auto* watcher = new QFutureWatcher<PointCloud>(this);
    connect(watcher, &QFutureWatcher<PointCloud>::finished, this,
        [this, watcher, progress, cancelled, revision, operation,
            completed = std::move(completed)]() mutable {
            PointCloud result = watcher->result();
            watcher->deleteLater();
            progress->close();
            progress->deleteLater();
            tabs_->setEnabled(true);
            if (cancelled->load(std::memory_order_relaxed)) {
                workspace_status_->setText(tr("已取消%1").arg(operation));
                return;
            }
            if (revision != cloud_revision_) {
                workspace_status_->setText(tr("点云已改变，已丢弃过期结果"));
                return;
            }
            pushProcessedCloud(std::move(result), operation);
            if (completed) completed();
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
        setMeasureMode(PointCloudMeasureMode::Navigate);
        begin_crop_button_->setChecked(true);
    }
    crop_selection_.clear();
    cloud_widget_->setSelectionPreviewIndices({});
    cloud_widget_->setFreeSelectionEnabled(active);
    if (free_selection_button_) {
        const QSignalBlocker navigation_blocker(navigation_button_);
        const QSignalBlocker selection_blocker(free_selection_button_);
        navigation_button_->setChecked(!active);
        free_selection_button_->setChecked(active);
    }
    keep_crop_button_->setEnabled(false);
    remove_crop_button_->setEnabled(false);
    crop_selection_label_->setText(active
        ? tr("在点云视图中按住左键沿目标轮廓绘制自由选区。")
        : tr("尚未选择点"));
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
    clearInteractiveCrop();
    const QString operation = keep_selected ? tr("保留框选点") : tr("移除框选点");
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
}

void PointCloudDialog::clearFittedPlane()
{
    fitted_plane_ = {};
    if (cloud_widget_) cloud_widget_->setFittedPlane({});
    if (show_plane_check_) show_plane_check_->setEnabled(false);
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
    geometric_models_.clear();
    active_model_id_ = 0;
    plane_model_count_ = sphere_model_count_ = cylinder_model_count_ = 0;
    if (cloud_widget_) {
        cloud_widget_->setGeometricModels({});
        cloud_widget_->setActiveGeometricModel(0);
    }
    refreshModelList();
}

void PointCloudDialog::fitGeometricModel(PointCloudGeometricModelType type)
{
    if (current_cloud_.Empty()) return;
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
        fit_status_->setText(tr("当前选择不足：该模型至少需要 %1 个点。")
            .arg(minimum_points));
        fit_status_->setProperty("status", QStringLiteral("error"));
        fit_status_->style()->unpolish(fit_status_);
        fit_status_->style()->polish(fit_status_);
        return;
    }
    if (options.minimum_radius > 0.0 && options.maximum_radius > 0.0 &&
        options.minimum_radius > options.maximum_radius) {
        fit_status_->setText(tr("半径约束冲突：最小半径不能大于最大半径。"));
        return;
    }
    const PointCloud cloud = current_cloud_;
    const std::uint64_t revision = cloud_revision_;
    fit_status_->setText(tr("正在后台拟合，请稍候…"));
    tabs_->setEnabled(false);
    auto* watcher = new QFutureWatcher<PointCloudFitResult>(this);
    connect(watcher, &QFutureWatcher<PointCloudFitResult>::finished, this,
        [this, watcher, revision] {
            PointCloudFitResult result = watcher->result();
            watcher->deleteLater();
            tabs_->setEnabled(true);
            acceptFitResult(std::move(result), revision);
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

void PointCloudDialog::acceptFitResult(PointCloudFitResult result, std::uint64_t revision)
{
    if (revision != cloud_revision_) {
        fit_status_->setText(tr("点云已改变，本次拟合结果已丢弃。"));
        return;
    }
    if (!result.valid) {
        fit_status_->setText(tr("拟合失败：%1").arg(errorText(result.error)));
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
    fitted_plane_ = result.model.type == PointCloudGeometricModelType::Plane
        ? result.model.plane : fitted_plane_;
    geometric_models_.push_back(std::move(result.model));
    active_model_id_ = geometric_models_.back().id;
    cloud_widget_->setGeometricModels(geometric_models_);
    cloud_widget_->setActiveGeometricModel(active_model_id_);
    refreshModelList();
    model_list_->setCurrentRow(static_cast<int>(geometric_models_.size()) - 1);
    fit_status_->setText(tr("拟合完成：内点 %1/%2，RMS %3 %4")
        .arg(geometric_models_.back().quality.inlier_count)
        .arg(geometric_models_.back().quality.sample_count)
        .arg(geometric_models_.back().quality.rms, 0, 'g', 7).arg(unitLabel()));
}

void PointCloudDialog::refreshModelList()
{
    if (!model_list_) return;
    const int old_row = model_list_->currentRow();
    model_list_->clear();
    for (const auto& model : geometric_models_) {
        const QString status = model.visible ? tr("显示") : tr("隐藏");
        model_list_->addItem(tr("%1 · %2 · RMS %3")
            .arg(QString::fromStdWString(model.name), status)
            .arg(model.quality.rms, 0, 'g', 5));
    }
    if (!geometric_models_.empty()) {
        model_list_->setCurrentRow(std::clamp(old_row, 0,
            static_cast<int>(geometric_models_.size()) - 1));
    } else if (model_details_) {
        model_details_->setText(tr("尚未创建模型"));
    }
}

void PointCloudDialog::selectModelRow(int row)
{
    if (row < 0 || row >= static_cast<int>(geometric_models_.size())) {
        active_model_id_ = 0;
        cloud_widget_->setActiveGeometricModel(0);
        return;
    }
    const PointCloudGeometricModel& model = geometric_models_[static_cast<std::size_t>(row)];
    active_model_id_ = model.id;
    cloud_widget_->setActiveGeometricModel(active_model_id_);
    QString parameters;
    if (model.type == PointCloudGeometricModelType::Plane) {
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
}

void PointCloudDialog::setSelectedModelVisible(bool visible)
{
    const int row = model_list_ ? model_list_->currentRow() : -1;
    if (row < 0 || row >= static_cast<int>(geometric_models_.size())) return;
    geometric_models_[static_cast<std::size_t>(row)].visible = visible;
    cloud_widget_->setGeometricModels(geometric_models_);
    refreshModelList();
    model_list_->setCurrentRow(row);
}

void PointCloudDialog::deleteSelectedModel()
{
    const int row = model_list_ ? model_list_->currentRow() : -1;
    if (row < 0 || row >= static_cast<int>(geometric_models_.size())) return;
    geometric_models_.erase(geometric_models_.begin() + row);
    active_model_id_ = 0;
    cloud_widget_->setGeometricModels(geometric_models_);
    refreshModelList();
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
        QMessageBox::information(this, tr("截面分析"), tr("请先打开点云数据。"));
        return;
    }
    setMeasureMode(PointCloudMeasureMode::Navigate);
    cloud_widget_->setSectionSelectionEnabled(true, section_width_spin_->value());
    section_status_->setText(tr("选择中：请在左侧点云中按住左键拉出截面线。"));
}

void PointCloudDialog::acceptSectionSelection(
    const QVector<int>& indices,
    const QPointF&,
    const QPointF&)
{
    std::vector<std::size_t> source_indices;
    source_indices.reserve(indices.size());
    for (int index : indices) {
        if (index >= 0) source_indices.push_back(static_cast<std::size_t>(index));
    }
    const PointCloudSectionProfile profile = PointCloudSectionAnalyzer::Analyze(
        current_cloud_, source_indices);
    if (!profile.valid) {
        section_status_->setText(tr("有效点不足，请增大采样半宽后重新选择。"));
        QMessageBox::information(this, tr("截面分析"),
            tr("截面线附近的有效点不足，请增大采样半宽或重新选择。"));
        return;
    }
    section_status_->setText(tr("已生成截面：%1 个剖面采样点，宽度 %2 %3。")
        .arg(profile.samples.size()).arg(profile.width, 0, 'g', 8).arg(unitLabel()));
    auto* dialog = new PointCloudSectionDialog(profile, unitLabel(), this);
    dialog->show();
    dialog->raise();
}

void PointCloudDialog::fitPlane()
{
    fitted_plane_ = PointCloudProcessor::FitPlane(current_cloud_);
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
    setMeasureMode(PointCloudMeasureMode::Navigate);
    redo_stack_.push_back(current_cloud_);
    current_cloud_ = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    ++cloud_revision_;
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
    setMeasureMode(PointCloudMeasureMode::Navigate);
    undo_stack_.push_back(current_cloud_);
    current_cloud_ = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    ++cloud_revision_;
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
    setMeasureMode(PointCloudMeasureMode::Navigate);
    undo_stack_.clear();
    redo_stack_.clear();
    current_cloud_ = original_cloud_;
    ++cloud_revision_;
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
    if (measurement_tool_group_) {
        if (auto* button = measurement_tool_group_->button(static_cast<int>(mode))) {
            button->setChecked(true);
        }
    }
    pending_points_.clear();
    cloud_widget_->setHighlightedIndices({});
    cloud_widget_->setPickingEnabled(mode != PointCloudMeasureMode::Navigate);
    QString hint;
    switch (mode) {
    case PointCloudMeasureMode::Point: hint = tr("点击一个点读取 XYZ 坐标。"); break;
    case PointCloudMeasureMode::Distance: hint = tr("依次点击两个点测量三维距离。"); break;
    case PointCloudMeasureMode::HeightDifference: hint = tr("依次点击两个点测量有符号 Z 高度差。"); break;
    case PointCloudMeasureMode::Angle: hint = tr("依次点击端点、顶点、端点测量三维角度。"); break;
    case PointCloudMeasureMode::PointToPlane:
        hint = fitted_plane_.valid ? tr("点击一点测量到参考平面的垂直距离。")
                                   : tr("请先在“处理”页拟合参考平面。");
        break;
    case PointCloudMeasureMode::PlaneAngle:
        hint = tr("依次点选第一平面 3 点和第二平面 3 点。");
        break;
    case PointCloudMeasureMode::LineIntersection:
        hint = tr("依次点选第一直线 2 点和第二直线 2 点。");
        break;
    case PointCloudMeasureMode::Navigate:
    default: hint = tr("浏览模式：左键旋转、右键平移、滚轮缩放。"); break;
    }
    measurement_hint_->setText(hint);
}

void PointCloudDialog::acceptPickedPoint(int index)
{
    if (index < 0 || index >= static_cast<int>(current_cloud_.points.size()) ||
        measure_mode_ == PointCloudMeasureMode::Navigate) return;
    if (measure_mode_ == PointCloudMeasureMode::PointToPlane && !fitted_plane_.valid) {
        measurement_hint_->setText(tr("请先拟合参考平面。"));
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
    measurements_.push_back(std::move(record));
    pending_points_.clear();
    cloud_widget_->setHighlightedIndices({});
    refreshMeasurementList();
    setMeasureMode(measure_mode_);
}

void PointCloudDialog::refreshMeasurementList()
{
    measurement_list_->clear();
    for (std::size_t index = 0; index < measurements_.size(); ++index) {
        measurement_list_->addItem(tr("%1. %2：%3")
            .arg(index + 1).arg(measurements_[index].type, measurements_[index].value));
    }
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
    if (workspace_splitter_ && !automated_test) {
        const QByteArray state = settings.value(QStringLiteral("splitterState")).toByteArray();
        if (!state.isEmpty()) workspace_splitter_->restoreState(state);
    }
    if (tabs_ && !automated_test) tabs_->setCurrentIndex(settings.value(QStringLiteral("tab"), 0).toInt());
    if (point_size_spin_) point_size_spin_->setValue(settings.value(QStringLiteral("pointSize"), 2.5).toDouble());
    if (color_combo_) color_combo_->setCurrentIndex(settings.value(QStringLiteral("colorMode"), 0).toInt());
    if (axes_check_) axes_check_->setChecked(settings.value(QStringLiteral("axes"), true).toBool());
    if (cylinder_axis_combo_) cylinder_axis_combo_->setCurrentIndex(settings.value(QStringLiteral("cylinderAxis"), 0).toInt());
    if (fit_threshold_spin_) fit_threshold_spin_->setValue(settings.value(QStringLiteral("fitThreshold"), 0.0).toDouble());
    settings.endGroup();
}

void PointCloudDialog::saveSettings() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("PointCloudWorkspace"));
    if (workspace_splitter_) settings.setValue(QStringLiteral("splitterState"), workspace_splitter_->saveState());
    if (tabs_) settings.setValue(QStringLiteral("tab"), tabs_->currentIndex());
    if (point_size_spin_) settings.setValue(QStringLiteral("pointSize"), point_size_spin_->value());
    if (color_combo_) settings.setValue(QStringLiteral("colorMode"), color_combo_->currentIndex());
    if (axes_check_) settings.setValue(QStringLiteral("axes"), axes_check_->isChecked());
    if (cylinder_axis_combo_) settings.setValue(QStringLiteral("cylinderAxis"), cylinder_axis_combo_->currentIndex());
    if (fit_threshold_spin_) settings.setValue(QStringLiteral("fitThreshold"), fit_threshold_spin_->value());
    settings.endGroup();
}

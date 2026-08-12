#include "PointCloudDialog.h"

#include "PointCloudWidget.h"
#include "pointcloud/PointCloudIO.h"
#include "pointcloud/PointCloudMeasurement.h"
#include "pointcloud/PointCloudProcessor.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QProgressDialog>
#include <QScrollArea>
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

void PointCloudDialog::buildUi()
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("3D 点云工作台"));
    setObjectName(QStringLiteral("PointCloudDialog"));
    resize(1280, 820);
    setMinimumSize(960, 620);
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);
    cloud_widget_ = new PointCloudWidget;
    root->addWidget(cloud_widget_, 1);

    auto* side = new QWidget;
    side->setMinimumWidth(330);
    side->setMaximumWidth(390);
    auto* side_layout = new QVBoxLayout(side);
    side_layout->setContentsMargins(0, 0, 0, 0);
    source_label_ = new QLabel(tr("尚未载入点云"));
    source_label_->setObjectName(QStringLiteral("PointCloudSourceLabel"));
    source_label_->setWordWrap(true);
    source_label_->setProperty("role", QStringLiteral("summary"));
    side_layout->addWidget(source_label_);
    auto* tabs = new QTabWidget;
    tabs->setObjectName(QStringLiteral("PointCloudToolTabs"));
    side_layout->addWidget(tabs, 1);

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
    auto* axes_check = new QCheckBox(tr("显示 XYZ 坐标轴"));
    axes_check->setObjectName(QStringLiteral("PointCloudAxesCheck"));
    axes_check->setChecked(true);
    backend_label_ = new QLabel(cloud_widget_->renderBackend());
    backend_label_->setObjectName(QStringLiteral("PointCloudRenderBackend"));
    backend_label_->setWordWrap(true);
    display_form->addRow(tr("坐标单位"), unit_combo_);
    display_form->addRow(tr("着色"), color_combo_);
    display_form->addRow(tr("点大小"), point_size_spin_);
    display_form->addRow({}, axes_check);
    display_form->addRow(tr("渲染后端"), backend_label_);
    data_layout->addWidget(display_group);
    auto* reset_view = new QPushButton(tr("复位视角"));
    reset_view->setObjectName(QStringLiteral("PointCloudResetViewButton"));
    data_layout->addWidget(reset_view);
    data_layout->addStretch();
    tabs->addTab(data_page, tr("数据/显示"));

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
    auto* crop_group = new QGroupBox(tr("坐标范围裁剪"));
    auto* crop_form = new QFormLayout(crop_group);
    crop_min_x_ = coordinateSpin(QStringLiteral("PointCloudCropMinX"));
    crop_max_x_ = coordinateSpin(QStringLiteral("PointCloudCropMaxX"));
    crop_min_y_ = coordinateSpin(QStringLiteral("PointCloudCropMinY"));
    crop_max_y_ = coordinateSpin(QStringLiteral("PointCloudCropMaxY"));
    crop_min_z_ = coordinateSpin(QStringLiteral("PointCloudCropMinZ"));
    crop_max_z_ = coordinateSpin(QStringLiteral("PointCloudCropMaxZ"));
    crop_form->addRow(QStringLiteral("X"), rowOf({crop_min_x_, crop_max_x_}));
    crop_form->addRow(QStringLiteral("Y"), rowOf({crop_min_y_, crop_max_y_}));
    crop_form->addRow(QStringLiteral("Z"), rowOf({crop_min_z_, crop_max_z_}));
    auto* crop_apply = new QPushButton(tr("应用裁剪"));
    crop_apply->setObjectName(QStringLiteral("PointCloudCropApplyButton"));
    crop_form->addRow({}, crop_apply);
    process_layout->addWidget(crop_group);
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
    plane_layout->addWidget(plane_label_);
    plane_layout->addWidget(rowOf({fit_button, level_button_}));
    process_layout->addWidget(plane_group);
    undo_button_ = new QPushButton(tr("撤销上一步处理"));
    undo_button_->setObjectName(QStringLiteral("PointCloudUndoButton"));
    undo_button_->setEnabled(false);
    auto* restore_button = new QPushButton(tr("恢复原始点云"));
    restore_button->setObjectName(QStringLiteral("PointCloudRestoreButton"));
    process_layout->addWidget(rowOf({undo_button_, restore_button}));
    process_layout->addStretch();
    process_page->setWidget(process_content);
    tabs->addTab(process_page, tr("处理"));

    auto* measure_page = new QWidget;
    auto* measure_layout = new QVBoxLayout(measure_page);
    measurement_hint_ = new QLabel(tr("选择测量工具后，在点云中点击取点。"));
    measurement_hint_->setObjectName(QStringLiteral("PointCloudMeasurementHint"));
    measurement_hint_->setWordWrap(true);
    measurement_hint_->setProperty("role", QStringLiteral("summary"));
    measure_layout->addWidget(measurement_hint_);
    auto* tool_group = new QButtonGroup(this);
    tool_group->setExclusive(true);
    auto addTool = [this, measure_layout, tool_group](
                       const QString& text, const QString& name,
                       PointCloudMeasureMode mode) {
        auto* button = new QPushButton(text);
        button->setObjectName(name);
        button->setCheckable(true);
        tool_group->addButton(button);
        measure_layout->addWidget(button);
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
    tabs->addTab(measure_page, tr("测量"));

    auto* close_button = new QPushButton(tr("关闭"));
    side_layout->addWidget(close_button);
    root->addWidget(side);

    connect(open_button_, &QPushButton::clicked, this, &PointCloudDialog::openCloud);
    connect(export_button, &QPushButton::clicked, this, &PointCloudDialog::exportCloud);
    connect(reset_view, &QPushButton::clicked, cloud_widget_, &PointCloudWidget::resetView);
    connect(unit_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        current_cloud_.unit = static_cast<PointCloudUnit>(unit_combo_->currentData().toInt());
        original_cloud_.unit = current_cloud_.unit;
        for (PointCloud& cloud : undo_stack_) cloud.unit = current_cloud_.unit;
        measurements_.clear();
        pending_points_.clear();
        cloud_widget_->setHighlightedIndices({});
        updateCloudPresentation();
        refreshMeasurementList();
    });
    connect(color_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        cloud_widget_->setColorMode(static_cast<PointCloudColorMode>(
            color_combo_->currentData().toInt()));
    });
    connect(point_size_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
        cloud_widget_, &PointCloudWidget::setPointSize);
    connect(axes_check, &QCheckBox::toggled, cloud_widget_, &PointCloudWidget::setAxesVisible);
    connect(cloud_widget_, &PointCloudWidget::renderBackendChanged, this,
        [this](const QString& description, bool hardware) {
            backend_label_->setText(hardware
                ? tr("%1（硬件加速）").arg(description)
                : tr("%1（软件回退）").arg(description));
        });
    connect(voxel_apply, &QPushButton::clicked, this, &PointCloudDialog::applyVoxelDownsample);
    connect(outlier_apply, &QPushButton::clicked, this, &PointCloudDialog::applyOutlierRemoval);
    connect(crop_apply, &QPushButton::clicked, this, &PointCloudDialog::applyCrop);
    connect(begin_crop_button_, &QPushButton::clicked,
        this, &PointCloudDialog::beginInteractiveCrop);
    connect(keep_crop_button_, &QPushButton::clicked,
        this, [this] { applyInteractiveCrop(true); });
    connect(remove_crop_button_, &QPushButton::clicked,
        this, [this] { applyInteractiveCrop(false); });
    connect(cloud_widget_, &PointCloudWidget::boxSelectionFinished,
        this, &PointCloudDialog::acceptBoxSelection);
    connect(fit_button, &QPushButton::clicked, this, &PointCloudDialog::fitPlane);
    connect(level_button_, &QPushButton::clicked, this, &PointCloudDialog::levelCloud);
    connect(undo_button_, &QPushButton::clicked, this, &PointCloudDialog::undoProcessing);
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
    connect(close_button, &QPushButton::clicked, this, &QDialog::close);
}

void PointCloudDialog::setCloud(const PointCloud& cloud)
{
    clearInteractiveCrop();
    original_cloud_ = current_cloud_ = cloud;
    if (!current_cloud_.Empty() && !current_cloud_.bounds.valid) {
        original_cloud_.RecalculateBounds();
        current_cloud_ = original_cloud_;
    }
    undo_stack_.clear();
    measurements_.clear();
    pending_points_.clear();
    fitted_plane_ = {};
    const int unit_index = unit_combo_->findData(static_cast<int>(current_cloud_.unit));
    if (unit_index >= 0) unit_combo_->setCurrentIndex(unit_index);
    updateCloudPresentation();
    updateCropRanges();
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

void PointCloudDialog::updateCloudPresentation(const QString& operation)
{
    cloud_widget_->setCloud(current_cloud_);
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
    level_button_->setEnabled(fitted_plane_.valid && !current_cloud_.Empty());
}

void PointCloudDialog::updateCropRanges()
{
    if (!current_cloud_.bounds.valid) return;
    crop_min_x_->setValue(current_cloud_.bounds.min_x);
    crop_max_x_->setValue(current_cloud_.bounds.max_x);
    crop_min_y_->setValue(current_cloud_.bounds.min_y);
    crop_max_y_->setValue(current_cloud_.bounds.max_y);
    crop_min_z_->setValue(current_cloud_.bounds.min_z);
    crop_max_z_->setValue(current_cloud_.bounds.max_z);
    const double extent = std::max({current_cloud_.bounds.Width(), current_cloud_.bounds.Depth(),
        current_cloud_.bounds.Height(), 1e-6});
    voxel_spin_->setValue(extent / 100.0);
    outlier_radius_spin_->setValue(extent / 50.0);
}

void PointCloudDialog::pushProcessedCloud(PointCloud cloud, const QString& operation)
{
    if (cloud.Empty()) {
        QMessageBox::information(this, tr("点云处理"), tr("该参数会移除所有点，请调整后重试。"));
        return;
    }
    clearInteractiveCrop();
    undo_stack_.push_back(current_cloud_);
    current_cloud_ = std::move(cloud);
    fitted_plane_ = {};
    measurements_.clear();
    pending_points_.clear();
    updateCloudPresentation(operation);
    updateCropRanges();
    refreshMeasurementList();
}

void PointCloudDialog::applyVoxelDownsample()
{
    pushProcessedCloud(PointCloudProcessor::VoxelDownsample(
        current_cloud_, voxel_spin_->value()), tr("体素降采样"));
}

void PointCloudDialog::applyOutlierRemoval()
{
    pushProcessedCloud(PointCloudProcessor::RemoveRadiusOutliers(
        current_cloud_, outlier_radius_spin_->value(),
        static_cast<std::size_t>(outlier_neighbors_spin_->value())), tr("离群点过滤"));
}

void PointCloudDialog::applyCrop()
{
    PointCloudBounds bounds;
    bounds.min_x = crop_min_x_->value();
    bounds.max_x = crop_max_x_->value();
    bounds.min_y = crop_min_y_->value();
    bounds.max_y = crop_max_y_->value();
    bounds.min_z = crop_min_z_->value();
    bounds.max_z = crop_max_z_->value();
    bounds.valid = bounds.min_x <= bounds.max_x && bounds.min_y <= bounds.max_y &&
        bounds.min_z <= bounds.max_z;
    if (!bounds.valid) {
        QMessageBox::warning(this, tr("点云裁剪"), tr("每个坐标轴的最小值必须小于或等于最大值。"));
        return;
    }
    pushProcessedCloud(PointCloudProcessor::Crop(current_cloud_, bounds), tr("范围裁剪"));
}

void PointCloudDialog::beginInteractiveCrop()
{
    if (current_cloud_.Empty()) {
        begin_crop_button_->setChecked(false);
        QMessageBox::information(this, tr("交互式裁剪"), tr("请先打开点云数据。"));
        return;
    }
    const bool active = begin_crop_button_->isChecked();
    crop_selection_.clear();
    cloud_widget_->setSelectionPreviewIndices({});
    cloud_widget_->setBoxSelectionEnabled(active);
    keep_crop_button_->setEnabled(false);
    remove_crop_button_->setEnabled(false);
    crop_selection_label_->setText(active
        ? tr("在点云视图中按住左键拖出选区。")
        : tr("尚未选择点"));
}

void PointCloudDialog::acceptBoxSelection(const QVector<int>& indices)
{
    crop_selection_ = indices;
    cloud_widget_->setSelectionPreviewIndices(crop_selection_);
    begin_crop_button_->setChecked(false);
    cloud_widget_->setBoxSelectionEnabled(false);
    const bool valid = !crop_selection_.isEmpty();
    keep_crop_button_->setEnabled(valid);
    remove_crop_button_->setEnabled(valid && crop_selection_.size() < current_cloud_.Size());
    crop_selection_label_->setText(valid
        ? tr("已选择 %1 / %2 个点，黄色为裁剪预览。")
              .arg(crop_selection_.size()).arg(current_cloud_.Size())
        : tr("选区中没有点，请重新拖框。"));
}

void PointCloudDialog::applyInteractiveCrop(bool keep_selected)
{
    if (crop_selection_.isEmpty()) return;
    std::vector<std::size_t> indices;
    indices.reserve(crop_selection_.size());
    for (int index : crop_selection_) {
        if (index >= 0) indices.push_back(static_cast<std::size_t>(index));
    }
    PointCloud result = PointCloudProcessor::SelectIndices(
        current_cloud_, indices, keep_selected);
    clearInteractiveCrop();
    pushProcessedCloud(std::move(result),
        keep_selected ? tr("保留框选点") : tr("移除框选点"));
}

void PointCloudDialog::clearInteractiveCrop()
{
    crop_selection_.clear();
    if (cloud_widget_) {
        cloud_widget_->setBoxSelectionEnabled(false);
        cloud_widget_->setSelectionPreviewIndices({});
    }
    if (begin_crop_button_) begin_crop_button_->setChecked(false);
    if (keep_crop_button_) keep_crop_button_->setEnabled(false);
    if (remove_crop_button_) remove_crop_button_->setEnabled(false);
    if (crop_selection_label_) crop_selection_label_->setText(tr("尚未选择点"));
}

void PointCloudDialog::fitPlane()
{
    fitted_plane_ = PointCloudProcessor::FitPlane(current_cloud_);
    if (!fitted_plane_.valid) {
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
    current_cloud_ = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    fitted_plane_ = {};
    measurements_.clear();
    pending_points_.clear();
    updateCloudPresentation(tr("已撤销"));
    updateCropRanges();
    refreshMeasurementList();
}

void PointCloudDialog::restoreOriginal()
{
    if (original_cloud_.Empty()) return;
    undo_stack_.clear();
    current_cloud_ = original_cloud_;
    fitted_plane_ = {};
    measurements_.clear();
    pending_points_.clear();
    updateCloudPresentation(tr("原始数据"));
    updateCropRanges();
    refreshMeasurementList();
}

void PointCloudDialog::setMeasureMode(PointCloudMeasureMode mode)
{
    clearInteractiveCrop();
    measure_mode_ = mode;
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
    const int required = measure_mode_ == PointCloudMeasureMode::Angle ? 3
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

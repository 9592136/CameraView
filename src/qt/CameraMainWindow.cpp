#include "CameraMainWindow.h"

#include "CalibrationDialog.h"
#include "CameraWorker.h"
#include "FluorescenceCaptureSettings.h"
#include "HistogramWidget.h"
#include "ImageSurface3DDialog.h"
#include "MeasurementToolButton.h"
#include "NumericSlider.h"
#include "ObjectiveCalibrationSettings.h"
#include "ProfileAnalysisDialog.h"
#include "PointCloudDialog.h"
#include "ReportTemplateDialog.h"
#include "ai/YoloWorkspaceWidget.h"
#include "app/ExportActions.h"
#include "domain/MeasurementFormatter.h"
#include "domain/MeasurementNameFormatter.h"
#include "imaging/ChannelFusionEngine.h"
#include "imaging/DyeLibrary.h"
#include "imaging/EdfStackListActions.h"
#include "imaging/FluorescenceChannelAnalysis.h"
#include "imaging/FluorescenceChannelListActions.h"
#include "imaging/FluorescenceChannelUpdater.h"
#include "imaging/FluorescenceFormatter.h"
#include "imaging/HistogramCalculator.h"
#include "imaging/LiveStitchCapturePlanner.h"
#include "imaging/LiveStitchPreviewBuilder.h"
#include "imaging/ProcessingJobExecutor.h"
#include "imaging/ProcessingParameterRules.h"
#include "imaging/StitchTileListActions.h"
#include "storage/MeasurementCsvExporter.h"
#include "storage/ProjectRepository.h"
#include "storage/ProjectSessionMapper.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCollator>
#include <QColorDialog>
#include <QDockWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QDateTime>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QImageWriter>
#include <QInputDialog>
#include <QLabel>
#include <QLineF>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QPainter>
#include <QShortcut>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QStyle>
#include <QTimer>
#include <QTransform>
#include <QTime>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <utility>

namespace {

constexpr int kLiveStitchPreviewMaxEdge = 960;
constexpr int kLiveStitchPreviewTileMaxEdge = 640;
constexpr int kLiveStitchRegistrationMaxEdge = 224;
constexpr int kLiveStitchMinMovementPercent = 15;
constexpr int kLiveStitchMinOverlapPercent = 15;
constexpr int kLiveStitchReferenceTileCount = 5;
constexpr int kLiveStitchOutOfRangeWarningFrames = 3;
constexpr int kLiveStitchMissingMatchWarningFrames = 6;
constexpr qint64 kLiveStitchWarningBeepMinIntervalMs = 2500;

bool editObjectiveCalibrationRecord(
    QWidget* parent,
    const QString& title,
    QString& objectiveLabel,
    double& micronsPerPixel)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setObjectName(QStringLiteral("ObjectiveCalibrationEditor"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* label = new QLineEdit(objectiveLabel, &dialog);
    label->setObjectName(QStringLiteral("ObjectiveCalibrationName"));
    label->setPlaceholderText(QObject::tr("例如：20x、40x 油镜"));
    auto* scale = new QDoubleSpinBox(&dialog);
    scale->setObjectName(QStringLiteral("ObjectiveCalibrationScale"));
    scale->setDecimals(10);
    scale->setRange(0.0, 1000000.0);
    scale->setSingleStep(0.01);
    scale->setSpecialValueText(QObject::tr("未标定"));
    scale->setSuffix(QObject::tr(" µm / px"));
    scale->setValue(std::max(0.0, micronsPerPixel));
    form->addRow(QObject::tr("物镜倍率"), label);
    form->addRow(QObject::tr("标定值"), scale);
    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    label->setFocus();
    label->selectAll();
    if (dialog.exec() != QDialog::Accepted) return false;
    objectiveLabel = label->text().trimmed();
    micronsPerPixel = scale->value();
    if (objectiveLabel.isEmpty()) {
        QMessageBox::warning(parent, QObject::tr("物镜标定"), QObject::tr("物镜倍率不能为空。"));
        return false;
    }
    return true;
}

QPushButton* addButton(QBoxLayout* layout, const QString& text)
{
    auto* button = new QPushButton(text);
    layout->addWidget(button);
    return button;
}

QVBoxLayout* panelLayout(QWidget* page)
{
    page->setProperty("panelPage", true);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(14, 14, 14, 18);
    layout->setSpacing(10);
    return layout;
}

QScrollArea* scrollablePanel(QWidget* page)
{
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(page);
    return scroll;
}

void setButtonRole(QPushButton* button, const char* role)
{
    button->setProperty("role", QString::fromLatin1(role));
}

struct LiveStitchEvaluation {
    LiveStitchCaptureDecision decision;
    ImageFrame frame;
    std::size_t baseTileCount = 0;
    quint64 generation = 0;
    qint64 elapsedMs = 0;
};

QWidget* buttonRow(std::initializer_list<QPushButton*> buttons)
{
    auto* widget = new QWidget;
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    for (QPushButton* button : buttons) {
        layout->addWidget(button);
    }
    return widget;
}

QString errorText(const std::wstring& value)
{
    return QString::fromStdWString(value);
}

QString imageFilterName(ImageFilterKind kind)
{
    switch (kind) {
    case ImageFilterKind::Grayscale: return QObject::tr("灰度");
    case ImageFilterKind::Invert: return QObject::tr("反相");
    case ImageFilterKind::AutoContrast: return QObject::tr("自动对比度");
    case ImageFilterKind::HistogramEqualization: return QObject::tr("直方图均衡");
    case ImageFilterKind::GaussianBlur: return QObject::tr("高斯平滑");
    case ImageFilterKind::MedianDenoise: return QObject::tr("中值降噪");
    case ImageFilterKind::Sharpen: return QObject::tr("反锐化增强");
    case ImageFilterKind::EdgeDetection: return QObject::tr("边缘检测");
    case ImageFilterKind::BinaryThreshold: return QObject::tr("二值化");
    }
    return QObject::tr("未知处理");
}

QString imageFilterStepDescription(const ImageFilterStep& step)
{
    const QString name = imageFilterName(step.kind);
    switch (step.kind) {
    case ImageFilterKind::GaussianBlur:
    case ImageFilterKind::MedianDenoise:
        return QObject::tr("%1（半径 %2 px）").arg(name).arg(step.parameter);
    case ImageFilterKind::Sharpen:
        return QObject::tr("%1（%2%）").arg(name).arg(step.parameter);
    case ImageFilterKind::EdgeDetection:
    case ImageFilterKind::BinaryThreshold:
        return QObject::tr("%1（阈值 %2）").arg(name).arg(step.parameter);
    default:
        return name;
    }
}

} // namespace

CameraMainWindow::CameraMainWindow(QWidget* parent)
    : QMainWindow(parent), dyes_(DyeLibrary::DefaultDyes())
{
    const ObjectiveCalibrationState objective_state = ObjectiveCalibrationSettings::Defaults();
    objective_labels_ = objective_state.labels;
    objective_calibrations_ = objective_state.calibrations;
    selected_objective_index_ = objective_state.selected_index;
    setupUi();
    loadMeasurementPreferences();
    loadObjectiveCalibrationMemory();
    loadReportTemplateSettings();
    {
        QSettings fluorescence_settings;
        fluorescence_capture_presets_ = FluorescenceCaptureSettings::Load(
            fluorescence_settings, dyes_);
        refreshFluorescencePresetList();
    }
    setupMenusAndToolbar();
    QSettings settings;
    settings.beginGroup(QStringLiteral("MainWindow"));
    restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("state")).toByteArray());
    settings.endGroup();
    if (measurement_toolbar_) {
        removeToolBarBreak(measurement_toolbar_);
        // Older saved QMainWindow state can restore the former 24 px toolbar size.
        // Re-apply the shared measurement-button size after restoring the layout.
        measurement_toolbar_->setIconSize(QSize(30, 30));
        for (QAction* action : measurement_toolbar_->actions()) {
            if (auto* button = qobject_cast<QToolButton*>(
                    measurement_toolbar_->widgetForAction(action))) {
                button->setIconSize(QSize(30, 30));
            }
        }
    }
    setAcceptDrops(true);

    camera_worker_ = new CameraWorker;
    camera_worker_->moveToThread(&camera_thread_);
    connect(&camera_thread_, &QThread::started, camera_worker_, &CameraWorker::initialize);
    connect(&camera_thread_, &QThread::finished, camera_worker_, &QObject::deleteLater);
    connect(camera_worker_, &CameraWorker::devicesReady, this, &CameraMainWindow::onDevicesReady);
    connect(camera_worker_, &CameraWorker::frameReady, this, &CameraMainWindow::onCameraFrame);
    connect(camera_worker_, &CameraWorker::cameraCapabilitiesChanged,
        this, &CameraMainWindow::onCameraCapabilities);
    connect(camera_worker_, &CameraWorker::cameraStateChanged, this,
        [this](bool opened, const QString& message) {
            camera_open_ = opened;
            if (!opened && live_stitch_active_) stopLiveStitch(false);
            if (!opened && canvas_->tool() == CanvasTool::CameraRoi) {
                restoreToolAfterCameraRoi(tr("相机已断开，ROI 框选已取消"));
            }
            preview_fps_timer_.invalidate();
            preview_frames_since_sample_ = 0;
            if (opened && message.contains(tr("正在重配置"))) {
                setCameraPanelState(CameraPanelState::Reconfiguring, message);
            } else if (opened && message.contains(tr("等待"))) {
                setCameraPanelState(CameraPanelState::WaitingTrigger, message);
            } else if (opened) {
                setCameraPanelState(CameraPanelState::Previewing, message);
            } else if (message.contains(tr("断开"))) {
                setCameraPanelState(CameraPanelState::Disconnected, message);
            } else if (message.contains(tr("失败"))) {
                setCameraPanelState(CameraPanelState::Error, message);
            } else {
                setCameraPanelState(
                    camera_indices_.isEmpty() ? CameraPanelState::NoDevice : CameraPanelState::Ready,
                    message);
            }
            preview_fps_label_->setText(opened ? tr("FPS --") : tr("FPS —"));
            statusBar()->showMessage(message, 5000);
            updateCameraControlAvailability();
        });
    connect(camera_worker_, &CameraWorker::operationFinished, this,
        [this](const QString& message, bool success) {
            statusBar()->showMessage(message, 5000);
            camera_feedback_label_->setText(message);
            camera_feedback_label_->setProperty(
                "feedbackState", success ? QStringLiteral("success") : QStringLiteral("error"));
            camera_feedback_label_->style()->unpolish(camera_feedback_label_);
            camera_feedback_label_->style()->polish(camera_feedback_label_);
        });
    connect(camera_worker_, &CameraWorker::configurationFinished, this,
        [this](CameraConfiguration configuration, bool success, const QString& message) {
            camera_configuration_ = configuration;
            updateCameraConfigurationUi(configuration);
            last_sent_exposure_ = configuration.exposure_ms;
            last_sent_rgb_gain_ = {
                configuration.red_gain, configuration.green_gain, configuration.blue_gain};
            last_sent_rgb_offset_ = {
                configuration.red_offset, configuration.green_offset, configuration.blue_offset};
            camera_feedback_label_->setText(message);
            camera_feedback_label_->setProperty(
                "feedbackState", success ? QStringLiteral("success") : QStringLiteral("error"));
            camera_feedback_label_->style()->unpolish(camera_feedback_label_);
            camera_feedback_label_->style()->polish(camera_feedback_label_);
            if (success) saveCameraProfile();
            updateCameraControlAvailability();
            if (camera_roi_selection_pending_) {
                camera_roi_selection_pending_ = false;
                if (success) {
                    QTimer::singleShot(0, this, &CameraMainWindow::startCameraRoiSelection);
                } else {
                    camera_feedback_label_->setText(tr("无法恢复全幅，ROI 框选未开始"));
                }
            }
        });
    connect(camera_worker_, &CameraWorker::exposureApplied, this,
        [this](double value, bool success, const QString& message) {
            if (!FluorescenceCaptureSequence::ConfirmExposure(
                    fluorescence_capture_state_, value, success,
                    latest_camera_sequence_)) return;
            updateFluorescenceCaptureUi();
            if (!success && fluorescence_capture_status_label_) {
                fluorescence_capture_status_label_->setText(
                    tr("当前通道曝光设置失败：%1。请检查相机曝光控制后重试。")
                        .arg(message));
            }
        });
    camera_thread_.start();
}

CameraMainWindow::~CameraMainWindow()
{
    if (camera_thread_.isRunning()) {
        QMetaObject::invokeMethod(camera_worker_, "shutdown", Qt::BlockingQueuedConnection);
        camera_thread_.quit();
        camera_thread_.wait();
    }
}

void CameraMainWindow::setupUi()
{
    setWindowTitle(tr("CameraView · Qt 工业相机与显微测量"));
    resize(1360, 850);
    setMinimumSize(960, 640);

    canvas_ = new ImageCanvas;
    canvas_->setObjectName(QStringLiteral("ImageCanvas"));
    setCentralWidget(canvas_);
    connect(canvas_, &ImageCanvas::pointsCommitted, this, &CameraMainWindow::onCanvasPoints);
    connect(canvas_, &ImageCanvas::toolCancelled, this, [this](CanvasTool tool) {
        if (tool == CanvasTool::CameraRoi) {
            restoreToolAfterCameraRoi(tr("已取消 ROI 框选"));
        }
    });
    connect(canvas_, &ImageCanvas::imagePositionChanged, this, [this](const QPointF& point) {
        coordinate_label_->setText(tr("X %1  Y %2").arg(point.x(), 0, 'f', 1).arg(point.y(), 0, 'f', 1));
    });
    connect(canvas_, &ImageCanvas::zoomChanged, this, [this](double zoom) {
        zoom_label_->setText(tr("缩放 %1%").arg(qRound(zoom * 100.0)));
    });
    connect(canvas_, &ImageCanvas::edgeSnapEvaluated, this,
        [this](bool snapped, const QPointF& original, const QPointF& result, double) {
            if (snapped) {
                statusBar()->showMessage(tr("自动寻边：已从 (%1, %2) 吸附到 (%3, %4)")
                    .arg(original.x(), 0, 'f', 1).arg(original.y(), 0, 'f', 1)
                    .arg(result.x(), 0, 'f', 1).arg(result.y(), 0, 'f', 1), 2500);
            } else {
                statusBar()->showMessage(tr("自动寻边：附近未发现清晰边缘，保留原始落点"), 2500);
            }
        });
    connect(canvas_, &ImageCanvas::overlayPointMoved, this,
        [this](int sourceIndex, int pointIndex, const QPointF& point, bool finished) {
            if (sourceIndex < 0) return;
            const auto reference = measurements_.AtFlatIndex(static_cast<std::size_t>(sourceIndex));
            if (!reference) return;
            EditablePoint editablePoint = EditablePoint::None;
            if (reference->kind == MeasurementKind::Angle) {
                editablePoint = pointIndex == 0 ? EditablePoint::First :
                    (pointIndex == 1 ? EditablePoint::Vertex : EditablePoint::Second);
            } else if (reference->kind == MeasurementKind::Length ||
                reference->kind == MeasurementKind::RectangleArea ||
                reference->kind == MeasurementKind::Circle ||
                reference->kind == MeasurementKind::Ellipse) {
                editablePoint = pointIndex == 0 ? EditablePoint::First : EditablePoint::Second;
            }
            if (measurements_.SetPoint(*reference, editablePoint,
                    static_cast<std::size_t>(std::max(0, pointIndex)), imagePoint(point))) {
                measurement_list_->setCurrentRow(sourceIndex);
                updateMeasurementList();
                if (finished) statusBar()->showMessage(tr("测量点位置已更新"), 2500);
            }
        });

    function_dock_ = new QDockWidget(tr("工作区"), this);
    function_dock_->setObjectName(QStringLiteral("FunctionDock"));
    function_dock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    function_dock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    function_dock_->setMinimumWidth(370);
    function_tabs_ = new QTabWidget;
    function_tabs_->setObjectName(QStringLiteral("FunctionTabs"));
    function_tabs_->setDocumentMode(true);
    function_tabs_->setUsesScrollButtons(true);
    function_tabs_->setElideMode(Qt::ElideRight);
    function_tabs_->addTab(scrollablePanel(buildCameraPage()), tr("相机"));
    function_tabs_->addTab(scrollablePanel(buildImagePage()), tr("图像"));
    function_tabs_->addTab(scrollablePanel(buildFluorescencePage()), tr("荧光"));
    function_tabs_->addTab(scrollablePanel(buildProcessingPage()), tr("处理"));
    function_tabs_->addTab(scrollablePanel(buildMeasurementPage()), tr("测量"));
    yolo_workspace_ = new YoloWorkspaceWidget;
    function_tabs_->addTab(yolo_workspace_, tr("AI"));
    function_tabs_->addTab(scrollablePanel(buildProjectPage()), tr("项目"));
    connect(yolo_workspace_, &YoloWorkspaceWidget::overlaysChanged, this,
        [this](QVector<CanvasOverlay> overlays) {
            ai_overlays_ = std::move(overlays);
            rebuildOverlays();
        });
    connect(yolo_workspace_, &YoloWorkspaceWidget::focusRequested, this,
        [this](const QRectF& image_bounds) { canvas_->focusOnImageRect(image_bounds); });
    connect(yolo_workspace_, &YoloWorkspaceWidget::annotationToolRequested, this,
        [this](CanvasTool tool, const QString& hint) {
            if (tool == CanvasTool::None) {
                ai_annotation_active_ = false;
                canvas_->setEdgeSnappingEnabled(edge_snap_check_ && edge_snap_check_->isChecked());
                canvas_->setTool(CanvasTool::None);
                statusBar()->showMessage(hint, 4000);
                return;
            }
            if (!currentVisibleFrame().IsValid()) {
                ai_annotation_active_ = false;
                yolo_workspace_->cancelPendingDatasetImageOpen();
                QMessageBox::information(this, tr("数据标注"), tr("请先打开图像或连接相机。"));
                return;
            }
            ai_annotation_active_ = true;
            canvas_->setEdgeSnappingEnabled(false);
            canvas_->setTool(tool);
            statusBar()->showMessage(hint);
        });
    connect(yolo_workspace_, &YoloWorkspaceWidget::imageOpenRequested, this,
        [this](const QString& path) {
            if (!loadImageFile(path)) yolo_workspace_->cancelPendingDatasetImageOpen();
        });
    connect(yolo_workspace_, &YoloWorkspaceWidget::statusMessage, this,
        [this](const QString& message) { statusBar()->showMessage(message, 7000); });
    function_dock_->setWidget(function_tabs_);
    addDockWidget(Qt::RightDockWidgetArea, function_dock_);
    resizeDocks({function_dock_}, {410}, Qt::Horizontal);

    for (QFormLayout* form : findChildren<QFormLayout*>()) {
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        form->setHorizontalSpacing(10);
        form->setVerticalSpacing(9);
    }

    source_label_ = new QLabel(tr("无图像"));
    source_label_->setObjectName(QStringLiteral("SourceStatus"));
    coordinate_label_ = new QLabel(tr("X —  Y —"));
    zoom_label_ = new QLabel(tr("缩放 100%"));
    preview_fps_label_ = new QLabel(tr("FPS —"));
    preview_fps_label_->setObjectName(QStringLiteral("PreviewFpsStatus"));
    statusBar()->addWidget(source_label_, 1);
    statusBar()->addPermanentWidget(preview_fps_label_);
    statusBar()->addPermanentWidget(coordinate_label_);
    statusBar()->addPermanentWidget(zoom_label_);
    statusBar()->showMessage(tr("就绪"));

}

void CameraMainWindow::setupMenusAndToolbar()
{
    QMenu* file_menu = menuBar()->addMenu(tr("文件(&F)"));
    QAction* open_action = file_menu->addAction(tr("打开图像…"), QKeySequence::Open, this, &CameraMainWindow::openImage);
    export_action_ = file_menu->addAction(tr("导出当前图像…"), QKeySequence::SaveAs, this, &CameraMainWindow::exportImage);
    file_menu->addSeparator();
    file_menu->addAction(tr("打开项目…"), this, &CameraMainWindow::openProject);
    file_menu->addAction(tr("保存项目…"), QKeySequence::Save, this, &CameraMainWindow::saveProject);
    file_menu->addSeparator();
    QMenu* report_menu = file_menu->addMenu(tr("报告"));
    QAction* image_report_action = report_menu->addAction(
        tr("导出图文报告…"), this, &CameraMainWindow::exportImageReport);
    image_report_action->setObjectName(QStringLiteral("ExportImageReportAction"));
    QAction* diagnostic_report_action = report_menu->addAction(
        tr("导出诊断信息…"), this, &CameraMainWindow::exportDiagnosticReport);
    diagnostic_report_action->setObjectName(QStringLiteral("ExportDiagnosticReportAction"));
    report_menu->addSeparator();
    QAction* design_report_action = report_menu->addAction(
        tr("设计报告模板…"), this, &CameraMainWindow::showReportTemplateDesigner);
    design_report_action->setObjectName(QStringLiteral("DesignReportTemplateAction"));
    QAction* load_report_action = report_menu->addAction(
        tr("载入报告模板…"), this, &CameraMainWindow::loadReportTemplate);
    load_report_action->setObjectName(QStringLiteral("LoadReportTemplateAction"));
    QAction* clear_report_action = report_menu->addAction(
        tr("清除自定义模板"), this, &CameraMainWindow::clearReportTemplate);
    clear_report_action->setObjectName(QStringLiteral("ClearReportTemplateAction"));
    file_menu->addSeparator();
    file_menu->addAction(tr("退出"), QKeySequence::Quit, this, &QWidget::close);

    QMenu* camera_menu = menuBar()->addMenu(tr("相机(&C)"));
    camera_menu->addAction(tr("刷新设备"), this, &CameraMainWindow::refreshDevices);
    camera_menu->addAction(tr("打开相机"), this, &CameraMainWindow::openSelectedCamera);
    camera_menu->addAction(tr("停止相机"), this, &CameraMainWindow::stopCamera);

    QMenu* image_menu = menuBar()->addMenu(tr("图像(&I)"));
    auto transform_frame = [this](const QTransform& transform, const QString& label) {
        const ImageFrame source_frame = currentVisibleFrame();
        if (!source_frame.IsValid()) {
            return;
        }
        if (!measurements_.Empty() && QMessageBox::question(
                this, tr("变换图像"), tr("图像变换会清空当前测量结果，是否继续？")) != QMessageBox::Yes) {
            return;
        }
        measurements_.Clear();
        updateMeasurementList();
        const QImage transformed = qImageFromFrame(source_frame).transformed(transform);
        setCurrentFrame(imageFrameFromQImage(transformed), label);
    };
    image_menu->addAction(tr("水平翻转"), this, [transform_frame] {
        transform_frame(QTransform::fromScale(-1.0, 1.0), QObject::tr("水平翻转结果"));
    });
    image_menu->addAction(tr("垂直翻转"), this, [transform_frame] {
        transform_frame(QTransform::fromScale(1.0, -1.0), QObject::tr("垂直翻转结果"));
    });
    image_menu->addAction(tr("顺时针旋转 90°"), this, [transform_frame] {
        transform_frame(QTransform().rotate(90.0), QObject::tr("顺时针旋转结果"));
    });
    image_menu->addAction(tr("逆时针旋转 90°"), this, [transform_frame] {
        transform_frame(QTransform().rotate(-90.0), QObject::tr("逆时针旋转结果"));
    });
    image_menu->addSeparator();
    QAction* surface_action = image_menu->addAction(tr("3D 高度图…"), this, &CameraMainWindow::show3DView);
    surface_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+3")));
    QAction* profile_action = image_menu->addAction(tr("剖线测量"), this, &CameraMainWindow::startProfileMeasurement);
    profile_action->setShortcut(QKeySequence(Qt::Key_P));

    QMenu* three_d_menu = menuBar()->addMenu(tr("3D(&D)"));
    QAction* point_cloud_action = three_d_menu->addAction(
        tr("3D 点云工作台…"), this, &CameraMainWindow::showPointCloudWorkspace);
    point_cloud_action->setObjectName(QStringLiteral("PointCloudWorkspaceAction"));
    point_cloud_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+3")));
    point_cloud_action->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    three_d_menu->addAction(surface_action);

    QMenu* view_menu = menuBar()->addMenu(tr("视图(&V)"));
    view_menu->addAction(tr("适合窗口"), QKeySequence(Qt::Key_F), canvas_, &ImageCanvas::fitToView);
    view_menu->addSeparator();
    view_menu->addAction(function_dock_->toggleViewAction());

    QMenu* help_menu = menuBar()->addMenu(tr("帮助(&H)"));
    help_menu->addAction(tr("关于"), this, [this] {
        QMessageBox::about(this, tr("关于 CameraView"),
            tr("<h2>CameraView</h2>"
               "<p>基于 Qt 6、OpenCV 与 Ultralytics 的工业相机预览、显微图像处理、测量和 AI 分析软件。</p>"
               "<p><b>作者：</b>栗远<br>"
               "<b>邮箱：</b><a href=\"mailto:liyuan.cn@gmail.com\">liyuan.cn@gmail.com</a></p>"
               "<p>Copyright © 2026 栗远</p>"));
    });

    QToolBar* toolbar = addToolBar(tr("主工具栏"));
    toolbar->setObjectName(QStringLiteral("MainToolbar"));
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(18, 18));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    open_action->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    export_action_->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    export_action_->setEnabled(false);
    toolbar->addAction(open_action);
    toolbar->addAction(export_action_);
    toolbar->addSeparator();
    toolbar->addAction(point_cloud_action);
    toolbar->addSeparator();
    QAction* fit_action = toolbar->addAction(
        style()->standardIcon(QStyle::SP_BrowserReload), tr("适合窗口"), canvas_, &ImageCanvas::fitToView);
    fit_action->setShortcut(QKeySequence(Qt::Key_F));
    toolbar->addSeparator();
    toolbar->addAction(surface_action);

    measurement_toolbar_ = new QToolBar(tr("测量工具栏"), this);
    measurement_toolbar_->setObjectName(QStringLiteral("MeasurementToolbar"));
    measurement_toolbar_->setMovable(false);
    measurement_toolbar_->setIconSize(QSize(30, 30));
    measurement_toolbar_->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    addToolBar(Qt::TopToolBarArea, measurement_toolbar_);
    removeToolBarBreak(measurement_toolbar_);

    auto* measurement_actions = new QActionGroup(measurement_toolbar_);
    measurement_actions->setExclusive(true);
    auto align_toolbar_button = [this](
        QAction* action,
        const QString& role,
        const QString& panelButtonObjectName = {}) {
        if (!action) return;
        if (!panelButtonObjectName.isEmpty()) {
            action->setProperty("measurementPanelButton", panelButtonObjectName);
        }
        if (auto* button = qobject_cast<QToolButton*>(measurement_toolbar_->widgetForAction(action))) {
            button->setProperty("role", role);
            button->setProperty("measurementPanelButton", panelButtonObjectName);
            button->setIconSize(QSize(30, 30));
            button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            button->setMinimumHeight(64);
            button->setAccessibleName(action->text());
            button->setAccessibleDescription(action->toolTip());
        }
    };
    auto add_canvas_action = [this, measurement_actions, align_toolbar_button](
        MeasurementToolGlyph glyph,
        CanvasTool tool,
        const QString& text,
        const QString& status,
        const QString& panelButtonObjectName,
        const QKeySequence& shortcut = {}) {
        QAction* action = measurement_toolbar_->addAction(measurementToolIcon(glyph), text);
        action->setCheckable(true);
        action->setData(static_cast<int>(tool));
        action->setToolTip(status);
        action->setStatusTip(status);
        action->setObjectName(QStringLiteral("MeasurementToolbarTool%1").arg(static_cast<int>(tool)));
        if (!shortcut.isEmpty()) action->setShortcut(shortcut);
        measurement_actions->addAction(action);
        action->setChecked(canvas_->tool() == tool);
        align_toolbar_button(action, QStringLiteral("measurementTool"), panelButtonObjectName);
        connect(action, &QAction::triggered, this, [this, tool, status] {
            if (tool == CanvasTool::None) {
                enterMeasurementSelectionMode();
            } else {
                setMeasurementTool(tool, status);
            }
        });
        return action;
    };
    add_canvas_action(MeasurementToolGlyph::Calibration, CanvasTool::Calibration,
        tr("标定"), tr("在图像上选择标定线的两个端点"), {});
    measurement_toolbar_->addSeparator();
    add_canvas_action(MeasurementToolGlyph::Point, CanvasTool::Point,
        tr("点坐标"), tr("记录图像中一个点的坐标"),
        QStringLiteral("MeasurementPointButton"));
    add_canvas_action(MeasurementToolGlyph::Length, CanvasTool::Length,
        tr("长度"), tr("选择两个端点测量直线长度"),
        QStringLiteral("MeasurementLengthButton"), QKeySequence(Qt::Key_L));
    add_canvas_action(MeasurementToolGlyph::Polyline, CanvasTool::Polyline,
        tr("折线"), tr("依次选择节点，双击完成折线长度测量"),
        QStringLiteral("MeasurementPolylineButton"));
    add_canvas_action(MeasurementToolGlyph::Angle, CanvasTool::Angle,
        tr("角度"), tr("依次选择端点、顶点和端点"),
        QStringLiteral("MeasurementAngleButton"));
    add_canvas_action(MeasurementToolGlyph::Rectangle, CanvasTool::Rectangle,
        tr("矩形"), tr("选择两个对角点测量宽、高、周长和面积"),
        QStringLiteral("MeasurementRectangleButton"));
    add_canvas_action(MeasurementToolGlyph::Polygon, CanvasTool::Polygon,
        tr("多边形"), tr("依次选择顶点，双击完成面积测量"),
        QStringLiteral("MeasurementPolygonButton"));
    add_canvas_action(MeasurementToolGlyph::Circle, CanvasTool::Circle,
        tr("圆"), tr("选择圆心和圆周上一点"),
        QStringLiteral("MeasurementCircleButton"));
    add_canvas_action(MeasurementToolGlyph::Ellipse, CanvasTool::Ellipse,
        tr("椭圆"), tr("选择椭圆外接矩形的两个对角点"),
        QStringLiteral("MeasurementEllipseButton"));
    add_canvas_action(MeasurementToolGlyph::Profile, CanvasTool::ProfileLine,
        tr("剖线"), tr("选择两个端点分析亮度与 RGB 强度曲线"),
        QStringLiteral("MeasurementProfileButton"));
    add_canvas_action(MeasurementToolGlyph::SelectMeasurement, CanvasTool::None,
        tr("选择"), tr("进入选择模式：选择、拖动或编辑测量对象"),
        QStringLiteral("MeasurementSelectionButton"));

    measurement_toolbar_->addSeparator();
    QAction* rename_action = measurement_toolbar_->addAction(
        measurementToolIcon(MeasurementToolGlyph::RenameMeasurement), tr("重命名"));
    rename_action->setToolTip(tr("重命名当前选中的测量结果"));
    align_toolbar_button(rename_action, QStringLiteral("measurementAction"),
        QStringLiteral("MeasurementRenameToolButton"));
    connect(rename_action, &QAction::triggered,
        this, &CameraMainWindow::renameSelectedMeasurement);
    measurement_color_action_ = measurement_toolbar_->addAction(
        measurementToolIcon(MeasurementToolGlyph::MeasurementColor), tr("设置颜色"));
    measurement_color_action_->setToolTip(tr("设置全部测量统一使用的全局颜色"));
    align_toolbar_button(measurement_color_action_, QStringLiteral("measurementAction"),
        QStringLiteral("MeasurementColorToolButton"));
    connect(measurement_color_action_, &QAction::triggered,
        this, &CameraMainWindow::chooseSelectedMeasurementColor);
    QAction* reset_color_action = measurement_toolbar_->addAction(
        measurementToolIcon(MeasurementToolGlyph::ResetMeasurementColor), tr("默认颜色"));
    reset_color_action->setToolTip(tr("恢复系统默认的全局测量颜色"));
    align_toolbar_button(reset_color_action, QStringLiteral("measurementAction"),
        QStringLiteral("MeasurementResetColorToolButton"));
    connect(reset_color_action, &QAction::triggered,
        this, &CameraMainWindow::resetSelectedMeasurementColor);
    QAction* delete_action = measurement_toolbar_->addAction(
        measurementToolIcon(MeasurementToolGlyph::DeleteMeasurement), tr("删除选中"));
    delete_action->setToolTip(tr("删除当前选中的测量结果"));
    align_toolbar_button(delete_action, QStringLiteral("measurementAction"),
        QStringLiteral("MeasurementDeleteToolButton"));
    connect(delete_action, &QAction::triggered,
        this, &CameraMainWindow::deleteSelectedMeasurement);
    QAction* clear_action = measurement_toolbar_->addAction(
        measurementToolIcon(MeasurementToolGlyph::ClearMeasurements), tr("清空测量"));
    clear_action->setToolTip(tr("清空全部测量结果"));
    align_toolbar_button(clear_action, QStringLiteral("measurementAction"),
        QStringLiteral("MeasurementClearToolButton"));
    connect(clear_action, &QAction::triggered, this, &CameraMainWindow::clearMeasurements);
    QAction* export_measurements_action = measurement_toolbar_->addAction(
        measurementToolIcon(MeasurementToolGlyph::ExportCsv), tr("导出 CSV"));
    export_measurements_action->setToolTip(tr("导出全部测量结果为 CSV"));
    align_toolbar_button(export_measurements_action, QStringLiteral("measurementAction"),
        QStringLiteral("MeasurementExportToolButton"));
    connect(export_measurements_action, &QAction::triggered,
        this, &CameraMainWindow::exportMeasurements);

    measurement_toolbar_->addSeparator();
    QAction* smart_sample_action = measurement_toolbar_->addAction(
        measurementToolIcon(MeasurementToolGlyph::SmartCount), tr("智能框选"));
    smart_sample_action->setCheckable(true);
    smart_sample_action->setData(static_cast<int>(CanvasTool::SmartCountSample));
    smart_sample_action->setToolTip(tr("连续框选典型目标作为自动计数样本"));
    measurement_actions->addAction(smart_sample_action);
    align_toolbar_button(smart_sample_action, QStringLiteral("measurementTool"));
    connect(smart_sample_action, &QAction::triggered,
        this, &CameraMainWindow::startSmartTargetSampleSelection);

    connect(canvas_, &ImageCanvas::toolChanged, measurement_toolbar_,
        [measurement_actions](CanvasTool tool) {
            measurement_actions->setExclusive(false);
            for (QAction* action : measurement_actions->actions()) {
                action->setChecked(action->data().toInt() == static_cast<int>(tool));
            }
            measurement_actions->setExclusive(true);
        });

    QAction* smart_run_action = measurement_toolbar_->addAction(
        measurementToolIcon(MeasurementToolGlyph::SmartCountRun), tr("开始计数"));
    smart_run_action->setToolTip(tr("根据已框选样本自动查找并统计目标"));
    align_toolbar_button(smart_run_action, QStringLiteral("measurementAction"));
    connect(smart_run_action, &QAction::triggered, this, &CameraMainWindow::runSmartTargetCounting);

    QAction* edge_snap_action = measurement_toolbar_->addAction(
        measurementToolIcon(MeasurementToolGlyph::EdgeSnap), tr("自动寻边"));
    edge_snap_action->setCheckable(true);
    edge_snap_action->setToolTip(tr("将测量落点吸附到附近最清晰的边缘"));
    align_toolbar_button(edge_snap_action, QStringLiteral("measurementAction"));
    connect(edge_snap_action, &QAction::toggled, edge_snap_check_, &QCheckBox::setChecked);
    connect(edge_snap_check_, &QCheckBox::toggled, edge_snap_action, &QAction::setChecked);
    updateMeasurementStyleUi();
}

QString cameraFrameFormatName(int format)
{
    switch (format) {
    case 0: return QObject::tr("Bayer GR/BG");
    case 1: return QObject::tr("Bayer BG/GR");
    case 2: return QObject::tr("Bayer GB/RG");
    case 3: return QObject::tr("Bayer RG/GB");
    case 4: return QObject::tr("RGB");
    case 5: return QObject::tr("BGR");
    case 6: return QObject::tr("单色");
    default: return QObject::tr("未知");
    }
}

QWidget* addCollapsibleSection(
    QBoxLayout* parentLayout,
    const QString& title,
    bool expanded = true)
{
    auto* container = new QWidget;
    container->setProperty("cameraSection", true);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    auto* header = new QToolButton;
    header->setText(title);
    header->setCheckable(true);
    header->setChecked(expanded);
    header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    header->setProperty("cameraSectionHeader", true);
    auto* body = new QWidget;
    body->setVisible(expanded);
    layout->addWidget(header);
    layout->addWidget(body);
    QObject::connect(header, &QToolButton::toggled, body, &QWidget::setVisible);
    QObject::connect(header, &QToolButton::toggled, header,
        [header](bool checked) {
            header->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
        });
    parentLayout->addWidget(container);
    return body;
}

QWidget* CameraMainWindow::buildCameraPage()
{
    auto* page = new QWidget;
    auto* layout = panelLayout(page);

    auto* status_card = new QFrame;
    status_card->setObjectName(QStringLiteral("CameraStatusCard"));
    status_card->setProperty("cameraStatusCard", true);
    auto* status_layout = new QVBoxLayout(status_card);
    status_layout->setContentsMargins(12, 10, 12, 10);
    status_layout->setSpacing(5);
    auto* status_header = new QHBoxLayout;
    camera_state_badge_ = new QLabel(tr("初始化"));
    camera_state_badge_->setObjectName(QStringLiteral("CameraStateBadge"));
    camera_state_badge_->setProperty("cameraState", QStringLiteral("busy"));
    camera_state_label_ = new QLabel(tr("正在初始化 MUCam SDK…"));
    camera_state_label_->setObjectName(QStringLiteral("CameraStateLabel"));
    camera_state_label_->setWordWrap(true);
    status_header->addWidget(camera_state_badge_);
    status_header->addWidget(camera_state_label_, 1);
    status_layout->addLayout(status_header);
    camera_device_summary_label_ = new QLabel(tr("尚未选择设备"));
    camera_device_summary_label_->setObjectName(QStringLiteral("CameraDeviceSummary"));
    camera_device_summary_label_->setWordWrap(true);
    camera_telemetry_label_ = new QLabel(tr("分辨率 —  ·  FPS —  ·  像素格式 —  ·  8 位"));
    camera_telemetry_label_->setObjectName(QStringLiteral("CameraTelemetry"));
    camera_telemetry_label_->setWordWrap(true);
    status_layout->addWidget(camera_device_summary_label_);
    status_layout->addWidget(camera_telemetry_label_);
    layout->addWidget(status_card);

    QWidget* device_body = addCollapsibleSection(layout, tr("设备与连接"));
    auto* device_layout = new QVBoxLayout(device_body);
    device_layout->setContentsMargins(0, 0, 0, 0);
    device_layout->setSpacing(7);
    device_combo_ = new QComboBox;
    device_combo_->setObjectName(QStringLiteral("CameraDeviceCombo"));
    device_combo_->setPlaceholderText(tr("选择相机设备"));
    device_layout->addWidget(device_combo_);
    camera_refresh_button_ = new QPushButton(tr("刷新设备"));
    camera_connection_button_ = new QPushButton(tr("连接相机"));
    camera_refresh_button_->setObjectName(QStringLiteral("CameraRefreshButton"));
    camera_connection_button_->setObjectName(QStringLiteral("CameraConnectionButton"));
    camera_refresh_button_->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    camera_connection_button_->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    setButtonRole(camera_connection_button_, "primary");
    device_layout->addWidget(buttonRow({camera_refresh_button_, camera_connection_button_}));

    QWidget* basic_body = addCollapsibleSection(layout, tr("基础参数"));
    auto* basic_layout = new QVBoxLayout(basic_body);
    basic_layout->setContentsMargins(0, 0, 0, 0);
    basic_layout->setSpacing(7);
    exposure_spin_ = new QDoubleSpinBox;
    exposure_spin_->setObjectName(QStringLiteral("CameraExposureSpin"));
    exposure_spin_->setDecimals(3);
    exposure_spin_->setRange(0.01, 10000.0);
    exposure_spin_->setValue(10.0);
    exposure_spin_->setSuffix(tr(" ms"));
    camera_exposure_slider_ = new QSlider(Qt::Horizontal);
    camera_exposure_slider_->setObjectName(QStringLiteral("CameraExposureSlider"));
    camera_exposure_slider_->setRange(0, 1000);
    auto* exposure_row = new QWidget;
    auto* exposure_row_layout = new QHBoxLayout(exposure_row);
    exposure_row_layout->setContentsMargins(0, 0, 0, 0);
    exposure_row_layout->addWidget(camera_exposure_slider_, 1);
    exposure_row_layout->addWidget(exposure_spin_);
    basic_layout->addWidget(new QLabel(tr("曝光时间")));
    basic_layout->addWidget(exposure_row);
    gain_spin_ = new QDoubleSpinBox;
    gain_spin_->setObjectName(QStringLiteral("CameraGainSpin"));
    gain_spin_->setDecimals(3);
    gain_spin_->setRange(0.01, 100.0);
    gain_spin_->setValue(1.0);
    camera_gain_slider_ = new QSlider(Qt::Horizontal);
    camera_gain_slider_->setObjectName(QStringLiteral("CameraGainSlider"));
    camera_gain_slider_->setRange(1, 10000);
    camera_gain_slider_->setValue(100);
    auto* gain_row = new QWidget;
    auto* gain_row_layout = new QHBoxLayout(gain_row);
    gain_row_layout->setContentsMargins(0, 0, 0, 0);
    gain_row_layout->addWidget(camera_gain_slider_, 1);
    gain_row_layout->addWidget(gain_spin_);
    basic_layout->addWidget(new QLabel(tr("总增益")));
    basic_layout->addWidget(gain_row);
    camera_auto_exposure_button_ = new QPushButton(tr("自动曝光一次"));
    camera_white_balance_button_ = new QPushButton(tr("白平衡一次"));
    camera_auto_exposure_button_->setObjectName(QStringLiteral("CameraAutoExposureButton"));
    camera_white_balance_button_->setObjectName(QStringLiteral("CameraWhiteBalanceButton"));
    camera_auto_exposure_button_->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    camera_white_balance_button_->setIcon(style()->standardIcon(QStyle::SP_DialogResetButton));
    basic_layout->addWidget(buttonRow({camera_auto_exposure_button_, camera_white_balance_button_}));

    QWidget* color_body = addCollapsibleSection(layout, tr("颜色控制"), false);
    camera_color_group_ = new QGroupBox;
    camera_color_group_->setObjectName(QStringLiteral("CameraColorControls"));
    auto* color_layout = new QGridLayout(camera_color_group_);
    color_layout->setContentsMargins(0, 0, 0, 0);
    camera_link_channels_check_ = new QCheckBox(tr("通道联动"));
    camera_link_channels_check_->setChecked(true);
    color_layout->addWidget(camera_link_channels_check_, 0, 0, 1, 3);
    const QStringList channel_names{tr("R"), tr("G"), tr("B")};
    for (int channel = 0; channel < 3; ++channel) {
        color_layout->addWidget(new QLabel(channel_names[channel]), channel + 1, 0);
        camera_rgb_gain_spins_[channel] = new QDoubleSpinBox;
        camera_rgb_gain_spins_[channel]->setDecimals(3);
        camera_rgb_gain_spins_[channel]->setRange(0.01, 100.0);
        camera_rgb_gain_spins_[channel]->setValue(1.0);
        camera_rgb_gain_spins_[channel]->setPrefix(tr("增益 "));
        camera_rgb_offset_spins_[channel] = new QSpinBox;
        camera_rgb_offset_spins_[channel]->setRange(-255, 255);
        camera_rgb_offset_spins_[channel]->setPrefix(tr("偏移 "));
        color_layout->addWidget(camera_rgb_gain_spins_[channel], channel + 1, 1);
        color_layout->addWidget(camera_rgb_offset_spins_[channel], channel + 1, 2);
    }
    auto* color_outer_layout = new QVBoxLayout(color_body);
    color_outer_layout->setContentsMargins(0, 0, 0, 0);
    color_outer_layout->addWidget(camera_color_group_);

    QWidget* capture_body = addCollapsibleSection(layout, tr("采集参数"), false);
    auto* capture_form = new QFormLayout(capture_body);
    capture_form->setContentsMargins(0, 0, 0, 0);
    camera_resolution_combo_ = new QComboBox;
    camera_resolution_combo_->setObjectName(QStringLiteral("CameraResolutionCombo"));
    camera_trigger_combo_ = new QComboBox;
    camera_trigger_combo_->setObjectName(QStringLiteral("CameraTriggerCombo"));
    camera_trigger_combo_->addItem(tr("自由采集"), static_cast<int>(CameraTriggerMode::Free));
    camera_trigger_combo_->addItem(tr("软件触发"), static_cast<int>(CameraTriggerMode::Software));
    camera_trigger_combo_->addItem(tr("硬件触发（上升沿）"), static_cast<int>(CameraTriggerMode::HardwareRise));
    camera_trigger_combo_->addItem(tr("硬件触发（下降沿）"), static_cast<int>(CameraTriggerMode::HardwareFall));
    camera_flip_check_ = new QCheckBox(tr("垂直翻转"));
    camera_mirror_check_ = new QCheckBox(tr("水平镜像"));
    camera_single_frame_button_ = new QPushButton(tr("采集一帧"));
    camera_single_frame_button_->setObjectName(QStringLiteral("CameraSingleFrameButton"));
    camera_single_frame_button_->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    capture_form->addRow(tr("分辨率 / Binning"), camera_resolution_combo_);
    capture_form->addRow(tr("触发模式"), camera_trigger_combo_);
    capture_form->addRow(camera_flip_check_);
    capture_form->addRow(camera_mirror_check_);
    capture_form->addRow(camera_single_frame_button_);

    QWidget* roi_body = addCollapsibleSection(layout, tr("ROI"), false);
    auto* roi_layout = new QVBoxLayout(roi_body);
    roi_layout->setContentsMargins(0, 0, 0, 0);
    auto* roi_grid = new QGridLayout;
    const QStringList roi_names{tr("X"), tr("Y"), tr("宽"), tr("高")};
    for (int index = 0; index < 4; ++index) {
        camera_roi_spins_[index] = new QSpinBox;
        camera_roi_spins_[index]->setRange(0, 100000);
        camera_roi_spins_[index]->setObjectName(
            QStringLiteral("CameraRoi%1Spin").arg(roi_names[index]));
        roi_grid->addWidget(new QLabel(roi_names[index]), index / 2, (index % 2) * 2);
        roi_grid->addWidget(camera_roi_spins_[index], index / 2, (index % 2) * 2 + 1);
    }
    roi_layout->addLayout(roi_grid);
    camera_roi_select_button_ = new QPushButton(tr("在画面框选 ROI"));
    camera_roi_apply_button_ = new QPushButton(tr("应用数值 ROI"));
    camera_roi_reset_button_ = new QPushButton(tr("恢复全幅"));
    camera_roi_select_button_->setObjectName(QStringLiteral("CameraRoiSelectButton"));
    camera_roi_apply_button_->setObjectName(QStringLiteral("CameraRoiApplyButton"));
    camera_roi_reset_button_->setObjectName(QStringLiteral("CameraRoiResetButton"));
    camera_roi_select_button_->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    camera_roi_apply_button_->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    camera_roi_reset_button_->setIcon(style()->standardIcon(QStyle::SP_DialogResetButton));
    roi_layout->addWidget(camera_roi_select_button_);
    roi_layout->addWidget(buttonRow({camera_roi_apply_button_, camera_roi_reset_button_}));

    camera_feedback_label_ = new QLabel(tr("参数可在连接前预设，连接后自动应用"));
    camera_feedback_label_->setObjectName(QStringLiteral("CameraInlineFeedback"));
    camera_feedback_label_->setWordWrap(true);
    camera_feedback_label_->setProperty("feedbackState", QStringLiteral("info"));
    camera_feedback_label_->style()->unpolish(camera_feedback_label_);
    camera_feedback_label_->style()->polish(camera_feedback_label_);
    layout->addWidget(camera_feedback_label_);

    camera_parameter_timer_ = new QTimer(page);
    camera_parameter_timer_->setSingleShot(true);
    camera_parameter_timer_->setInterval(250);
    connect(camera_parameter_timer_, &QTimer::timeout,
        this, &CameraMainWindow::applyPendingCameraParameters);
    connect(camera_refresh_button_, &QPushButton::clicked, this, &CameraMainWindow::refreshDevices);
    connect(camera_connection_button_, &QPushButton::clicked, this, [this] {
        camera_open_ ? stopCamera() : openSelectedCamera();
    });
    connect(device_combo_, &QComboBox::currentIndexChanged, this, [this](int) {
        if (!camera_open_ && !camera_ui_updating_) loadCameraProfile();
        updateCameraControlAvailability();
    });
    connect(camera_auto_exposure_button_, &QPushButton::clicked, this, [this] {
        camera_feedback_label_->setText(tr("正在执行自动曝光…"));
        QMetaObject::invokeMethod(camera_worker_, "autoExposure", Qt::QueuedConnection);
    });
    connect(camera_white_balance_button_, &QPushButton::clicked, this, [this] {
        camera_feedback_label_->setText(tr("正在执行白平衡…"));
        QMetaObject::invokeMethod(camera_worker_, "whiteBalance", Qt::QueuedConnection);
    });
    connect(camera_single_frame_button_, &QPushButton::clicked, this, [this] {
        QMetaObject::invokeMethod(camera_worker_, "captureOneFrame", Qt::QueuedConnection);
    });
    connect(camera_roi_select_button_, &QPushButton::clicked,
        this, &CameraMainWindow::startCameraRoiSelection);
    connect(camera_roi_apply_button_, &QPushButton::clicked,
        this, &CameraMainWindow::applyCameraRoiInputs);
    connect(camera_roi_reset_button_, &QPushButton::clicked,
        this, &CameraMainWindow::resetCameraRoi);

    connect(exposure_spin_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (camera_ui_updating_) return;
        const double minimum = exposure_spin_->minimum();
        const double span = exposure_spin_->maximum() - minimum;
        const QSignalBlocker blocker(camera_exposure_slider_);
        camera_exposure_slider_->setValue(span > 0.0
            ? qRound((value - minimum) * 1000.0 / span) : 0);
        scheduleCameraParameterApply();
    });
    connect(camera_exposure_slider_, &QSlider::valueChanged, this, [this](int value) {
        if (camera_ui_updating_) return;
        const double target = exposure_spin_->minimum() +
            (exposure_spin_->maximum() - exposure_spin_->minimum()) * value / 1000.0;
        const QSignalBlocker blocker(exposure_spin_);
        exposure_spin_->setValue(target);
        scheduleCameraParameterApply();
    });
    connect(gain_spin_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (camera_ui_updating_) return;
        {
            const QSignalBlocker blocker(camera_gain_slider_);
            camera_gain_slider_->setValue(qRound(value * 100.0));
        }
        if (camera_link_channels_check_->isChecked()) {
            for (QDoubleSpinBox* spin : camera_rgb_gain_spins_) {
                const QSignalBlocker blocker(spin);
                spin->setValue(value);
            }
        }
        scheduleCameraParameterApply();
    });
    connect(camera_gain_slider_, &QSlider::valueChanged, this, [this](int value) {
        if (camera_ui_updating_) return;
        gain_spin_->setValue(value / 100.0);
    });
    for (int channel = 0; channel < 3; ++channel) {
        connect(camera_rgb_gain_spins_[channel], &QDoubleSpinBox::valueChanged,
            this, [this, channel](double value) {
                if (camera_ui_updating_) return;
                if (camera_link_channels_check_->isChecked()) {
                    for (int other = 0; other < 3; ++other) {
                        if (other == channel) continue;
                        const QSignalBlocker blocker(camera_rgb_gain_spins_[other]);
                        camera_rgb_gain_spins_[other]->setValue(value);
                    }
                    const QSignalBlocker blocker(gain_spin_);
                    gain_spin_->setValue(value);
                }
                scheduleCameraParameterApply();
            });
        connect(camera_rgb_offset_spins_[channel], &QSpinBox::valueChanged,
            this, [this](int) {
                if (!camera_ui_updating_) scheduleCameraParameterApply();
            });
    }
    auto structural_change = [this] {
        if (!camera_ui_updating_) applyCameraConfigurationFromUi();
    };
    connect(camera_resolution_combo_, &QComboBox::currentIndexChanged,
        this, [structural_change](int) { structural_change(); });
    connect(camera_trigger_combo_, &QComboBox::currentIndexChanged,
        this, [structural_change](int) { structural_change(); });
    connect(camera_flip_check_, &QCheckBox::toggled,
        this, [structural_change](bool) { structural_change(); });
    connect(camera_mirror_check_, &QCheckBox::toggled,
        this, [structural_change](bool) { structural_change(); });

    setCameraPanelState(CameraPanelState::Initializing, tr("正在初始化 MUCam SDK…"));
    updateCameraControlAvailability();
    layout->addStretch();
    return page;
}

QWidget* CameraMainWindow::buildImagePage()
{
    auto* page = new QWidget;
    auto* layout = panelLayout(page);
    auto* form = new QFormLayout;
    palette_combo_ = new QComboBox;
    for (PseudoColorPalette palette : PseudoColorMapper::PaletteOptions()) {
        palette_combo_->addItem(QString::fromStdWString(PseudoColorMapper::Label(palette)));
    }
    form->addRow(tr("伪彩"), palette_combo_);
    histogram_channel_combo_ = new QComboBox;
    histogram_channel_combo_->addItems({tr("亮度"), tr("红"), tr("绿"), tr("蓝")});
    form->addRow(tr("直方图通道"), histogram_channel_combo_);
    layout->addLayout(form);

    auto make_slider = [this, layout](const QString& text, int minimum, int maximum, int value, QSlider*& member) {
        auto* label = new QLabel(text);
        member = new QSlider(Qt::Horizontal);
        member->setRange(minimum, maximum);
        member->setValue(value);
        layout->addWidget(label);
        layout->addWidget(member);
        connect(member, &QSlider::valueChanged, this, &CameraMainWindow::updateImagePresentation);
    };
    make_slider(tr("亮度"), -100, 100, 0, brightness_slider_);
    make_slider(tr("对比度"), -100, 100, 0, contrast_slider_);
    make_slider(tr("伽马 × 0.1"), 1, 30, 10, gamma_slider_);
    make_slider(tr("窗位"), 0, 255, 128, level_slider_);
    make_slider(tr("窗宽"), 1, 256, 256, width_slider_);

    auto* reset = addButton(layout, tr("重置图像调整"));
    connect(reset, &QPushButton::clicked, this, [this] {
        brightness_slider_->setValue(0);
        contrast_slider_->setValue(0);
        gamma_slider_->setValue(10);
        level_slider_->setValue(128);
        width_slider_->setValue(256);
    });

    auto* filter_group = new QGroupBox(tr("快速图像处理"));
    auto* filter_layout = new QVBoxLayout(filter_group);
    auto* filter_form = new QFormLayout;
    image_filter_combo_ = new QComboBox;
    image_filter_combo_->setObjectName(QStringLiteral("ImageFilterCombo"));
    const std::array<ImageFilterKind, 9> filter_kinds{
        ImageFilterKind::Grayscale,
        ImageFilterKind::Invert,
        ImageFilterKind::AutoContrast,
        ImageFilterKind::HistogramEqualization,
        ImageFilterKind::GaussianBlur,
        ImageFilterKind::MedianDenoise,
        ImageFilterKind::Sharpen,
        ImageFilterKind::EdgeDetection,
        ImageFilterKind::BinaryThreshold};
    for (const ImageFilterKind kind : filter_kinds) {
        image_filter_combo_->addItem(imageFilterName(kind), static_cast<int>(kind));
    }
    image_filter_parameter_slider_ = new NumericSlider;
    image_filter_parameter_slider_->setObjectName(QStringLiteral("ImageFilterParameterSlider"));
    image_filter_parameter_slider_->slider()->setObjectName(
        QStringLiteral("ImageFilterParameterSliderHandle"));
    image_filter_parameter_label_ = new QLabel(tr("参数"));
    filter_form->addRow(tr("处理功能"), image_filter_combo_);
    filter_form->addRow(image_filter_parameter_label_, image_filter_parameter_slider_);
    filter_layout->addLayout(filter_form);
    auto* apply_filter = new QPushButton(tr("添加到处理链"));
    apply_filter->setObjectName(QStringLiteral("ApplyImageFilterButton"));
    setButtonRole(apply_filter, "primary");
    auto* undo_filter = new QPushButton(tr("撤销一步"));
    auto* clear_filters = new QPushButton(tr("恢复原图"));
    filter_layout->addWidget(buttonRow({apply_filter, undo_filter, clear_filters}));
    image_filter_pipeline_label_ = new QLabel;
    image_filter_pipeline_label_->setObjectName(QStringLiteral("ImageFilterPipelineLabel"));
    image_filter_pipeline_label_->setWordWrap(true);
    image_filter_pipeline_label_->setProperty("role", QStringLiteral("summary"));
    filter_layout->addWidget(image_filter_pipeline_label_);
    layout->addWidget(filter_group);

    connect(image_filter_combo_, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &CameraMainWindow::updateImageFilterControls);
    connect(apply_filter, &QPushButton::clicked, this, &CameraMainWindow::applySelectedImageFilter);
    connect(undo_filter, &QPushButton::clicked, this, &CameraMainWindow::undoImageFilter);
    connect(clear_filters, &QPushButton::clicked, this, &CameraMainWindow::clearImageFilters);
    updateImageFilterControls();
    updateImageFilterPipelineUi();

    connect(palette_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &CameraMainWindow::updateImagePresentation);
    connect(histogram_channel_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &CameraMainWindow::updateImagePresentation);
    histogram_ = new HistogramWidget;
    layout->addWidget(histogram_);
    layout->addStretch();
    return page;
}

QWidget* CameraMainWindow::buildFluorescencePage()
{
    auto* page = new QWidget;
    auto* layout = panelLayout(page);
    auto* workflow_hint = new QLabel(tr(
        "按染料依次采集单通道图像。自动拉伸只改变显示范围，不修改原始像素；"
        "曝光提示用于发现欠曝和饱和。"));
    workflow_hint->setWordWrap(true);
    workflow_hint->setProperty("role", QStringLiteral("summary"));
    layout->addWidget(workflow_hint);
    fluorescence_preset_group_ = new QGroupBox(tr("采集预设"));
    fluorescence_preset_group_->setObjectName(QStringLiteral("FluorescencePresetGroup"));
    auto* preset_layout = new QVBoxLayout(fluorescence_preset_group_);
    fluorescence_preset_list_ = new QListWidget;
    fluorescence_preset_list_->setObjectName(QStringLiteral("FluorescencePresetList"));
    fluorescence_preset_list_->setMinimumHeight(100);
    fluorescence_preset_list_->setMaximumHeight(170);
    fluorescence_preset_list_->setToolTip(tr("按列表顺序逐通道采集；每项保存独立曝光与伪彩。"));
    preset_layout->addWidget(fluorescence_preset_list_);
    dye_combo_ = new QComboBox;
    dye_combo_->setObjectName(QStringLiteral("FluorescenceDyeCombo"));
    for (const DyeProfile& dye : dyes_) {
        dye_combo_->addItem(QString::fromStdWString(FluorescenceFormatter::FormatDyeLabel(dye)));
    }
    fluorescence_preset_exposure_spin_ = new QDoubleSpinBox;
    fluorescence_preset_exposure_spin_->setObjectName(
        QStringLiteral("FluorescencePresetExposure"));
    fluorescence_preset_exposure_spin_->setRange(0.01, 10000.0);
    fluorescence_preset_exposure_spin_->setDecimals(2);
    fluorescence_preset_exposure_spin_->setSuffix(tr(" ms"));
    fluorescence_preset_color_button_ = new QPushButton(tr("选择伪彩…"));
    fluorescence_preset_color_button_->setObjectName(
        QStringLiteral("FluorescencePresetColorButton"));
    auto* preset_form = new QFormLayout;
    preset_form->addRow(tr("染料 / 激发-发射"), dye_combo_);
    preset_form->addRow(tr("曝光参数"), fluorescence_preset_exposure_spin_);
    preset_form->addRow(tr("伪彩"), fluorescence_preset_color_button_);
    preset_layout->addLayout(preset_form);
    auto* preset_add = new QPushButton(tr("新增预设"));
    auto* preset_save = new QPushButton(tr("保存修改"));
    auto* preset_delete = new QPushButton(tr("删除预设"));
    preset_add->setObjectName(QStringLiteral("FluorescencePresetAddButton"));
    preset_save->setObjectName(QStringLiteral("FluorescencePresetSaveButton"));
    preset_delete->setObjectName(QStringLiteral("FluorescencePresetDeleteButton"));
    preset_layout->addWidget(buttonRow({preset_add, preset_save, preset_delete}));
    layout->addWidget(fluorescence_preset_group_);

    auto* capture_group = new QGroupBox(tr("逐通道采集"));
    capture_group->setObjectName(QStringLiteral("FluorescenceCaptureGroup"));
    auto* capture_layout = new QVBoxLayout(capture_group);
    fluorescence_capture_status_label_ = new QLabel;
    fluorescence_capture_status_label_->setObjectName(
        QStringLiteral("FluorescenceCaptureStatus"));
    fluorescence_capture_status_label_->setWordWrap(true);
    fluorescence_capture_status_label_->setProperty("role", QStringLiteral("summary"));
    capture_layout->addWidget(fluorescence_capture_status_label_);
    fluorescence_capture_start_button_ = new QPushButton(tr("开始逐通道采集"));
    fluorescence_capture_button_ = new QPushButton(tr("采集当前通道"));
    fluorescence_capture_cancel_button_ = new QPushButton(tr("取消"));
    fluorescence_capture_start_button_->setObjectName(
        QStringLiteral("FluorescenceCaptureStartButton"));
    fluorescence_capture_button_->setObjectName(
        QStringLiteral("FluorescenceCaptureCurrentButton"));
    fluorescence_capture_cancel_button_->setObjectName(
        QStringLiteral("FluorescenceCaptureCancelButton"));
    setButtonRole(fluorescence_capture_start_button_, "primary");
    setButtonRole(fluorescence_capture_cancel_button_, "danger");
    capture_layout->addWidget(buttonRow({
        fluorescence_capture_start_button_, fluorescence_capture_button_,
        fluorescence_capture_cancel_button_}));
    layout->addWidget(capture_group);

    auto* add = addButton(layout, tr("当前帧添加为通道"));
    add->setObjectName(QStringLiteral("FluorescenceAddChannelButton"));
    fusion_check_ = new QCheckBox(tr("显示融合预览"));
    fusion_check_->setObjectName(QStringLiteral("FluorescencePreviewCheck"));
    layout->addWidget(fusion_check_);

    auto* fusion_form = new QFormLayout;
    fluorescence_blend_combo_ = new QComboBox;
    fluorescence_blend_combo_->setObjectName(QStringLiteral("FluorescenceBlendCombo"));
    fluorescence_blend_combo_->addItem(
        tr("加法（高亮共定位）"), static_cast<int>(FluorescenceBlendMode::Additive));
    fluorescence_blend_combo_->addItem(
        tr("屏幕（推荐，减少硬饱和）"), static_cast<int>(FluorescenceBlendMode::Screen));
    fluorescence_blend_combo_->addItem(
        tr("最大值（保留主导通道）"), static_cast<int>(FluorescenceBlendMode::Maximum));
    fluorescence_blend_combo_->setCurrentIndex(1);
    fusion_form->addRow(tr("融合方式"), fluorescence_blend_combo_);
    layout->addLayout(fusion_form);

    channel_list_ = new QListWidget;
    channel_list_->setObjectName(QStringLiteral("FluorescenceChannelList"));
    channel_list_->setToolTip(tr("双击通道可隔离显示；列表显示可见性、尺寸和黑白点。"));
    channel_list_->setMinimumHeight(120);
    channel_list_->setMaximumHeight(210);
    layout->addWidget(channel_list_);
    auto* remove_channel = new QPushButton(tr("删除选中"));
    auto* isolate_channel = new QPushButton(tr("仅显示选中"));
    auto* show_all_channels = new QPushButton(tr("显示全部"));
    auto* clear = new QPushButton(tr("清空"));
    remove_channel->setObjectName(QStringLiteral("FluorescenceRemoveChannelButton"));
    isolate_channel->setObjectName(QStringLiteral("FluorescenceIsolateChannelButton"));
    show_all_channels->setObjectName(QStringLiteral("FluorescenceShowAllButton"));
    clear->setObjectName(QStringLiteral("FluorescenceClearButton"));
    setButtonRole(remove_channel, "danger");
    setButtonRole(clear, "danger");
    layout->addWidget(buttonRow({remove_channel, isolate_channel, show_all_channels, clear}));

    auto* channel_form = new QFormLayout;
    channel_visible_check_ = new QCheckBox(tr("可见"));
    channel_visible_check_->setObjectName(QStringLiteral("FluorescenceChannelVisible"));
    channel_visible_check_->setChecked(true);
    channel_black_slider_ = new NumericSlider;
    channel_black_slider_->setObjectName(QStringLiteral("FluorescenceBlackLevelSlider"));
    channel_black_slider_->slider()->setObjectName(QStringLiteral("FluorescenceBlackLevel"));
    channel_black_slider_->setRange(0, 254);
    channel_white_slider_ = new NumericSlider;
    channel_white_slider_->setObjectName(QStringLiteral("FluorescenceWhiteLevelSlider"));
    channel_white_slider_->slider()->setObjectName(QStringLiteral("FluorescenceWhiteLevel"));
    channel_white_slider_->setRange(1, 255);
    channel_white_slider_->setValue(255);
    channel_form->addRow(tr("通道"), channel_visible_check_);
    channel_form->addRow(tr("黑电平"), channel_black_slider_);
    channel_form->addRow(tr("白电平"), channel_white_slider_);
    layout->addLayout(channel_form);
    auto* apply_channel = new QPushButton(tr("应用显示范围"));
    auto* auto_level = new QPushButton(tr("稳健自动拉伸"));
    apply_channel->setObjectName(QStringLiteral("FluorescenceApplyLevelsButton"));
    auto_level->setObjectName(QStringLiteral("FluorescenceAutoLevelsButton"));
    setButtonRole(auto_level, "primary");
    layout->addWidget(buttonRow({apply_channel, auto_level}));
    fluorescence_statistics_label_ = new QLabel(tr("选择通道后显示强度与曝光质量。"));
    fluorescence_statistics_label_->setObjectName(QStringLiteral("FluorescenceStatisticsLabel"));
    fluorescence_statistics_label_->setWordWrap(true);
    fluorescence_statistics_label_->setProperty("role", QStringLiteral("summary"));
    layout->addWidget(fluorescence_statistics_label_);
    connect(fluorescence_preset_list_, &QListWidget::currentRowChanged,
        this, [this](int) { updateFluorescencePresetEditor(); });
    connect(fluorescence_preset_color_button_, &QPushButton::clicked, this, [this] {
        const QColor current(fluorescence_preset_editor_color_.r,
            fluorescence_preset_editor_color_.g, fluorescence_preset_editor_color_.b);
        const QColor selected = QColorDialog::getColor(
            current, this, tr("选择荧光通道伪彩"), QColorDialog::DontUseNativeDialog);
        if (!selected.isValid()) return;
        fluorescence_preset_editor_color_ = {
            static_cast<unsigned char>(selected.red()),
            static_cast<unsigned char>(selected.green()),
            static_cast<unsigned char>(selected.blue())};
        QPixmap swatch(22, 22);
        swatch.fill(selected);
        fluorescence_preset_color_button_->setIcon(QIcon(swatch));
        fluorescence_preset_color_button_->setIconSize(swatch.size());
        fluorescence_preset_color_button_->setText(
            tr("伪彩 %1").arg(selected.name(QColor::HexRgb).toUpper()));
    });
    connect(preset_add, &QPushButton::clicked, this, [this] {
        const int dye_index = dye_combo_->currentIndex();
        if (dye_index < 0 || dye_index >= static_cast<int>(dyes_.size())) return;
        const DyeProfile& dye = dyes_[static_cast<std::size_t>(dye_index)];
        fluorescence_capture_presets_.push_back({
            dye.name, fluorescence_preset_exposure_spin_->value(),
            fluorescence_preset_editor_color_});
        saveFluorescenceCapturePresets();
        refreshFluorescencePresetList(
            static_cast<int>(fluorescence_capture_presets_.size()) - 1);
    });
    connect(preset_save, &QPushButton::clicked, this, [this] {
        const int row = fluorescence_preset_list_->currentRow();
        const int dye_index = dye_combo_->currentIndex();
        if (row < 0 || row >= static_cast<int>(fluorescence_capture_presets_.size()) ||
            dye_index < 0 || dye_index >= static_cast<int>(dyes_.size())) return;
        FluorescenceCapturePreset& preset =
            fluorescence_capture_presets_[static_cast<std::size_t>(row)];
        preset.dye_name = dyes_[static_cast<std::size_t>(dye_index)].name;
        preset.exposure_ms = fluorescence_preset_exposure_spin_->value();
        preset.color = fluorescence_preset_editor_color_;
        saveFluorescenceCapturePresets();
        refreshFluorescencePresetList(row);
    });
    connect(preset_delete, &QPushButton::clicked, this, [this] {
        const int row = fluorescence_preset_list_->currentRow();
        if (row < 0 || row >= static_cast<int>(fluorescence_capture_presets_.size())) return;
        fluorescence_capture_presets_.erase(fluorescence_capture_presets_.begin() + row);
        saveFluorescenceCapturePresets();
        refreshFluorescencePresetList(std::min(
            row, static_cast<int>(fluorescence_capture_presets_.size()) - 1));
    });
    connect(fluorescence_capture_start_button_, &QPushButton::clicked,
        this, &CameraMainWindow::startFluorescenceCaptureSequence);
    connect(fluorescence_capture_button_, &QPushButton::clicked,
        this, &CameraMainWindow::captureCurrentFluorescencePreset);
    connect(fluorescence_capture_cancel_button_, &QPushButton::clicked,
        this, &CameraMainWindow::cancelFluorescenceCaptureSequence);
    connect(add, &QPushButton::clicked, this, &CameraMainWindow::addFluorescenceChannel);
    connect(clear, &QPushButton::clicked, this, &CameraMainWindow::clearFluorescenceChannels);
    connect(remove_channel, &QPushButton::clicked,
        this, &CameraMainWindow::removeSelectedFluorescenceChannel);
    connect(isolate_channel, &QPushButton::clicked,
        this, &CameraMainWindow::isolateSelectedFluorescenceChannel);
    connect(show_all_channels, &QPushButton::clicked,
        this, &CameraMainWindow::showAllFluorescenceChannels);
    connect(auto_level, &QPushButton::clicked,
        this, &CameraMainWindow::autoLevelSelectedFluorescenceChannel);
    connect(fusion_check_, &QCheckBox::toggled, this, &CameraMainWindow::toggleFusion);
    connect(fluorescence_blend_combo_, qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this] {
            fluorescence_blend_mode_ = static_cast<FluorescenceBlendMode>(
                fluorescence_blend_combo_->currentData().toInt());
            updateImagePresentation();
        });
    connect(canvas_, &ImageCanvas::overlaySelected, this, [this](int sourceIndex) {
        if (!measurement_list_) return;
        if (sourceIndex >= 0 && sourceIndex < measurement_list_->count()) {
            measurement_list_->setCurrentRow(sourceIndex);
        } else {
            measurement_list_->setCurrentRow(-1);
        }
        updateMeasurementStyleUi();
    });
    connect(canvas_, &ImageCanvas::overlayMoved, this,
        [this](int sourceIndex, const QPointF& delta, bool finished) {
            if (sourceIndex < 0) return;
            const auto reference = measurements_.AtFlatIndex(
                static_cast<std::size_t>(sourceIndex));
            if (!reference) return;
            const bool has_delta = !qFuzzyIsNull(delta.x()) || !qFuzzyIsNull(delta.y());
            if (has_delta && !measurements_.Translate(*reference, imagePoint(delta))) return;
            if (measurement_list_ && measurement_list_->currentRow() != sourceIndex) {
                measurement_list_->setCurrentRow(sourceIndex);
            }
            if (finished) {
                updateMeasurementList();
                statusBar()->showMessage(tr("测量覆盖层位置已更新"), 2500);
            }
        });
    connect(channel_list_, &QListWidget::currentRowChanged,
        this, [this](int) { updateFluorescenceChannelUi(); });
    connect(channel_list_, &QListWidget::itemDoubleClicked,
        this, [this](QListWidgetItem*) { isolateSelectedFluorescenceChannel(); });
    connect(apply_channel, &QPushButton::clicked, this, [this] {
        const int row = channel_list_->currentRow();
        const FluorescenceChannelUpdateResult result = FluorescenceChannelUpdater::Apply(
            channels_,
            row,
            channel_visible_check_->isChecked(),
            channel_black_slider_->integerValue(),
            channel_white_slider_->integerValue());
        if (!result.applied) {
            QMessageBox::warning(this, tr("通道设置"), QString::fromStdWString(result.message));
            return;
        }
        refreshFluorescenceChannelList(row);
        updateImagePresentation();
    });
    return page;
}

QWidget* CameraMainWindow::buildProcessingPage()
{
    auto* page = new QWidget;
    auto* layout = panelLayout(page);
    auto* stitch_group = new QGroupBox(tr("图像拼接"));
    auto* stitch_layout = new QVBoxLayout(stitch_group);
    stitch_count_label_ = new QLabel;
    stitch_count_label_->setObjectName(QStringLiteral("StitchCountLabel"));
    stitch_layout->addWidget(stitch_count_label_);

    auto* live_group = new QGroupBox(tr("相机实时拼接"));
    auto* live_layout = new QVBoxLayout(live_group);
    auto* live_form = new QFormLayout;
    live_stitch_interval_slider_ = new NumericSlider;
    live_stitch_interval_slider_->setObjectName(QStringLiteral("LiveStitchIntervalSlider"));
    live_stitch_interval_slider_->slider()->setObjectName(QStringLiteral("LiveStitchIntervalSpin"));
    live_stitch_interval_slider_->setRange(250, 10000);
    live_stitch_interval_slider_->setValue(1200);
    live_stitch_interval_slider_->setSuffix(tr(" ms"));
    live_form->addRow(tr("检测间隔"), live_stitch_interval_slider_);
    live_layout->addLayout(live_form);
    live_stitch_start_button_ = new QPushButton(tr("开始实时拼接"));
    live_stitch_stop_button_ = new QPushButton(tr("停止"));
    live_stitch_start_button_->setObjectName(QStringLiteral("LiveStitchStartButton"));
    live_stitch_stop_button_->setObjectName(QStringLiteral("LiveStitchStopButton"));
    live_stitch_stop_button_->setEnabled(false);
    setButtonRole(live_stitch_start_button_, "primary");
    setButtonRole(live_stitch_stop_button_, "danger");
    live_layout->addWidget(buttonRow({live_stitch_start_button_, live_stitch_stop_button_}));
    live_stitch_status_label_ = new QLabel(tr("打开相机后可自动判断移动距离与重叠区域并采集。"));
    live_stitch_status_label_->setWordWrap(true);
    live_layout->addWidget(live_stitch_status_label_);
    stitch_layout->addWidget(live_group);

    live_stitch_timer_ = new QTimer(this);

    auto* settings_form = new QFormLayout;
    stitch_layout_combo_ = new QComboBox;
    stitch_layout_combo_->setObjectName(QStringLiteral("StitchLayoutCombo"));
    stitch_layout_combo_->addItem(tr("网格（按行排列）"), static_cast<int>(StitchLayoutMode::Grid));
    stitch_layout_combo_->addItem(tr("线性序列"), static_cast<int>(StitchLayoutMode::Linear));
    stitch_rows_spin_ = new QSpinBox;
    stitch_rows_spin_->setRange(1, 50);
    stitch_rows_spin_->setValue(3);
    stitch_cols_spin_ = new QSpinBox;
    stitch_cols_spin_->setRange(1, 50);
    stitch_cols_spin_->setValue(4);
    auto* grid_size = new QWidget;
    auto* grid_size_layout = new QHBoxLayout(grid_size);
    grid_size_layout->setContentsMargins(0, 0, 0, 0);
    grid_size_layout->setSpacing(7);
    grid_size_layout->addWidget(stitch_rows_spin_);
    grid_size_layout->addWidget(new QLabel(QStringLiteral("×")));
    grid_size_layout->addWidget(stitch_cols_spin_);
    stitch_overlap_slider_ = new NumericSlider;
    stitch_overlap_slider_->setObjectName(QStringLiteral("StitchOverlapSlider"));
    stitch_overlap_slider_->slider()->setObjectName(QStringLiteral("StitchOverlapSpin"));
    stitch_overlap_slider_->setRange(
        ProcessingParameterRules::MinStitchOverlapPercent(),
        ProcessingParameterRules::MaxStitchOverlapPercent());
    stitch_overlap_slider_->setValue(ProcessingParameterRules::DefaultStitchOverlapPercent());
    stitch_overlap_slider_->setSuffix(QStringLiteral(" %"));
    stitch_registration_combo_ = new QComboBox;
    stitch_registration_combo_->setObjectName(QStringLiteral("StitchRegistrationCombo"));
    stitch_registration_combo_->addItem(tr("显微图像（推荐）"), static_cast<int>(StitchRegistrationMethod::Micro));
    stitch_registration_combo_->addItem(tr("自动选择"), static_cast<int>(StitchRegistrationMethod::Auto));
    stitch_registration_combo_->addItem(tr("相位相关"), static_cast<int>(StitchRegistrationMethod::Phase));
    stitch_registration_combo_->addItem(tr("ORB 特征"), static_cast<int>(StitchRegistrationMethod::Feature));
    stitch_registration_combo_->addItem(tr("SIFT 特征"), static_cast<int>(StitchRegistrationMethod::Sift));
    stitch_transform_combo_ = new QComboBox;
    stitch_transform_combo_->setObjectName(QStringLiteral("StitchTransformCombo"));
    stitch_transform_combo_->addItem(tr("平移"), static_cast<int>(StitchTransformModel::Translation));
    stitch_transform_combo_->addItem(tr("仿射（推荐）"), static_cast<int>(StitchTransformModel::Affine));
    stitch_transform_combo_->addItem(tr("透视"), static_cast<int>(StitchTransformModel::Homography));
    stitch_transform_combo_->setCurrentIndex(1);
    stitch_blend_combo_ = new QComboBox;
    stitch_blend_combo_->setObjectName(QStringLiteral("StitchBlendCombo"));
    stitch_blend_combo_->addItem(tr("线性羽化"), static_cast<int>(StitchBlendMode::Linear));
    stitch_blend_combo_->addItem(tr("不融合"), static_cast<int>(StitchBlendMode::None));
    settings_form->addRow(tr("排列方式"), stitch_layout_combo_);
    settings_form->addRow(tr("网格行 × 列"), grid_size);
    settings_form->addRow(tr("预计重叠"), stitch_overlap_slider_);
    settings_form->addRow(tr("配准方法"), stitch_registration_combo_);
    settings_form->addRow(tr("变换模型"), stitch_transform_combo_);
    settings_form->addRow(tr("接缝融合"), stitch_blend_combo_);
    stitch_layout->addLayout(settings_form);

    stitch_tile_list_ = new QListWidget;
    stitch_tile_list_->setObjectName(QStringLiteral("StitchTileList"));
    stitch_tile_list_->setMinimumHeight(120);
    stitch_tile_list_->setToolTip(tr("双击可在主画布查看该原始图像"));
    stitch_layout->addWidget(stitch_tile_list_);
    auto* add_tile = addButton(stitch_layout, tr("添加当前帧"));
    auto* import_tiles = addButton(stitch_layout, tr("导入多张图像…"));
    auto* import_directory = addButton(stitch_layout, tr("导入目录…"));
    add_tile->setObjectName(QStringLiteral("StitchAddCurrentButton"));
    import_tiles->setObjectName(QStringLiteral("StitchImportFilesButton"));
    import_directory->setObjectName(QStringLiteral("StitchImportDirectoryButton"));
    auto* tile_actions = new QHBoxLayout;
    auto* move_up = new QPushButton(tr("上移"));
    auto* move_down = new QPushButton(tr("下移"));
    auto* delete_tile = new QPushButton(tr("删除"));
    auto* clear_tiles = new QPushButton(tr("清空"));
    move_up->setObjectName(QStringLiteral("StitchMoveUpButton"));
    move_down->setObjectName(QStringLiteral("StitchMoveDownButton"));
    delete_tile->setObjectName(QStringLiteral("StitchDeleteButton"));
    clear_tiles->setObjectName(QStringLiteral("StitchClearButton"));
    setButtonRole(delete_tile, "danger");
    setButtonRole(clear_tiles, "danger");
    tile_actions->addWidget(move_up);
    tile_actions->addWidget(move_down);
    tile_actions->addWidget(delete_tile);
    tile_actions->addWidget(clear_tiles);
    stitch_layout->addLayout(tile_actions);
    stitch_start_button_ = addButton(stitch_layout, tr("生成拼接图"));
    stitch_start_button_->setObjectName(QStringLiteral("StitchBuildButton"));
    setButtonRole(stitch_start_button_, "primary");
    stitch_progress_ = new QProgressBar;
    stitch_progress_->setObjectName(QStringLiteral("StitchProgress"));
    stitch_progress_->setRange(0, 100);
    stitch_progress_->setValue(0);
    stitch_layout->addWidget(stitch_progress_);
    stitch_cancel_button_ = addButton(stitch_layout, tr("取消拼接"));
    stitch_cancel_button_->setObjectName(QStringLiteral("StitchCancelButton"));
    stitch_cancel_button_->setEnabled(false);
    setButtonRole(stitch_cancel_button_, "danger");
    stitch_retry_button_ = addButton(stitch_layout, tr("重试上次拼接"));
    stitch_retry_button_->setObjectName(QStringLiteral("StitchRetryButton"));
    stitch_retry_button_->setEnabled(false);
    stitch_backend_label_ = new QLabel(tr("后端：OpenCV 优先，缺失时使用内置算法"));
    stitch_backend_label_->setWordWrap(true);
    stitch_layout->addWidget(stitch_backend_label_);
    stitch_save_button_ = addButton(stitch_layout, tr("导出拼接结果…"));
    stitch_save_button_->setObjectName(QStringLiteral("StitchSaveButton"));
    stitch_save_button_->setEnabled(false);
    layout->addWidget(stitch_group);

    auto* edf_group = new QGroupBox(tr("景深扩展 EDF"));
    auto* edf_layout = new QVBoxLayout(edf_group);
    edf_count_label_ = new QLabel;
    edf_layout->addWidget(edf_count_label_);
    auto* add_edf = addButton(edf_layout, tr("添加当前帧"));
    auto* import_edf = addButton(edf_layout, tr("导入焦平面图像…"));
    auto* build_edf = addButton(edf_layout, tr("生成 EDF 图"));
    setButtonRole(build_edf, "primary");
    focus_map_button_ = addButton(edf_layout, tr("显示焦点图"));
    focus_map_button_->setEnabled(false);
    layout->addWidget(edf_group);
    auto* clear = addButton(layout, tr("清空处理队列"));
    setButtonRole(clear, "danger");
    layout->addStretch();

    connect(stitch_layout_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        const bool grid = stitch_layout_combo_->currentData().toInt() == static_cast<int>(StitchLayoutMode::Grid);
        stitch_rows_spin_->setEnabled(grid);
        stitch_cols_spin_->setEnabled(grid);
    });
    connect(live_stitch_start_button_, &QPushButton::clicked, this, &CameraMainWindow::startLiveStitch);
    connect(live_stitch_stop_button_, &QPushButton::clicked, this, [this] { stopLiveStitch(); });
    connect(live_stitch_timer_, &QTimer::timeout, this, &CameraMainWindow::evaluateLiveStitch);
    connect(add_tile, &QPushButton::clicked, this, &CameraMainWindow::addStitchTile);
    connect(import_tiles, &QPushButton::clicked, this, [this] {
        const QStringList files = QFileDialog::getOpenFileNames(
            this, tr("导入拼接图像"), {}, tr("图像 (*.bmp *.png *.jpg *.jpeg *.tif *.tiff)"));
        importStitchFiles(files, tr("所选文件"));
    });
    connect(import_directory, &QPushButton::clicked, this, [this] {
        const QString directory = QFileDialog::getExistingDirectory(this, tr("选择拼接图像目录"));
        if (directory.isEmpty()) return;
        QDir dir(directory);
        QStringList names = dir.entryList(
            {QStringLiteral("*.bmp"), QStringLiteral("*.png"), QStringLiteral("*.jpg"),
             QStringLiteral("*.jpeg"), QStringLiteral("*.tif"), QStringLiteral("*.tiff")},
            QDir::Files, QDir::NoSort);
        names.erase(std::remove_if(names.begin(), names.end(), [](const QString& name) {
            return name.startsWith(QLatin1Char('_'));
        }), names.end());
        QCollator collator;
        collator.setNumericMode(true);
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        std::sort(names.begin(), names.end(), [&collator](const QString& left, const QString& right) {
            return collator.compare(left, right) < 0;
        });
        QStringList files;
        for (const QString& name : names) files.push_back(dir.filePath(name));
        importStitchFiles(files, QDir::toNativeSeparators(directory));
    });
    connect(move_up, &QPushButton::clicked, this, [this] {
        if (busy_ || live_stitch_active_) return;
        const int row = stitch_tile_list_->currentRow();
        if (row <= 0 || row >= static_cast<int>(stitch_tiles_.size())) return;
        std::swap(stitch_tiles_[static_cast<std::size_t>(row)], stitch_tiles_[static_cast<std::size_t>(row - 1)]);
        stitch_tile_sources_.swapItemsAt(row, row - 1);
        invalidateStitchResult();
        refreshStitchTileList(row - 1);
    });
    connect(move_down, &QPushButton::clicked, this, [this] {
        if (busy_ || live_stitch_active_) return;
        const int row = stitch_tile_list_->currentRow();
        if (row < 0 || row + 1 >= static_cast<int>(stitch_tiles_.size())) return;
        std::swap(stitch_tiles_[static_cast<std::size_t>(row)], stitch_tiles_[static_cast<std::size_t>(row + 1)]);
        stitch_tile_sources_.swapItemsAt(row, row + 1);
        invalidateStitchResult();
        refreshStitchTileList(row + 1);
    });
    connect(delete_tile, &QPushButton::clicked, this, &CameraMainWindow::deleteSelectedStitchTile);
    connect(clear_tiles, &QPushButton::clicked, this, [this] {
        if (busy_ || live_stitch_active_) return;
        StitchTileListActions::Clear(stitch_tiles_);
        stitch_tile_sources_.clear();
        invalidateStitchResult();
        refreshStitchTileList();
    });
    connect(stitch_tile_list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        const int row = stitch_tile_list_->currentRow();
        if (row >= 0 && row < static_cast<int>(stitch_tiles_.size())) {
            setCurrentFrame(stitch_tiles_[static_cast<std::size_t>(row)].frame,
                tr("拼接源图 %1").arg(row + 1));
        }
    });
    connect(stitch_start_button_, &QPushButton::clicked, this, &CameraMainWindow::buildStitch);
    connect(stitch_cancel_button_, &QPushButton::clicked, this, [this] {
        if (stitch_cancel_token_) stitch_cancel_token_->store(true);
        stitch_cancel_button_->setEnabled(false);
        statusBar()->showMessage(tr("正在取消拼接…"));
    });
    connect(stitch_retry_button_, &QPushButton::clicked, this, &CameraMainWindow::retryStitch);
    connect(stitch_save_button_, &QPushButton::clicked, this, &CameraMainWindow::saveStitchResult);
    connect(add_edf, &QPushButton::clicked, this, &CameraMainWindow::addEdfFrame);
    connect(import_edf, &QPushButton::clicked, this, [this] {
        const QStringList files = QFileDialog::getOpenFileNames(
            this, tr("导入 EDF 图像"), {}, tr("图像 (*.bmp *.png *.jpg *.jpeg *.tif *.tiff)"));
        for (const QString& file : files) {
            QImageReader reader(file);
            const QImage image = reader.read();
            if (!image.isNull()) {
                edf_stack_.push_back(imageFrameFromQImage(image));
            }
        }
        updateProcessingLabels();
    });
    connect(build_edf, &QPushButton::clicked, this, &CameraMainWindow::buildEdf);
    connect(focus_map_button_, &QPushButton::clicked, this, &CameraMainWindow::showEdfFocusMap);
    connect(clear, &QPushButton::clicked, this, &CameraMainWindow::clearProcessing);
    updateProcessingLabels();
    return page;
}

QWidget* CameraMainWindow::buildMeasurementPage()
{
    auto* page = new QWidget;
    auto* layout = panelLayout(page);
    auto* calibration_group = new QGroupBox(tr("标定"));
    auto* calibration_form = new QFormLayout(calibration_group);
    objective_combo_ = new QComboBox;
    objective_combo_->setObjectName(QStringLiteral("CalibrationObjective"));
    for (const std::wstring& objective : objective_labels_) {
        objective_combo_->addItem(QString::fromStdWString(objective));
    }
    calibration_form->addRow(tr("系统当前物镜"), objective_combo_);
    auto* add_objective = new QPushButton(tr("新增"));
    add_objective->setObjectName(QStringLiteral("CalibrationObjectiveAddButton"));
    auto* edit_objective = new QPushButton(tr("编辑"));
    edit_objective->setObjectName(QStringLiteral("CalibrationObjectiveEditButton"));
    auto* delete_objective = new QPushButton(tr("删除"));
    delete_objective->setObjectName(QStringLiteral("CalibrationObjectiveDeleteButton"));
    calibration_form->addRow(
        tr("标定数据管理"), buttonRow({add_objective, edit_objective, delete_objective}));
    calibration_length_spin_ = new QDoubleSpinBox;
    calibration_length_spin_->setObjectName(QStringLiteral("CalibrationLength"));
    calibration_length_spin_->setDecimals(6);
    calibration_length_spin_->setRange(0.001, 1000000.0);
    calibration_length_spin_->setValue(100.0);
    calibration_unit_combo_ = new QComboBox;
    calibration_unit_combo_->setObjectName(QStringLiteral("CalibrationUnit"));
    calibration_unit_combo_->addItems({tr("µm"), tr("mm")});
    calibration_form->addRow(tr("真实长度"), calibration_length_spin_);
    calibration_form->addRow(tr("单位"), calibration_unit_combo_);
    auto* calibrate = new QPushButton(tr("两点标定"));
    calibrate->setObjectName(QStringLiteral("CalibrationStartButton"));
    calibrate->setIcon(measurementToolIcon(MeasurementToolGlyph::Calibration));
    calibrate->setIconSize(QSize(22, 22));
    auto* clear_calibration = new QPushButton(tr("清除标定"));
    clear_calibration->setObjectName(QStringLiteral("CalibrationClearButton"));
    calibration_form->addRow(buttonRow({calibrate, clear_calibration}));
    calibration_label_ = new QLabel(tr("未标定"));
    calibration_label_->setObjectName(QStringLiteral("CalibrationStatus"));
    calibration_form->addRow(calibration_label_);
    layout->addWidget(calibration_group);

    auto* tool_group = new QGroupBox(tr("测量工具"));
    auto* tool_layout = new QVBoxLayout(tool_group);
    auto* tool_grid = new QGridLayout;
    tool_grid->setContentsMargins(0, 0, 0, 0);
    tool_grid->setHorizontalSpacing(7);
    tool_grid->setVerticalSpacing(7);
    auto* tool_buttons = new QButtonGroup(tool_group);
    tool_buttons->setExclusive(true);
    auto make_tool_button = [tool_group, tool_grid, tool_buttons](
        MeasurementToolGlyph glyph,
        CanvasTool canvasTool,
        const QString& text,
        const QString& toolTip,
        const QString& objectName,
        int row,
        int column) {
        auto* button = new MeasurementToolButton(glyph, text, toolTip, tool_group);
        button->setObjectName(objectName);
        tool_buttons->addButton(button, static_cast<int>(canvasTool));
        tool_grid->addWidget(button, row, column);
        return button;
    };
    auto* point = make_tool_button(MeasurementToolGlyph::Point, CanvasTool::Point,
        tr("点坐标"), tr("记录图像中一个点的坐标"), QStringLiteral("MeasurementPointButton"), 0, 0);
    auto* length = make_tool_button(MeasurementToolGlyph::Length, CanvasTool::Length,
        tr("长度"), tr("选择两个端点测量直线长度"), QStringLiteral("MeasurementLengthButton"), 0, 1);
    auto* polyline = make_tool_button(MeasurementToolGlyph::Polyline, CanvasTool::Polyline,
        tr("折线"), tr("依次选择节点，双击完成折线长度测量"), QStringLiteral("MeasurementPolylineButton"), 0, 2);
    auto* angle = make_tool_button(MeasurementToolGlyph::Angle, CanvasTool::Angle,
        tr("角度"), tr("依次选择端点、顶点和端点"), QStringLiteral("MeasurementAngleButton"), 1, 0);
    auto* rectangle = make_tool_button(MeasurementToolGlyph::Rectangle, CanvasTool::Rectangle,
        tr("矩形"), tr("选择两个对角点测量宽、高、周长和面积"), QStringLiteral("MeasurementRectangleButton"), 1, 1);
    auto* polygon = make_tool_button(MeasurementToolGlyph::Polygon, CanvasTool::Polygon,
        tr("多边形"), tr("依次选择顶点，双击完成面积测量"), QStringLiteral("MeasurementPolygonButton"), 1, 2);
    auto* circle = make_tool_button(MeasurementToolGlyph::Circle, CanvasTool::Circle,
        tr("圆"), tr("选择圆心和圆周上一点"), QStringLiteral("MeasurementCircleButton"), 2, 0);
    auto* ellipse = make_tool_button(MeasurementToolGlyph::Ellipse, CanvasTool::Ellipse,
        tr("椭圆"), tr("选择椭圆外接矩形的两个对角点"), QStringLiteral("MeasurementEllipseButton"), 2, 1);
    auto* profile = make_tool_button(MeasurementToolGlyph::Profile, CanvasTool::ProfileLine,
        tr("剖线"), tr("选择两个端点分析亮度与 RGB 强度曲线"), QStringLiteral("MeasurementProfileButton"), 2, 2);
    auto* select_tool = make_tool_button(
        MeasurementToolGlyph::SelectMeasurement, CanvasTool::None,
        tr("选择"), tr("进入选择模式：选择、拖动或编辑测量对象"),
        QStringLiteral("MeasurementSelectionButton"), 3, 0);
    select_tool->setChecked(canvas_->tool() == CanvasTool::None);
    auto* rename_tool = createMeasurementActionButton(
        MeasurementToolGlyph::RenameMeasurement, tr("重命名"),
        tr("重命名当前选中的测量结果"), tool_group);
    rename_tool->setObjectName(QStringLiteral("MeasurementRenameToolButton"));
    measurement_color_button_ = createMeasurementActionButton(
        MeasurementToolGlyph::MeasurementColor, tr("设置颜色"),
        tr("设置全部测量统一使用的全局颜色"), tool_group);
    measurement_color_button_->setObjectName(QStringLiteral("MeasurementColorToolButton"));
    measurement_reset_color_button_ = createMeasurementActionButton(
        MeasurementToolGlyph::ResetMeasurementColor, tr("默认颜色"),
        tr("恢复系统默认的全局测量颜色"), tool_group);
    measurement_reset_color_button_->setObjectName(
        QStringLiteral("MeasurementResetColorToolButton"));
    auto* delete_tool = createMeasurementActionButton(
        MeasurementToolGlyph::DeleteMeasurement, tr("删除选中"),
        tr("删除当前选中的测量结果"), tool_group);
    delete_tool->setObjectName(QStringLiteral("MeasurementDeleteToolButton"));
    auto* clear_tool = createMeasurementActionButton(
        MeasurementToolGlyph::ClearMeasurements, tr("清空测量"),
        tr("清空全部测量结果"), tool_group);
    clear_tool->setObjectName(QStringLiteral("MeasurementClearToolButton"));
    auto* export_tool = createMeasurementActionButton(
        MeasurementToolGlyph::ExportCsv, tr("导出 CSV"),
        tr("导出全部测量结果为 CSV"), tool_group);
    export_tool->setObjectName(QStringLiteral("MeasurementExportToolButton"));
    tool_grid->addWidget(rename_tool, 3, 1);
    tool_grid->addWidget(measurement_color_button_, 3, 2);
    tool_grid->addWidget(measurement_reset_color_button_, 4, 0);
    tool_grid->addWidget(delete_tool, 4, 1);
    tool_grid->addWidget(clear_tool, 4, 2);
    tool_grid->addWidget(export_tool, 5, 1);
    tool_layout->addLayout(tool_grid);
    display_unit_combo_ = new QComboBox;
    display_unit_combo_->addItems({tr("像素"), tr("µm"), tr("mm")});
    display_unit_combo_->setCurrentIndex(1);
    auto* display_unit_row = new QFormLayout;
    display_unit_row->addRow(tr("显示单位"), display_unit_combo_);
    tool_layout->addLayout(display_unit_row);
    layout->addWidget(tool_group);

    auto* edge_group = new QGroupBox(tr("自动寻边"));
    auto* edge_form = new QFormLayout(edge_group);
    edge_snap_check_ = new QCheckBox(tr("落点吸附到附近最清晰的边缘"));
    edge_snap_radius_slider_ = new NumericSlider;
    edge_snap_radius_slider_->setObjectName(QStringLiteral("EdgeSnapRadiusSlider"));
    edge_snap_radius_slider_->setRange(2, 40);
    edge_snap_radius_slider_->setValue(12);
    edge_snap_radius_slider_->setSuffix(tr(" px"));
    edge_form->addRow(edge_snap_check_);
    edge_form->addRow(tr("搜索半径"), edge_snap_radius_slider_);
    layout->addWidget(edge_group);

    auto* smart_group = new QGroupBox(tr("智能目标计数"));
    auto* smart_layout = new QVBoxLayout(smart_group);
    auto* smart_hint = new QLabel(tr("连续框选几个典型目标，支持圆形、方形、细长形和不规则目标。"));
    smart_hint->setWordWrap(true);
    smart_layout->addWidget(smart_hint);
    smart_select_button_ = new QPushButton(tr("开始框选样本"));
    smart_select_button_->setIcon(measurementToolIcon(MeasurementToolGlyph::SmartCount));
    smart_select_button_->setIconSize(QSize(22, 22));
    smart_select_button_->setObjectName(QStringLiteral("SmartCountSelectButton"));
    auto* remove_smart_sample = new QPushButton(tr("撤销一个"));
    auto* clear_smart = new QPushButton(tr("清除"));
    smart_layout->addWidget(buttonRow({smart_select_button_, remove_smart_sample, clear_smart}));
    auto* smart_form = new QFormLayout;
    smart_similarity_slider_ = new NumericSlider;
    smart_similarity_slider_->setObjectName(QStringLiteral("SmartCountSimilaritySlider"));
    smart_similarity_slider_->setRange(0.40, 0.99);
    smart_similarity_slider_->setSingleStep(0.01);
    smart_similarity_slider_->setDecimals(2);
    smart_similarity_slider_->setValue(0.78);
    smart_scale_tolerance_slider_ = new NumericSlider;
    smart_scale_tolerance_slider_->setObjectName(QStringLiteral("SmartCountScaleToleranceSlider"));
    smart_scale_tolerance_slider_->setRange(0, 40);
    smart_scale_tolerance_slider_->setValue(15);
    smart_scale_tolerance_slider_->setSuffix(tr(" %"));
    smart_form->addRow(tr("相似度阈值"), smart_similarity_slider_);
    smart_form->addRow(tr("尺寸变化范围"), smart_scale_tolerance_slider_);
    smart_layout->addLayout(smart_form);
    smart_sample_label_ = new QLabel(tr("已框选 0 个样本"));
    smart_layout->addWidget(smart_sample_label_);
    smart_count_progress_ = new QProgressBar;
    smart_count_progress_->setRange(0, 100);
    smart_count_progress_->setValue(0);
    smart_count_progress_->setTextVisible(true);
    smart_count_progress_->hide();
    smart_layout->addWidget(smart_count_progress_);
    smart_count_button_ = new QPushButton(tr("自动查找并计数"));
    smart_count_button_->setObjectName(QStringLiteral("SmartCountRunButton"));
    setButtonRole(smart_count_button_, "primary");
    smart_layout->addWidget(smart_count_button_);
    smart_result_label_ = new QLabel(tr("识别结果：尚未运行"));
    smart_result_label_->setObjectName(QStringLiteral("SmartCountResultLabel"));
    smart_result_label_->setProperty("role", QStringLiteral("summary"));
    smart_layout->addWidget(smart_result_label_);
    smart_result_list_ = new QListWidget;
    smart_result_list_->setObjectName(QStringLiteral("SmartCountResultList"));
    smart_result_list_->setAlternatingRowColors(true);
    smart_result_list_->setMaximumHeight(130);
    smart_result_list_->setToolTip(tr("双击结果可在图像中定位"));
    smart_layout->addWidget(smart_result_list_);
    layout->addWidget(smart_group);

    measurement_count_label_ = new QLabel(tr("测量结果（0）"));
    layout->addWidget(measurement_count_label_);
    measurement_list_ = new QListWidget;
    measurement_list_->setAlternatingRowColors(true);
    measurement_list_->setObjectName(QStringLiteral("MeasurementResultList"));
    measurement_list_->setToolTip(tr("单击高亮，双击在图像中定位；F2 重命名，Delete 删除"));
    layout->addWidget(measurement_list_, 1);

    connect(calibrate, &QPushButton::clicked, this, [this] {
        setMeasurementTool(CanvasTool::Calibration, tr("请在图像上选择标定线的两个端点"));
    });
    connect(objective_combo_, qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this](int index) { selectObjective(index); });
    connect(add_objective, &QPushButton::clicked, this, [this] {
        QString label;
        double microns_per_pixel = 0.0;
        if (!editObjectiveCalibrationRecord(
                this, tr("新增物镜标定"), label, microns_per_pixel)) return;
        const auto duplicate = std::find(
            objective_labels_.begin(), objective_labels_.end(), label.toStdWString());
        if (duplicate != objective_labels_.end()) {
            QMessageBox::warning(this, tr("物镜标定"), tr("该物镜倍率已存在。"));
            return;
        }
        objective_labels_.push_back(label.toStdWString());
        objective_calibrations_.push_back(
            CalibrationProfile::FromMicronsPerPixel(microns_per_pixel));
        selected_objective_index_ = static_cast<int>(objective_labels_.size()) - 1;
        calibration_ = objective_calibrations_.back();
        refreshObjectiveControls();
        saveObjectiveCalibrationMemory();
        statusBar()->showMessage(tr("已新增并设为当前物镜：%1").arg(label), 3500);
    });
    connect(edit_objective, &QPushButton::clicked, this, [this] {
        if (selected_objective_index_ < 0 ||
            selected_objective_index_ >= static_cast<int>(objective_labels_.size())) return;
        const std::size_t index = static_cast<std::size_t>(selected_objective_index_);
        QString label = QString::fromStdWString(objective_labels_[index]);
        double microns_per_pixel = objective_calibrations_[index].MicronsPerPixel();
        if (!editObjectiveCalibrationRecord(
                this, tr("编辑物镜标定"), label, microns_per_pixel)) return;
        for (std::size_t candidate = 0; candidate < objective_labels_.size(); ++candidate) {
            if (candidate != index && objective_labels_[candidate] == label.toStdWString()) {
                QMessageBox::warning(this, tr("物镜标定"), tr("该物镜倍率已存在。"));
                return;
            }
        }
        objective_labels_[index] = label.toStdWString();
        objective_calibrations_[index] =
            CalibrationProfile::FromMicronsPerPixel(microns_per_pixel);
        calibration_ = objective_calibrations_[index];
        refreshObjectiveControls();
        saveObjectiveCalibrationMemory();
        statusBar()->showMessage(tr("物镜标定记录已更新：%1").arg(label), 3500);
    });
    connect(delete_objective, &QPushButton::clicked, this, [this] {
        if (objective_labels_.size() <= 1) {
            QMessageBox::warning(this, tr("物镜标定"), tr("系统至少需要保留一个物镜倍率。"));
            return;
        }
        const int index = selected_objective_index_;
        if (index < 0 || index >= static_cast<int>(objective_labels_.size())) return;
        const QString label = QString::fromStdWString(
            objective_labels_[static_cast<std::size_t>(index)]);
        if (QMessageBox::question(
                this, tr("删除物镜标定"),
                tr("确定删除“%1”及其标定数据吗？").arg(label)) != QMessageBox::Yes) return;
        objective_labels_.erase(objective_labels_.begin() + index);
        objective_calibrations_.erase(objective_calibrations_.begin() + index);
        selected_objective_index_ = std::min(
            index, static_cast<int>(objective_labels_.size()) - 1);
        calibration_ = objective_calibrations_[
            static_cast<std::size_t>(selected_objective_index_)];
        refreshObjectiveControls();
        saveObjectiveCalibrationMemory();
        statusBar()->showMessage(tr("物镜标定记录已删除：%1").arg(label), 3500);
    });
    connect(clear_calibration, &QPushButton::clicked, this, [this] {
        calibration_ = CalibrationProfile::Uncalibrated();
        if (selected_objective_index_ >= 0 &&
            selected_objective_index_ < static_cast<int>(objective_calibrations_.size())) {
            objective_calibrations_[static_cast<std::size_t>(selected_objective_index_)] = calibration_;
        }
        updateCalibrationUi();
        updateMeasurementList();
        saveObjectiveCalibrationMemory();
        statusBar()->showMessage(
            tr("%1 物镜标定已清除").arg(currentObjectiveLabel()), 3000);
    });
    connect(length, &QToolButton::clicked, this, [this] { setMeasurementTool(CanvasTool::Length, tr("请选择长度的两个端点")); });
    connect(profile, &QToolButton::clicked, this, &CameraMainWindow::startProfileMeasurement);
    connect(angle, &QToolButton::clicked, this, [this] { setMeasurementTool(CanvasTool::Angle, tr("请选择端点、顶点和端点")); });
    connect(rectangle, &QToolButton::clicked, this, [this] { setMeasurementTool(CanvasTool::Rectangle, tr("请选择矩形的两个对角点")); });
    connect(polygon, &QToolButton::clicked, this, [this] { setMeasurementTool(CanvasTool::Polygon, tr("依次选择顶点，双击完成多边形")); });
    connect(point, &QToolButton::clicked, this, [this] { setMeasurementTool(CanvasTool::Point, tr("请选择需要记录坐标的点")); });
    connect(polyline, &QToolButton::clicked, this, [this] { setMeasurementTool(CanvasTool::Polyline, tr("依次选择折线节点，双击完成")); });
    connect(circle, &QToolButton::clicked, this, [this] { setMeasurementTool(CanvasTool::Circle, tr("请选择圆心和圆周上的一点")); });
    connect(ellipse, &QToolButton::clicked, this, [this] { setMeasurementTool(CanvasTool::Ellipse, tr("请选择椭圆外接矩形的两个对角点")); });
    connect(select_tool, &QToolButton::clicked,
        this, &CameraMainWindow::enterMeasurementSelectionMode);
    connect(canvas_, &ImageCanvas::toolChanged, page, [tool_buttons](CanvasTool tool) {
        tool_buttons->setExclusive(false);
        for (QAbstractButton* button : tool_buttons->buttons()) {
            button->setChecked(tool_buttons->id(button) == static_cast<int>(tool));
        }
        tool_buttons->setExclusive(true);
    });
    connect(edge_snap_check_, &QCheckBox::toggled, this, [this](bool enabled) {
        canvas_->setEdgeSnappingEnabled(enabled);
        statusBar()->showMessage(enabled ? tr("自动寻边已开启") : tr("自动寻边已关闭"), 2500);
    });
    connect(edge_snap_radius_slider_, &NumericSlider::valueChanged,
        this, [this](double value) { canvas_->setEdgeSnapRadius(qRound(value)); });
    connect(smart_select_button_, &QPushButton::clicked,
        this, &CameraMainWindow::startSmartTargetSampleSelection);
    connect(remove_smart_sample, &QPushButton::clicked, this, [this] {
        if (smart_count_running_) {
            statusBar()->showMessage(tr("请先取消当前智能计数，再修改样本"), 3000);
            return;
        }
        if (!smart_target_samples_.empty()) {
            smart_target_samples_.pop_back();
            smart_target_result_ = {};
            updateSmartTargetUi();
            rebuildOverlays();
        }
    });
    connect(clear_smart, &QPushButton::clicked, this, &CameraMainWindow::clearSmartTargetCounting);
    connect(smart_count_button_, &QPushButton::clicked, this, &CameraMainWindow::runSmartTargetCounting);
    connect(smart_result_list_, &QListWidget::currentRowChanged, this, [this](int) { rebuildOverlays(); });
    connect(smart_result_list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        const int row = smart_result_list_->currentRow();
        if (row < 0 || row >= static_cast<int>(smart_target_result_.matches.size())) return;
        const SmartTargetRegion& region = smart_target_result_.matches[static_cast<std::size_t>(row)].region;
        canvas_->focusOnImageRect(QRectF(region.x, region.y, region.width, region.height));
        statusBar()->showMessage(tr("已定位到智能计数目标 %1").arg(row + 1), 2500);
    });
    connect(display_unit_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        display_unit_ = index == 0 ? MeasurementUnit::Pixels :
            (index == 1 ? MeasurementUnit::Micrometers : MeasurementUnit::Millimeters);
        updateMeasurementList();
    });
    connect(rename_tool, &QToolButton::clicked,
        this, &CameraMainWindow::renameSelectedMeasurement);
    connect(measurement_list_, &QListWidget::currentRowChanged, this,
        [this](int) {
            rebuildOverlays();
            updateMeasurementStyleUi();
        });
    connect(measurement_list_, &QListWidget::itemDoubleClicked, this,
        [this](QListWidgetItem*) { focusSelectedMeasurement(); });
    auto* rename_shortcut = new QShortcut(QKeySequence(Qt::Key_F2), measurement_list_);
    connect(rename_shortcut, &QShortcut::activated,
        this, &CameraMainWindow::renameSelectedMeasurement);
    auto* delete_shortcut = new QShortcut(QKeySequence::Delete, measurement_list_);
    connect(delete_shortcut, &QShortcut::activated, this, &CameraMainWindow::deleteSelectedMeasurement);
    connect(delete_tool, &QToolButton::clicked, this, &CameraMainWindow::deleteSelectedMeasurement);
    connect(clear_tool, &QToolButton::clicked, this, &CameraMainWindow::clearMeasurements);
    connect(export_tool, &QToolButton::clicked, this, &CameraMainWindow::exportMeasurements);
    connect(measurement_color_button_, &QToolButton::clicked,
        this, &CameraMainWindow::chooseSelectedMeasurementColor);
    connect(measurement_reset_color_button_, &QToolButton::clicked,
        this, &CameraMainWindow::resetSelectedMeasurementColor);
    updateMeasurementStyleUi();
    updateSmartTargetUi();
    return page;
}

QWidget* CameraMainWindow::buildProjectPage()
{
    auto* page = new QWidget;
    auto* layout = panelLayout(page);
    auto* project_group = new QGroupBox(tr("项目"));
    auto* project_layout = new QVBoxLayout(project_group);
    auto* open = addButton(layout, tr("打开项目…"));
    auto* save = addButton(layout, tr("保存项目…"));
    layout->removeWidget(open);
    layout->removeWidget(save);
    project_layout->addWidget(open);
    project_layout->addWidget(save);
    setButtonRole(save, "primary");
    auto* project_hint = new QLabel(tr("项目文件保存标定、测量、染料、荧光通道配置以及处理参数。图像数据请单独导出。"));
    project_hint->setWordWrap(true);
    project_layout->addWidget(project_hint);
    layout->addWidget(project_group);

    auto* report_group = new QGroupBox(tr("报告"));
    auto* report_layout = new QVBoxLayout(report_group);
    auto* report_export = new QPushButton(tr("导出图文报告…"));
    auto* report_design = new QPushButton(tr("设计报告模板…"));
    auto* report_load = new QPushButton(tr("载入报告模板…"));
    auto* report_clear = new QPushButton(tr("清除自定义模板"));
    report_export->setObjectName(QStringLiteral("ProjectExportImageReportButton"));
    report_design->setObjectName(QStringLiteral("ProjectDesignReportTemplateButton"));
    report_load->setObjectName(QStringLiteral("ProjectLoadReportTemplateButton"));
    report_clear->setObjectName(QStringLiteral("ProjectClearReportTemplateButton"));
    setButtonRole(report_export, "primary");
    report_layout->addWidget(report_export);
    report_layout->addWidget(report_design);
    auto* template_row = new QHBoxLayout;
    template_row->addWidget(report_load);
    template_row->addWidget(report_clear);
    report_layout->addLayout(template_row);
    auto* report_hint = new QLabel(tr("图文报告包含当前图像、标定信息和测量结果；模板设置会自动记忆。"));
    report_hint->setWordWrap(true);
    report_layout->addWidget(report_hint);
    layout->addWidget(report_group);
    layout->addStretch();
    connect(open, &QPushButton::clicked, this, &CameraMainWindow::openProject);
    connect(save, &QPushButton::clicked, this, &CameraMainWindow::saveProject);
    connect(report_export, &QPushButton::clicked, this, &CameraMainWindow::exportImageReport);
    connect(report_design, &QPushButton::clicked, this, &CameraMainWindow::showReportTemplateDesigner);
    connect(report_load, &QPushButton::clicked, this, &CameraMainWindow::loadReportTemplate);
    connect(report_clear, &QPushButton::clicked, this, &CameraMainWindow::clearReportTemplate);
    return page;
}

void CameraMainWindow::openImage()
{
    const QString file_name = QFileDialog::getOpenFileName(
        this,
        tr("打开图像"),
        {},
        tr("图像 (*.bmp *.png *.jpg *.jpeg *.tif *.tiff);;所有文件 (*)"));
    if (!file_name.isEmpty()) {
        loadImageFile(file_name);
    }
}

bool CameraMainWindow::loadImageFile(const QString& fileName)
{
    QImageReader reader(fileName);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::warning(this, tr("打开失败"), reader.errorString());
        return false;
    }
    setCurrentFrame(
        imageFrameFromQImage(image), QFileInfo(fileName).fileName(), QFileInfo(fileName).absoluteFilePath());
    statusBar()->showMessage(tr("已打开 %1").arg(fileName), 5000);
    return true;
}

void CameraMainWindow::exportImage()
{
    const ImageFrame frame = currentVisibleFrame();
    if (!frame.IsValid()) {
        QMessageBox::information(this, tr("导出图像"), tr("当前没有可导出的图像。"));
        return;
    }
    QString file_name = QFileDialog::getSaveFileName(
        this,
        tr("导出当前图像"),
        QStringLiteral("CameraView.png"),
        tr("PNG (*.png);;JPEG (*.jpg *.jpeg);;TIFF (*.tif *.tiff);;BMP (*.bmp)"));
    if (file_name.isEmpty()) {
        return;
    }
    if (QFileInfo(file_name).suffix().isEmpty()) {
        file_name += QStringLiteral(".png");
    }
    QImage output = qImageFromFrame(frame).convertToFormat(QImage::Format_ARGB32);
    QPainter painter(&output);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QFont label_font = painter.font();
    label_font.setPixelSize(std::max(12, output.width() / 90));
    painter.setFont(label_font);
    const double pen_width = std::max(2.0, output.width() / 800.0);
    QVector<CanvasOverlay> export_overlays = measurementOverlays();
    export_overlays += smartTargetOverlays();
    export_overlays += ai_overlays_;
    for (const CanvasOverlay& overlay : export_overlays) {
        if (overlay.points.isEmpty()) {
            continue;
        }
        painter.setPen(QPen(overlay.color, pen_width));
        painter.setBrush(Qt::NoBrush);
        if ((overlay.kind == CanvasTool::Rectangle || overlay.kind == CanvasTool::Ellipse ||
            overlay.kind == CanvasTool::SmartCountSample || overlay.kind == CanvasTool::SmartCountResult) &&
            overlay.points.size() >= 2) {
            const QRectF bounds = QRectF(overlay.points[0], overlay.points[1]).normalized();
            if (overlay.kind == CanvasTool::Ellipse) painter.drawEllipse(bounds);
            else painter.drawRect(bounds);
        } else if (overlay.kind == CanvasTool::Circle && overlay.points.size() >= 2) {
            const double radius = QLineF(overlay.points[0], overlay.points[1]).length();
            painter.drawEllipse(overlay.points[0], radius, radius);
            painter.drawLine(overlay.points[0], overlay.points[1]);
        } else if (overlay.kind == CanvasTool::Point) {
            const double arm = pen_width * 4.0;
            painter.drawLine(overlay.points[0] + QPointF(-arm, 0.0), overlay.points[0] + QPointF(arm, 0.0));
            painter.drawLine(overlay.points[0] + QPointF(0.0, -arm), overlay.points[0] + QPointF(0.0, arm));
        } else if (overlay.kind == CanvasTool::Polygon && overlay.points.size() >= 3) {
            painter.drawPolygon(QPolygonF(overlay.points));
        } else {
            for (int index = 1; index < overlay.points.size(); ++index) {
                painter.drawLine(overlay.points[index - 1], overlay.points[index]);
            }
        }
        painter.setBrush(overlay.color);
        for (const QPointF& point : overlay.points) {
            painter.drawEllipse(point, pen_width * 2.0, pen_width * 2.0);
        }
        painter.drawText(overlay.points.last() + QPointF(8.0, -8.0), overlay.label);
    }
    painter.end();
    QImageWriter writer(file_name);
    writer.setQuality(95);
    if (!writer.write(output)) {
        QMessageBox::warning(this, tr("导出失败"), writer.errorString());
        return;
    }
    statusBar()->showMessage(tr("图像已导出：%1").arg(file_name), 5000);
}

DiagnosticReportActionInput CameraMainWindow::buildDiagnosticReportInput() const
{
    const QDateTime now = QDateTime::currentDateTime();
    const QDate date = now.date();
    const QTime time = now.time();
    DiagnosticReportActionInput input;
    input.generated = DiagnosticReportTimestamp{
        date.year(), date.month(), date.day(), time.hour(), time.minute(), time.second()};
    input.status = statusBar()->currentMessage().toStdWString();
    input.preview_telemetry = preview_fps_label_ ? preview_fps_label_->text().toStdWString() : L"";
    input.viewport_zoom = canvas_
        ? QStringLiteral("%1%").arg(canvas_->zoom() * 100.0, 0, 'f', 0).toStdWString()
        : L"";
    input.preview_running = camera_open_;
    input.processing_running = busy_;
    input.sdk.loaded = camera_open_ || !camera_indices_.isEmpty();
    input.sdk.last_error = camera_state_label_ ? camera_state_label_->text().toStdWString() : L"";
    input.enumerated_cameras = camera_indices_.size();
    input.selected_camera_index = device_combo_ ? device_combo_->currentIndex() : -1;
    if (device_combo_) {
        for (int row = 0; row < device_combo_->count(); ++row) {
            CameraDevice device;
            device.index = row < camera_indices_.size() ? camera_indices_.at(row) : row;
            device.display_name = device_combo_->itemText(row).toStdWString();
            input.enumerated_devices.push_back(std::move(device));
        }
    }
    input.latest_frame_source = current_source_.toStdWString();
    input.latest_frame = currentVisibleFrame();
    input.objective_label = currentObjectiveLabel().toStdWString();
    input.calibration = calibration_;
    input.display_unit = display_unit_;
    input.preview_display_mode = fusion_enabled_
        ? L"Fluorescence fusion"
        : PseudoColorMapper::Label(palette_);
    input.pseudo_color = palette_;
    input.dye_profiles = dyes_.size();
    input.fluorescence_channels = channels_.size();
    input.stitch_tiles = stitch_tiles_.size();
    input.stitch_search_percent = stitch_overlap_slider_ ? stitch_overlap_slider_->value() : 0;
    if (stitch_result_metadata_.available) {
        input.stitch_result_backend = stitch_result_metadata_.backend;
        input.stitch_result_layout = stitch_result_metadata_.layout_mode;
        input.stitch_result_registration = stitch_result_metadata_.registration_method;
        input.stitch_result_transform = stitch_result_metadata_.transform_model;
        input.stitch_result_blend = stitch_result_metadata_.blend_mode;
        input.stitch_result_overlap_percent = stitch_result_metadata_.overlap_percent;
        input.stitch_result_tiles = stitch_result_metadata_.tiles.size();
        input.stitch_result_relations = stitch_result_metadata_.relation_count;
        QStringList positions;
        for (int index = 0; index < static_cast<int>(stitch_result_metadata_.tiles.size()); ++index) {
            const StitchResultTileMetadata& tile = stitch_result_metadata_.tiles[static_cast<std::size_t>(index)];
            positions.push_back(tr("%1: %2x%3 @ %4,%5%6")
                .arg(index + 1).arg(tile.width).arg(tile.height)
                .arg(tile.offset_x).arg(tile.offset_y)
                .arg(tile.estimated_position ? tr("（估计）") : QString{}));
        }
        input.stitch_result_tile_positions = positions.join(QStringLiteral("; ")).toStdWString();
    }
    input.edf_frames = edf_stack_.size();
    input.edf_focus_radius = 1;
    input.edf_composite_available = edf_result_.composite_frame.IsValid();
    input.edf_focus_map_available = edf_result_.focus_map.IsValid();
    if (stitch_result_.IsValid()) {
        input.processing_result_visible = true;
        input.processing_result_kind = L"Stitch";
        input.processing_result_source = L"Stitch result";
        input.processing_result_width = stitch_result_.width;
        input.processing_result_height = stitch_result_.height;
    } else if (edf_result_.composite_frame.IsValid()) {
        input.processing_result_visible = true;
        input.processing_result_kind = L"EDF";
        input.processing_result_source = L"EDF composite";
        input.processing_result_width = edf_result_.composite_frame.width;
        input.processing_result_height = edf_result_.composite_frame.height;
    }
    return input;
}

void CameraMainWindow::exportImageReport()
{
    const ImageFrame report_frame = currentVisibleFrame();
    if (!report_frame.IsValid()) {
        QMessageBox::information(this, tr("导出图文报告"), tr("请先打开图像或启动相机预览。"));
        return;
    }

    QString file_name = QFileDialog::getSaveFileName(
        this, tr("导出图文报告"), QStringLiteral("CameraView-report.html"),
        tr("HTML 报告 (*.html)"));
    if (file_name.isEmpty()) {
        return;
    }
    if (QFileInfo(file_name).suffix().isEmpty()) {
        file_name += QStringLiteral(".html");
    }

    const QFileInfo report_info(file_name);
    const QString stem = report_info.completeBaseName().isEmpty()
        ? QStringLiteral("CameraView-report")
        : report_info.completeBaseName();
    const QString image_name = stem + QStringLiteral("_image.png");
    const QString image_file = report_info.dir().filePath(image_name);
    const std::wstring display_mode = fusion_enabled_
        ? L"Fluorescence fusion"
        : PseudoColorMapper::Label(palette_);
    const ExportActionResult image_result = ExportActions::SaveImage(
        std::filesystem::path(image_file.toStdWString()), report_frame,
        measurements_, display_mode, &calibration_);
    if (!image_result.saved) {
        QMessageBox::warning(this, tr("报告图像导出失败"),
            QString::fromStdWString(image_result.message));
        return;
    }

    const std::wstring report = DiagnosticReportActions::BuildImageReport(
        buildDiagnosticReportInput(), measurements_, image_name.toStdWString(),
        report_frame, report_template_text_);
    const ExportActionResult report_result = ExportActions::SaveReportHtml(
        std::filesystem::path(file_name.toStdWString()), report);
    if (!report_result.saved) {
        QMessageBox::warning(this, tr("报告导出失败"),
            QString::fromStdWString(report_result.message));
        return;
    }
    statusBar()->showMessage(
        tr("图文报告已导出：%1（报告图像：%2）").arg(file_name, image_name), 7000);
}

void CameraMainWindow::exportDiagnosticReport()
{
    QString file_name = QFileDialog::getSaveFileName(
        this, tr("导出诊断信息"), QStringLiteral("CameraView-diagnostics.txt"),
        tr("文本文件 (*.txt)"));
    if (file_name.isEmpty()) {
        return;
    }
    if (QFileInfo(file_name).suffix().isEmpty()) {
        file_name += QStringLiteral(".txt");
    }
    const ExportActionResult result = ExportActions::SaveDiagnosticReport(
        std::filesystem::path(file_name.toStdWString()),
        DiagnosticReportActions::BuildReport(buildDiagnosticReportInput(), measurements_));
    if (!result.saved) {
        QMessageBox::warning(this, tr("诊断信息导出失败"),
            QString::fromStdWString(result.message));
        return;
    }
    statusBar()->showMessage(tr("诊断信息已导出：%1").arg(file_name), 5000);
}

void CameraMainWindow::showReportTemplateDesigner()
{
    ImageReportTemplateOptions parsed;
    const bool visual_template = report_template_text_.empty() ||
        DiagnosticReportActions::TryParseImageReportTemplateOptions(report_template_text_, parsed);
    if (!visual_template && QMessageBox::question(
            this, tr("替换自定义模板"),
            tr("当前载入的是手写 HTML 模板，进入可视化设计并应用后会替换它。是否继续？"))
            != QMessageBox::Yes) {
        return;
    }
    if (visual_template && !report_template_text_.empty()) {
        report_template_options_ = parsed;
    }
    ReportTemplateDialog dialog(report_template_options_, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    report_template_options_ = dialog.options();
    report_template_text_ = DiagnosticReportActions::BuildImageReportTemplate(report_template_options_);
    report_template_path_.clear();
    saveReportTemplateSettings();
    statusBar()->showMessage(tr("报告模板已应用并记忆"), 5000);
}

void CameraMainWindow::loadReportTemplate()
{
    const QString file_name = QFileDialog::getOpenFileName(
        this, tr("载入报告模板"), report_template_path_,
        tr("HTML 模板 (*.html *.htm);;所有文件 (*.*)"));
    if (file_name.isEmpty()) {
        return;
    }
    QFile file(file_name);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("载入报告模板失败"), file.errorString());
        return;
    }
    const QByteArray bytes = file.readAll();
    QString text = QString::fromUtf8(bytes);
    if (!text.isEmpty() && text.front() == QChar::ByteOrderMark) {
        text.remove(0, 1);
    }
    if (text.trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("载入报告模板失败"), tr("模板文件为空。"));
        return;
    }
    report_template_text_ = text.toStdWString();
    report_template_path_ = file_name;
    ImageReportTemplateOptions parsed;
    const bool visual = DiagnosticReportActions::TryParseImageReportTemplateOptions(
        report_template_text_, parsed);
    report_template_options_ = visual ? parsed : ImageReportTemplateOptions{};
    saveReportTemplateSettings();
    statusBar()->showMessage(visual
        ? tr("报告模板已载入，可在设计器中继续编辑：%1").arg(QFileInfo(file_name).fileName())
        : tr("自定义 HTML 报告模板已载入：%1").arg(QFileInfo(file_name).fileName()), 6000);
}

void CameraMainWindow::clearReportTemplate()
{
    if (report_template_text_.empty()) {
        statusBar()->showMessage(tr("当前已经使用默认报告模板"), 3000);
        return;
    }
    report_template_text_.clear();
    report_template_path_.clear();
    report_template_options_ = ImageReportTemplateOptions{};
    saveReportTemplateSettings();
    statusBar()->showMessage(tr("已恢复默认报告模板"), 4000);
}

void CameraMainWindow::loadReportTemplateSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("Report"));
    report_template_text_ = settings.value(QStringLiteral("templateText")).toString().toStdWString();
    report_template_path_ = settings.value(QStringLiteral("templatePath")).toString();
    settings.endGroup();
    ImageReportTemplateOptions parsed;
    if (DiagnosticReportActions::TryParseImageReportTemplateOptions(report_template_text_, parsed)) {
        report_template_options_ = std::move(parsed);
    }
}

void CameraMainWindow::saveReportTemplateSettings() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("Report"));
    settings.setValue(QStringLiteral("templateText"), QString::fromStdWString(report_template_text_));
    settings.setValue(QStringLiteral("templatePath"), report_template_path_);
    settings.endGroup();
}

void CameraMainWindow::saveProject()
{
    QString file_name = QFileDialog::getSaveFileName(
        this, tr("保存项目"), QStringLiteral("CameraView.cvproject"), tr("CameraView 项目 (*.cvproject);;JSON (*.json)"));
    if (file_name.isEmpty()) {
        return;
    }
    if (QFileInfo(file_name).suffix().isEmpty()) {
        file_name += QStringLiteral(".cvproject");
    }
    const StitchProcessingOptions stitch_options = stitchOptionsFromUi();
    const int stitch_search_percent =
        ProcessingParameterRules::SearchPercentFromOverlap(stitch_options.overlap_percent);
    ProjectDocument document = ProjectSessionMapper::ToDocument(
        calibration_, measurements_, dyes_, channels_, EdfOptions{}, stitch_search_percent,
        objective_labels_, objective_calibrations_, selected_objective_index_,
        stitch_options.registration_method != StitchRegistrationMethod::Phase);
    document.processing_settings.stitch_overlap_percent = stitch_options.overlap_percent;
    document.processing_settings.stitch_layout_mode = static_cast<int>(stitch_options.layout_mode);
    document.processing_settings.stitch_grid_rows = stitch_options.grid_rows;
    document.processing_settings.stitch_grid_cols = stitch_options.grid_cols;
    document.processing_settings.stitch_registration_method =
        static_cast<int>(stitch_options.registration_method);
    document.processing_settings.stitch_transform_model =
        static_cast<int>(stitch_options.transform_model);
    document.processing_settings.stitch_blend_mode = static_cast<int>(stitch_options.blend_mode);
    document.processing_settings.live_stitch_interval_ms =
        live_stitch_interval_slider_->integerValue();
    document.processing_settings.fluorescence_blend_mode =
        static_cast<int>(fluorescence_blend_mode_);
    std::wstring error;
    if (!ProjectRepository::Save(std::filesystem::path(file_name.toStdWString()), document, error)) {
        QMessageBox::warning(this, tr("保存失败"), errorText(error));
        return;
    }
    statusBar()->showMessage(tr("项目已保存：%1").arg(file_name), 5000);
}

void CameraMainWindow::openProject()
{
    const QString file_name = QFileDialog::getOpenFileName(
        this, tr("打开项目"), {}, tr("CameraView 项目 (*.cvproject *.json);;所有文件 (*)"));
    if (file_name.isEmpty()) {
        return;
    }
    ProjectDocument document;
    std::wstring error;
    if (!ProjectRepository::Load(std::filesystem::path(file_name.toStdWString()), document, error)) {
        QMessageBox::warning(this, tr("打开失败"), errorText(error));
        return;
    }
    ProjectSessionState state = ProjectSessionMapper::FromDocument(std::move(document));
    if (live_stitch_active_) stopLiveStitch(false);
    stitch_tiles_.clear();
    stitch_tile_sources_.clear();
    edf_stack_.clear();
    stitch_result_ = {};
    stitch_result_metadata_ = {};
    stitch_retry_tiles_.clear();
    stitch_retry_sources_.clear();
    stitch_retry_available_ = false;
    if (stitch_retry_button_) stitch_retry_button_->setEnabled(false);
    edf_result_ = {};
    objective_labels_ = std::move(state.objective_labels);
    objective_calibrations_ = std::move(state.objective_calibrations);
    selected_objective_index_ = state.selected_objective_index;
    if (objective_labels_.empty()) {
        const ObjectiveCalibrationState defaults = ObjectiveCalibrationSettings::Defaults();
        objective_labels_ = defaults.labels;
        objective_calibrations_ = defaults.calibrations;
        selected_objective_index_ = defaults.selected_index;
    }
    if (objective_calibrations_.size() < objective_labels_.size()) {
        objective_calibrations_.resize(
            objective_labels_.size(), CalibrationProfile::Uncalibrated());
    }
    selected_objective_index_ = std::clamp(
        selected_objective_index_, 0, static_cast<int>(objective_labels_.size()) - 1);
    calibration_ = objective_calibrations_[static_cast<std::size_t>(selected_objective_index_)];
    measurements_ = std::move(state.measurements);
    applyGlobalMeasurementColor();
    refreshObjectiveControls();
    dyes_ = std::move(state.dye_profiles);
    channels_ = std::move(state.fluorescence_channels);
    const auto restore_combo = [](QComboBox* combo, int value) {
        const int index = combo ? combo->findData(value) : -1;
        if (index >= 0) combo->setCurrentIndex(index);
    };
    restore_combo(stitch_layout_combo_, state.stitch_layout_mode);
    stitch_rows_spin_->setValue(state.stitch_grid_rows);
    stitch_cols_spin_->setValue(state.stitch_grid_cols);
    stitch_overlap_slider_->setValue(state.stitch_overlap_percent);
    restore_combo(stitch_registration_combo_, state.stitch_registration_method);
    restore_combo(stitch_transform_combo_, state.stitch_transform_model);
    restore_combo(stitch_blend_combo_, state.stitch_blend_mode);
    live_stitch_interval_slider_->setValue(state.live_stitch_interval_ms);
    restore_combo(fluorescence_blend_combo_, state.fluorescence_blend_mode);
    if (dyes_.empty()) {
        dyes_ = DyeLibrary::DefaultDyes();
    }
    dye_combo_->clear();
    for (const DyeProfile& dye : dyes_) {
        dye_combo_->addItem(QString::fromStdWString(FluorescenceFormatter::FormatDyeLabel(dye)));
    }
    refreshFluorescenceChannelList(channels_.empty() ? -1 : 0);
    updateCalibrationUi();
    updateMeasurementList();
    saveObjectiveCalibrationMemory();
    invalidateStitchResult();
    refreshStitchTileList();
    focus_map_button_->setEnabled(false);
    updateImagePresentation();
    statusBar()->showMessage(tr("项目已打开：%1").arg(file_name), 5000);
}

void CameraMainWindow::refreshDevices()
{
    if (camera_open_ || camera_panel_state_ == CameraPanelState::Connecting ||
        camera_panel_state_ == CameraPanelState::Reconfiguring) {
        camera_feedback_label_->setText(tr("请先断开相机，再刷新设备列表"));
        return;
    }
    setCameraPanelState(CameraPanelState::Initializing, tr("正在刷新设备…"));
    updateCameraControlAvailability();
    QMetaObject::invokeMethod(camera_worker_, "refreshDevices", Qt::QueuedConnection);
}

void CameraMainWindow::openSelectedCamera()
{
    if (camera_open_ || camera_panel_state_ == CameraPanelState::Connecting ||
        camera_panel_state_ == CameraPanelState::Reconfiguring) return;
    const int row = device_combo_->currentIndex();
    const int device_index = row >= 0 && row < camera_indices_.size() ? camera_indices_[row] : -1;
    if (device_index < 0) {
        setCameraPanelState(CameraPanelState::NoDevice, tr("请先选择相机设备"));
        return;
    }
    loadCameraProfile();
    camera_profile_reconfigure_pending_ = camera_profile_loaded_;
    QSettings settings;
    settings.setValue(QStringLiteral("CameraPanel/lastDeviceKey"), currentCameraDeviceKey());
    setCameraPanelState(CameraPanelState::Connecting, tr("正在连接相机…"));
    updateCameraControlAvailability();
    QMetaObject::invokeMethod(camera_worker_, "openCamera", Qt::QueuedConnection,
        Q_ARG(int, device_index), Q_ARG(double, exposure_spin_->value()));
}

void CameraMainWindow::stopCamera()
{
    if (camera_panel_state_ == CameraPanelState::Connecting ||
        camera_panel_state_ == CameraPanelState::Reconfiguring) {
        camera_feedback_label_->setText(tr("当前操作完成后即可断开相机"));
        return;
    }
    if (live_stitch_active_) stopLiveStitch(false);
    if (FluorescenceCaptureSequence::IsActive(fluorescence_capture_state_)) {
        cancelFluorescenceCaptureSequence();
    }
    camera_profile_reconfigure_pending_ = false;
    camera_roi_selection_pending_ = false;
    setCameraPanelState(CameraPanelState::Disconnected, tr("正在断开相机…"));
    QMetaObject::invokeMethod(camera_worker_, "stopCamera", Qt::QueuedConnection);
}

void CameraMainWindow::onDevicesReady(QStringList labels, QVector<int> indices, QString diagnostic)
{
    QSettings settings;
    const QString remembered_key = settings.value(
        QStringLiteral("CameraPanel/lastDeviceKey")).toString();
    camera_indices_ = std::move(indices);
    {
        const QSignalBlocker blocker(device_combo_);
        device_combo_->clear();
        device_combo_->addItems(labels);
        int remembered_row = -1;
        for (int row = 0; row < labels.size(); ++row) {
            const QString raw_key = QStringLiteral("%1|%2")
                .arg(labels[row])
                .arg(row < camera_indices_.size() ? camera_indices_[row] : row);
            const QString key = QString::fromLatin1(raw_key.toUtf8().toBase64(
                QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
            if (key == remembered_key) {
                remembered_row = row;
                break;
            }
        }
        if (remembered_row >= 0) device_combo_->setCurrentIndex(remembered_row);
        else if (!labels.isEmpty()) device_combo_->setCurrentIndex(0);
    }
    loadCameraProfile();
    if (labels.isEmpty()) {
        setCameraPanelState(
            diagnostic.contains(tr("失败")) ? CameraPanelState::Error : CameraPanelState::NoDevice,
            diagnostic);
    } else {
        setCameraPanelState(CameraPanelState::Ready, diagnostic);
    }
    statusBar()->showMessage(diagnostic, 5000);
    updateCameraControlAvailability();

    if (!startup_auto_connect_attempted_) {
        startup_auto_connect_attempted_ = true;
        if (!remembered_key.isEmpty() && currentCameraDeviceKey() == remembered_key) {
            QTimer::singleShot(0, this, &CameraMainWindow::openSelectedCamera);
        }
    }
}

void CameraMainWindow::onCameraCapabilities(
    CameraCapabilities capabilities,
    CameraConfiguration configuration,
    CameraOpenInfo openInfo)
{
    camera_capabilities_ = std::move(capabilities);
    camera_configuration_ = configuration;
    camera_open_info_ = openInfo;

    camera_ui_updating_ = true;
    camera_resolution_combo_->clear();
    for (const CameraResolutionOption& option : camera_capabilities_.resolutions) {
        camera_resolution_combo_->addItem(
            tr("%1 × %2").arg(option.width).arg(option.height), option.index);
    }
    exposure_spin_->setRange(
        camera_capabilities_.exposure_minimum,
        camera_capabilities_.exposure_maximum);
    double gain_minimum = 0.01;
    double gain_maximum = 100.0;
    if (!camera_capabilities_.gain_values.empty()) {
        const auto [minimum, maximum] = std::minmax_element(
            camera_capabilities_.gain_values.begin(), camera_capabilities_.gain_values.end());
        gain_minimum = *minimum;
        gain_maximum = *maximum;
    }
    gain_spin_->setRange(gain_minimum, gain_maximum);
    camera_gain_slider_->setRange(
        qRound(gain_minimum * 100.0), qRound(gain_maximum * 100.0));
    for (QDoubleSpinBox* spin : camera_rgb_gain_spins_) {
        spin->setRange(gain_minimum, gain_maximum);
    }
    for (QSpinBox* spin : camera_rgb_offset_spins_) {
        spin->setRange(
            camera_capabilities_.offset_minimum,
            camera_capabilities_.offset_maximum);
    }
    camera_ui_updating_ = false;

    camera_device_summary_label_->setText(
        tr("%1 · 类型 %2").arg(device_combo_->currentText()).arg(openInfo.type));
    camera_telemetry_label_->setText(
        tr("分辨率 %1 × %2  ·  FPS —  ·  %3  ·  8 位")
            .arg(openInfo.width).arg(openInfo.height)
            .arg(cameraFrameFormatName(camera_capabilities_.frame_format)));

    if (camera_profile_reconfigure_pending_ && camera_profile_loaded_) {
        camera_profile_reconfigure_pending_ = false;
        CameraConfiguration desired = camera_profile_configuration_;
        desired.roi = {};
        const bool resolution_available = std::any_of(
            camera_capabilities_.resolutions.begin(),
            camera_capabilities_.resolutions.end(),
            [&desired](const CameraResolutionOption& option) {
                return option.index == desired.resolution_index;
            });
        if (!resolution_available) desired.resolution_index = configuration.resolution_index;
        if (!camera_capabilities_.has_trigger) desired.trigger_mode = CameraTriggerMode::Free;
        if (!camera_capabilities_.has_flip) desired.vertical_flip = false;
        if (!camera_capabilities_.has_mirror) desired.horizontal_mirror = false;
        desired.exposure_ms = static_cast<float>(std::clamp(
            static_cast<double>(desired.exposure_ms),
            exposure_spin_->minimum(), exposure_spin_->maximum()));
        const auto differs = [](double left, double right) {
            return std::abs(left - right) > 0.0001;
        };
        const bool requires_reconfigure =
            desired.resolution_index != configuration.resolution_index ||
            desired.trigger_mode != configuration.trigger_mode ||
            desired.vertical_flip != configuration.vertical_flip ||
            desired.horizontal_mirror != configuration.horizontal_mirror ||
            differs(desired.exposure_ms, configuration.exposure_ms) ||
            (camera_capabilities_.has_gain &&
                (differs(desired.red_gain, configuration.red_gain) ||
                 differs(desired.green_gain, configuration.green_gain) ||
                 differs(desired.blue_gain, configuration.blue_gain))) ||
            (camera_capabilities_.has_offset &&
                (desired.red_offset != configuration.red_offset ||
                 desired.green_offset != configuration.green_offset ||
                 desired.blue_offset != configuration.blue_offset));
        if (!requires_reconfigure) {
            updateCameraConfigurationUi(configuration);
            last_sent_exposure_ = configuration.exposure_ms;
            last_sent_rgb_gain_ = {
                configuration.red_gain, configuration.green_gain, configuration.blue_gain};
            last_sent_rgb_offset_ = {
                configuration.red_offset, configuration.green_offset, configuration.blue_offset};
            updateCameraControlAvailability();
            return;
        }
        setCameraPanelState(CameraPanelState::Reconfiguring, tr("正在恢复此设备的参数…"));
        QMetaObject::invokeMethod(camera_worker_, "reconfigureCamera", Qt::QueuedConnection,
            Q_ARG(CameraConfiguration, desired));
        return;
    }

    updateCameraConfigurationUi(configuration);
    updateCameraControlAvailability();
}

void CameraMainWindow::setCameraPanelState(CameraPanelState state, const QString& message)
{
    camera_panel_state_ = state;
    if (!camera_state_label_ || !camera_state_badge_) return;
    QString badge;
    QString visual_state;
    switch (state) {
    case CameraPanelState::Initializing: badge = tr("初始化"); visual_state = QStringLiteral("busy"); break;
    case CameraPanelState::NoDevice: badge = tr("无设备"); visual_state = QStringLiteral("warning"); break;
    case CameraPanelState::Ready: badge = tr("待连接"); visual_state = QStringLiteral("ready"); break;
    case CameraPanelState::Connecting: badge = tr("连接中"); visual_state = QStringLiteral("busy"); break;
    case CameraPanelState::Previewing: badge = tr("预览中"); visual_state = QStringLiteral("success"); break;
    case CameraPanelState::Reconfiguring: badge = tr("重配置"); visual_state = QStringLiteral("busy"); break;
    case CameraPanelState::WaitingTrigger: badge = tr("等待触发"); visual_state = QStringLiteral("warning"); break;
    case CameraPanelState::Disconnected: badge = tr("已断开"); visual_state = QStringLiteral("ready"); break;
    case CameraPanelState::Error: badge = tr("错误"); visual_state = QStringLiteral("error"); break;
    }
    camera_state_badge_->setText(badge);
    camera_state_badge_->setProperty("cameraState", visual_state);
    camera_state_badge_->style()->unpolish(camera_state_badge_);
    camera_state_badge_->style()->polish(camera_state_badge_);
    camera_state_label_->setText(message);
}

void CameraMainWindow::updateCameraControlAvailability()
{
    if (!device_combo_) return;
    const bool transitioning = camera_panel_state_ == CameraPanelState::Initializing ||
        camera_panel_state_ == CameraPanelState::Connecting ||
        camera_panel_state_ == CameraPanelState::Reconfiguring;
    const bool has_device = device_combo_->currentIndex() >= 0 && !camera_indices_.isEmpty();
    device_combo_->setEnabled(!camera_open_ && !transitioning);
    camera_refresh_button_->setEnabled(!camera_open_ && !transitioning);
    camera_connection_button_->setEnabled(camera_open_ || (has_device && !transitioning));
    camera_connection_button_->setText(camera_open_ ? tr("断开相机") : tr("连接相机"));
    camera_connection_button_->setIcon(style()->standardIcon(
        camera_open_ ? QStyle::SP_DialogCloseButton : QStyle::SP_DialogApplyButton));
    camera_connection_button_->setProperty(
        "role", camera_open_ ? QStringLiteral("danger") : QStringLiteral("primary"));
    camera_connection_button_->style()->unpolish(camera_connection_button_);
    camera_connection_button_->style()->polish(camera_connection_button_);

    // Exposure and gain remain editable while offline as per-device presets.
    exposure_spin_->setEnabled(!transitioning && (!camera_open_ || camera_capabilities_.has_exposure));
    camera_exposure_slider_->setEnabled(exposure_spin_->isEnabled());
    gain_spin_->setEnabled(!transitioning && (!camera_open_ || camera_capabilities_.has_gain));
    camera_gain_slider_->setEnabled(gain_spin_->isEnabled());
    camera_auto_exposure_button_->setEnabled(
        camera_open_ && !transitioning && camera_capabilities_.has_auto_exposure);
    camera_white_balance_button_->setEnabled(
        camera_open_ && !transitioning && camera_capabilities_.has_white_balance);

    camera_color_group_->setVisible(!camera_open_ || camera_capabilities_.color);
    for (QDoubleSpinBox* spin : camera_rgb_gain_spins_) {
        spin->setEnabled(!transitioning && (!camera_open_ || camera_capabilities_.has_gain));
        spin->setToolTip(camera_open_ && !camera_capabilities_.has_gain
            ? tr("当前设备未提供颜色增益接口") : QString());
    }
    for (QSpinBox* spin : camera_rgb_offset_spins_) {
        spin->setEnabled(camera_open_ && !transitioning && camera_capabilities_.has_offset);
        spin->setToolTip(camera_open_ && !camera_capabilities_.has_offset
            ? tr("当前设备未提供颜色偏移接口") : QString());
    }

    camera_resolution_combo_->setEnabled(camera_open_ && !transitioning &&
        camera_capabilities_.resolutions.size() > 1);
    camera_trigger_combo_->setEnabled(camera_open_ && !transitioning && camera_capabilities_.has_trigger);
    camera_flip_check_->setEnabled(camera_open_ && !transitioning && camera_capabilities_.has_flip);
    camera_mirror_check_->setEnabled(camera_open_ && !transitioning && camera_capabilities_.has_mirror);
    const bool software_trigger = camera_configuration_.trigger_mode == CameraTriggerMode::Software;
    camera_single_frame_button_->setVisible(software_trigger);
    camera_single_frame_button_->setEnabled(camera_open_ && !transitioning && software_trigger);
    const bool roi_enabled = camera_open_ && !transitioning && camera_capabilities_.has_roi;
    camera_roi_select_button_->setEnabled(roi_enabled && !latest_camera_image_.isNull());
    camera_roi_apply_button_->setEnabled(roi_enabled);
    camera_roi_reset_button_->setEnabled(roi_enabled && camera_configuration_.roi.Enabled());
    for (QSpinBox* spin : camera_roi_spins_) spin->setEnabled(roi_enabled);
}

QString CameraMainWindow::currentCameraDeviceKey() const
{
    const int row = device_combo_ ? device_combo_->currentIndex() : -1;
    if (row < 0) return {};
    const int index = row < camera_indices_.size() ? camera_indices_[row] : row;
    const QString raw = QStringLiteral("%1|%2").arg(device_combo_->itemText(row)).arg(index);
    return QString::fromLatin1(raw.toUtf8().toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

void CameraMainWindow::loadCameraProfile()
{
    const QString key = currentCameraDeviceKey();
    camera_profile_loaded_ = false;
    if (key.isEmpty()) return;
    QSettings settings;
    settings.beginGroup(QStringLiteral("CameraProfiles/%1").arg(key));
    const bool has_saved_profile = settings.contains(QStringLiteral("exposureMs")) ||
        settings.contains(QStringLiteral("resolutionIndex")) ||
        settings.contains(QStringLiteral("triggerMode"));
    camera_profile_configuration_ = {};
    camera_profile_configuration_.exposure_ms = settings.value(
        QStringLiteral("exposureMs"), 10.0).toFloat();
    camera_profile_configuration_.red_gain = settings.value(QStringLiteral("redGain"), 1.0).toFloat();
    camera_profile_configuration_.green_gain = settings.value(QStringLiteral("greenGain"), 1.0).toFloat();
    camera_profile_configuration_.blue_gain = settings.value(QStringLiteral("blueGain"), 1.0).toFloat();
    camera_profile_configuration_.red_offset = settings.value(QStringLiteral("redOffset"), 0).toInt();
    camera_profile_configuration_.green_offset = settings.value(QStringLiteral("greenOffset"), 0).toInt();
    camera_profile_configuration_.blue_offset = settings.value(QStringLiteral("blueOffset"), 0).toInt();
    camera_profile_configuration_.resolution_index = settings.value(QStringLiteral("resolutionIndex"), 0).toInt();
    camera_profile_configuration_.trigger_mode = static_cast<CameraTriggerMode>(
        settings.value(QStringLiteral("triggerMode"), 0).toInt());
    camera_profile_configuration_.vertical_flip = settings.value(QStringLiteral("verticalFlip"), false).toBool();
    camera_profile_configuration_.horizontal_mirror = settings.value(QStringLiteral("horizontalMirror"), false).toBool();
    settings.endGroup();
    camera_profile_configuration_.roi = {};
    camera_profile_loaded_ = has_saved_profile;
    updateCameraConfigurationUi(camera_profile_configuration_);
}

void CameraMainWindow::saveCameraProfile() const
{
    const QString key = currentCameraDeviceKey();
    if (key.isEmpty()) return;
    QSettings settings;
    settings.setValue(QStringLiteral("CameraPanel/lastDeviceKey"), key);
    settings.beginGroup(QStringLiteral("CameraProfiles/%1").arg(key));
    settings.setValue(QStringLiteral("exposureMs"), exposure_spin_->value());
    settings.setValue(QStringLiteral("redGain"), camera_rgb_gain_spins_[0]->value());
    settings.setValue(QStringLiteral("greenGain"), camera_rgb_gain_spins_[1]->value());
    settings.setValue(QStringLiteral("blueGain"), camera_rgb_gain_spins_[2]->value());
    settings.setValue(QStringLiteral("redOffset"), camera_rgb_offset_spins_[0]->value());
    settings.setValue(QStringLiteral("greenOffset"), camera_rgb_offset_spins_[1]->value());
    settings.setValue(QStringLiteral("blueOffset"), camera_rgb_offset_spins_[2]->value());
    settings.setValue(QStringLiteral("resolutionIndex"),
        camera_resolution_combo_->currentData().toInt());
    settings.setValue(QStringLiteral("triggerMode"),
        camera_trigger_combo_->currentData().toInt());
    settings.setValue(QStringLiteral("verticalFlip"), camera_flip_check_->isChecked());
    settings.setValue(QStringLiteral("horizontalMirror"), camera_mirror_check_->isChecked());
    settings.endGroup();
}

void CameraMainWindow::updateCameraConfigurationUi(const CameraConfiguration& configuration)
{
    camera_ui_updating_ = true;
    exposure_spin_->setValue(std::clamp(
        static_cast<double>(configuration.exposure_ms),
        exposure_spin_->minimum(), exposure_spin_->maximum()));
    const double exposure_span = exposure_spin_->maximum() - exposure_spin_->minimum();
    camera_exposure_slider_->setValue(exposure_span > 0.0
        ? qRound((exposure_spin_->value() - exposure_spin_->minimum()) * 1000.0 / exposure_span) : 0);
    const std::array<double, 3> gains{
        configuration.red_gain, configuration.green_gain, configuration.blue_gain};
    const std::array<int, 3> offsets{
        configuration.red_offset, configuration.green_offset, configuration.blue_offset};
    for (int channel = 0; channel < 3; ++channel) {
        camera_rgb_gain_spins_[channel]->setValue(std::clamp(
            gains[channel],
            camera_rgb_gain_spins_[channel]->minimum(),
            camera_rgb_gain_spins_[channel]->maximum()));
        camera_rgb_offset_spins_[channel]->setValue(offsets[channel]);
    }
    gain_spin_->setValue(camera_rgb_gain_spins_[0]->value());
    camera_gain_slider_->setValue(qRound(gain_spin_->value() * 100.0));
    const int resolution_row = camera_resolution_combo_->findData(configuration.resolution_index);
    if (resolution_row >= 0) camera_resolution_combo_->setCurrentIndex(resolution_row);
    const int trigger_row = camera_trigger_combo_->findData(
        static_cast<int>(configuration.trigger_mode));
    if (trigger_row >= 0) camera_trigger_combo_->setCurrentIndex(trigger_row);
    camera_flip_check_->setChecked(configuration.vertical_flip);
    camera_mirror_check_->setChecked(configuration.horizontal_mirror);
    camera_roi_spins_[0]->setValue(configuration.roi.x);
    camera_roi_spins_[1]->setValue(configuration.roi.y);
    camera_roi_spins_[2]->setValue(configuration.roi.width);
    camera_roi_spins_[3]->setValue(configuration.roi.height);
    camera_ui_updating_ = false;
}

void CameraMainWindow::scheduleCameraParameterApply()
{
    saveCameraProfile();
    camera_feedback_label_->setText(
        camera_open_ ? tr("参数将在输入停止后自动应用…") : tr("参数预设已保存，连接时应用"));
    camera_feedback_label_->setProperty("feedbackState", QStringLiteral("info"));
    camera_feedback_label_->style()->unpolish(camera_feedback_label_);
    camera_feedback_label_->style()->polish(camera_feedback_label_);
    if (camera_open_) camera_parameter_timer_->start();
}

void CameraMainWindow::applyPendingCameraParameters()
{
    if (!camera_open_ || camera_panel_state_ == CameraPanelState::Reconfiguring) return;
    const double exposure = exposure_spin_->value();
    if (camera_capabilities_.has_exposure && !qFuzzyCompare(exposure + 1.0, last_sent_exposure_ + 1.0)) {
        last_sent_exposure_ = exposure;
        QMetaObject::invokeMethod(camera_worker_, "setExposure", Qt::QueuedConnection,
            Q_ARG(double, exposure));
    }
    std::array<double, 3> gains{
        camera_rgb_gain_spins_[0]->value(),
        camera_rgb_gain_spins_[1]->value(),
        camera_rgb_gain_spins_[2]->value()};
    if (camera_capabilities_.has_gain && gains != last_sent_rgb_gain_) {
        last_sent_rgb_gain_ = gains;
        QMetaObject::invokeMethod(camera_worker_, "setRgbGain", Qt::QueuedConnection,
            Q_ARG(double, gains[0]), Q_ARG(double, gains[1]), Q_ARG(double, gains[2]));
    }
    std::array<int, 3> offsets{
        camera_rgb_offset_spins_[0]->value(),
        camera_rgb_offset_spins_[1]->value(),
        camera_rgb_offset_spins_[2]->value()};
    if (camera_capabilities_.has_offset && offsets != last_sent_rgb_offset_) {
        last_sent_rgb_offset_ = offsets;
        QMetaObject::invokeMethod(camera_worker_, "setRgbOffset", Qt::QueuedConnection,
            Q_ARG(int, offsets[0]), Q_ARG(int, offsets[1]), Q_ARG(int, offsets[2]));
    }
}

void CameraMainWindow::applyCameraConfigurationFromUi(bool includeRoi)
{
    saveCameraProfile();
    if (!camera_open_ || camera_ui_updating_) return;
    CameraConfiguration requested = camera_configuration_;
    requested.resolution_index = camera_resolution_combo_->currentData().toInt();
    requested.trigger_mode = static_cast<CameraTriggerMode>(camera_trigger_combo_->currentData().toInt());
    requested.vertical_flip = camera_flip_check_->isChecked();
    requested.horizontal_mirror = camera_mirror_check_->isChecked();
    requested.exposure_ms = static_cast<float>(exposure_spin_->value());
    requested.red_gain = static_cast<float>(camera_rgb_gain_spins_[0]->value());
    requested.green_gain = static_cast<float>(camera_rgb_gain_spins_[1]->value());
    requested.blue_gain = static_cast<float>(camera_rgb_gain_spins_[2]->value());
    requested.red_offset = camera_rgb_offset_spins_[0]->value();
    requested.green_offset = camera_rgb_offset_spins_[1]->value();
    requested.blue_offset = camera_rgb_offset_spins_[2]->value();
    if (includeRoi) {
        requested.roi = {
            camera_roi_spins_[0]->value(), camera_roi_spins_[1]->value(),
            camera_roi_spins_[2]->value(), camera_roi_spins_[3]->value()};
    }
    setCameraPanelState(CameraPanelState::Reconfiguring, tr("正在重配置相机…"));
    updateCameraControlAvailability();
    QMetaObject::invokeMethod(camera_worker_, "reconfigureCamera", Qt::QueuedConnection,
        Q_ARG(CameraConfiguration, requested));
}

void CameraMainWindow::applyCameraRoiInputs()
{
    const int resolution_index = camera_resolution_combo_->currentData().toInt();
    const auto option = std::find_if(
        camera_capabilities_.resolutions.begin(), camera_capabilities_.resolutions.end(),
        [resolution_index](const CameraResolutionOption& value) {
            return value.index == resolution_index;
        });
    if (option == camera_capabilities_.resolutions.end()) return;
    const int x = std::clamp(camera_roi_spins_[0]->value(), 0, option->width - 1);
    const int y = std::clamp(camera_roi_spins_[1]->value(), 0, option->height - 1);
    const int width = std::clamp(camera_roi_spins_[2]->value(), 1, option->width - x);
    const int height = std::clamp(camera_roi_spins_[3]->value(), 1, option->height - y);
    camera_ui_updating_ = true;
    camera_roi_spins_[0]->setValue(x);
    camera_roi_spins_[1]->setValue(y);
    camera_roi_spins_[2]->setValue(width);
    camera_roi_spins_[3]->setValue(height);
    camera_ui_updating_ = false;
    applyCameraConfigurationFromUi();
}

void CameraMainWindow::resetCameraRoi()
{
    camera_ui_updating_ = true;
    for (QSpinBox* spin : camera_roi_spins_) spin->setValue(0);
    camera_ui_updating_ = false;
    applyCameraConfigurationFromUi();
}

void CameraMainWindow::startCameraRoiSelection()
{
    if (!camera_open_ || !camera_capabilities_.has_roi || latest_camera_image_.isNull()) return;
    if (camera_configuration_.roi.Enabled()) {
        camera_roi_selection_pending_ = true;
        camera_feedback_label_->setText(tr("正在恢复全幅，随后进入 ROI 框选…"));
        resetCameraRoi();
        return;
    }
    camera_roi_previous_tool_ = canvas_->tool();
    camera_roi_previous_edge_snapping_ = canvas_->edgeSnappingEnabled();
    canvas_->setEdgeSnappingEnabled(false);
    canvas_->setTool(CanvasTool::CameraRoi);
    camera_feedback_label_->setText(tr("在画面中拖动框选 ROI，按 Esc 取消"));
    camera_feedback_label_->setProperty("feedbackState", QStringLiteral("warning"));
    camera_feedback_label_->style()->unpolish(camera_feedback_label_);
    camera_feedback_label_->style()->polish(camera_feedback_label_);
    canvas_->setFocus();
}

void CameraMainWindow::restoreToolAfterCameraRoi(const QString& message)
{
    canvas_->setEdgeSnappingEnabled(camera_roi_previous_edge_snapping_);
    canvas_->setTool(camera_roi_previous_tool_);
    camera_feedback_label_->setText(message);
    camera_feedback_label_->setProperty("feedbackState", QStringLiteral("info"));
    camera_feedback_label_->style()->unpolish(camera_feedback_label_);
    camera_feedback_label_->style()->polish(camera_feedback_label_);
}

void CameraMainWindow::onCameraFrame(QImage image, quint64 sequence, quint32 timestamp)
{
    const bool first_frame = latest_camera_image_.isNull();
    latest_camera_image_ = std::move(image);
    latest_camera_sequence_ = sequence;
    latest_camera_timestamp_ = timestamp;
    if (live_stitch_active_) {
        canvas_->setLivePreviewOverlay(latest_camera_image_);
    } else if (!busy_ && !smart_count_session_active_) {
        if (hasNeutralPresentation()) {
            presentLiveCameraImage();
        } else {
            setCurrentFrame(latestCameraFrame(), tr("MUCam 实时预览"), QStringLiteral("camera-live"));
        }
    }
    if (camera_open_) {
        if (!preview_fps_timer_.isValid()) {
            preview_fps_timer_.start();
        }
        ++preview_frames_since_sample_;
        const qint64 elapsed_ms = preview_fps_timer_.elapsed();
        if (elapsed_ms >= 500) {
            const double fps = static_cast<double>(preview_frames_since_sample_) * 1000.0 /
                static_cast<double>(elapsed_ms);
            camera_telemetry_label_->setText(tr("分辨率 %1 × %2  ·  FPS %3  ·  %4  ·  8 位")
                .arg(latest_camera_image_.width())
                .arg(latest_camera_image_.height())
                .arg(fps, 0, 'f', 1)
                .arg(cameraFrameFormatName(camera_capabilities_.frame_format)));
            preview_fps_label_->setText(tr("FPS %1").arg(fps, 0, 'f', 1));
            preview_frames_since_sample_ = 0;
            preview_fps_timer_.restart();
        }
    }
    if (FluorescenceCaptureSequence::IsActive(fluorescence_capture_state_)) {
        updateFluorescenceCaptureUi();
    }
    if (camera_open_) {
        QMetaObject::invokeMethod(camera_worker_, "frameConsumed", Qt::QueuedConnection);
    }
    if (first_frame) updateCameraControlAvailability();
}

void CameraMainWindow::presentLiveCameraImage()
{
    if (latest_camera_image_.isNull()) return;
    const QString identity = QStringLiteral("camera-live");
    const bool source_changed = current_source_identity_ != identity;
    if (source_changed) {
        profile_line_points_.clear();
        if (smart_count_cancel_token_) smart_count_cancel_token_->store(true);
        smart_target_samples_.clear();
        smart_target_result_ = {};
        smart_count_session_active_ = false;
        updateSmartTargetUi();
        current_frame_ = {};
        current_source_ = tr("MUCam 实时预览");
        current_source_identity_ = identity;
        rebuildOverlays();
    }
    ++image_generation_;
    display_frame_ = {};
    source_label_->setText(tr("%1 · %2 × %3")
        .arg(current_source_)
        .arg(latest_camera_image_.width())
        .arg(latest_camera_image_.height()));
    if (export_action_) export_action_->setEnabled(true);
    canvas_->setImage(latest_camera_image_);
    canvas_->setProperty("directCameraPreview", true);
    canvas_->setProperty("cameraPreviewSequence", QVariant::fromValue<qulonglong>(latest_camera_sequence_));
    if (yolo_workspace_ && function_tabs_ && function_tabs_->currentWidget() == yolo_workspace_) {
        yolo_workspace_->setCurrentImage(
            latest_camera_image_, current_source_, current_source_identity_);
    }
    updateLiveHistogram();
}

bool CameraMainWindow::hasNeutralPresentation() const
{
    return (!brightness_slider_ || brightness_slider_->value() == 0) &&
        (!contrast_slider_ || contrast_slider_->value() == 0) &&
        (!gamma_slider_ || gamma_slider_->value() == 10) &&
        (!level_slider_ || level_slider_->value() == 128) &&
        (!width_slider_ || width_slider_->value() == 256) &&
        palette_ == PseudoColorPalette::Original &&
        image_filter_pipeline_.empty() &&
        (!fusion_enabled_ || channels_.empty());
}

ImageFrame CameraMainWindow::latestCameraFrame() const
{
    return latest_camera_image_.isNull()
        ? ImageFrame{}
        : imageFrameFromQImage(
            latest_camera_image_, latest_camera_sequence_, latest_camera_timestamp_);
}

ImageFrame CameraMainWindow::sourceFrame() const
{
    if (current_source_identity_ == QStringLiteral("camera-live") && !latest_camera_image_.isNull()) {
        return latestCameraFrame();
    }
    return current_frame_;
}

void CameraMainWindow::updateLiveHistogram()
{
    if (!histogram_ || !histogram_->isVisible()) return;
    if (!live_histogram_timer_.isValid()) {
        live_histogram_timer_.start();
    } else if (live_histogram_timer_.elapsed() < 250) {
        return;
    } else {
        live_histogram_timer_.restart();
    }
    const ImageFrame frame = latestCameraFrame();
    if (!frame.IsValid()) return;
    const HistogramData data = ComputeHistogram(frame, histogram_channel_);
    const QColor colors[] = {
        QColor(120, 190, 255), QColor(239, 68, 68), QColor(34, 197, 94), QColor(59, 130, 246)};
    histogram_->setHistogram(data, colors[static_cast<int>(histogram_channel_)]);
}

void CameraMainWindow::setCurrentFrame(
    ImageFrame frame,
    const QString& source,
    const QString& sourceIdentity)
{
    const QString new_identity = sourceIdentity.isEmpty() ? source : sourceIdentity;
    if (!current_source_identity_.isEmpty() && current_source_identity_ != new_identity) {
        profile_line_points_.clear();
        image_filter_pipeline_.clear();
        updateImageFilterPipelineUi();
        if (smart_count_cancel_token_) smart_count_cancel_token_->store(true);
        smart_target_samples_.clear();
        smart_target_result_ = {};
        smart_count_session_active_ = false;
        updateSmartTargetUi();
    }
    ++image_generation_;
    current_frame_ = std::move(frame);
    current_source_ = source;
    current_source_identity_ = new_identity;
    if (export_action_) export_action_->setEnabled(current_frame_.IsValid());
    source_label_->setText(tr("%1 · %2 × %3")
        .arg(source)
        .arg(current_frame_.width)
        .arg(current_frame_.height));
    export_action_->setEnabled(current_frame_.IsValid());
    updateImagePresentation();
}

void CameraMainWindow::updateImageFilterControls()
{
    if (!image_filter_combo_ || !image_filter_parameter_slider_ || !image_filter_parameter_label_) return;
    const ImageFilterKind kind = static_cast<ImageFilterKind>(image_filter_combo_->currentData().toInt());
    image_filter_parameter_slider_->setSuffix({});
    switch (kind) {
    case ImageFilterKind::GaussianBlur:
        image_filter_parameter_label_->setText(tr("模糊半径"));
        image_filter_parameter_slider_->setRange(1, 10);
        image_filter_parameter_slider_->setValue(2);
        image_filter_parameter_slider_->setSuffix(tr(" px"));
        image_filter_parameter_slider_->setEnabled(true);
        break;
    case ImageFilterKind::MedianDenoise:
        image_filter_parameter_label_->setText(tr("降噪半径"));
        image_filter_parameter_slider_->setRange(1, 3);
        image_filter_parameter_slider_->setValue(1);
        image_filter_parameter_slider_->setSuffix(tr(" px"));
        image_filter_parameter_slider_->setEnabled(true);
        break;
    case ImageFilterKind::Sharpen:
        image_filter_parameter_label_->setText(tr("增强强度"));
        image_filter_parameter_slider_->setRange(10, 300);
        image_filter_parameter_slider_->setValue(100);
        image_filter_parameter_slider_->setSuffix(tr(" %"));
        image_filter_parameter_slider_->setEnabled(true);
        break;
    case ImageFilterKind::EdgeDetection:
        image_filter_parameter_label_->setText(tr("边缘阈值（0 为连续灰度）"));
        image_filter_parameter_slider_->setRange(0, 255);
        image_filter_parameter_slider_->setValue(40);
        image_filter_parameter_slider_->setEnabled(true);
        break;
    case ImageFilterKind::BinaryThreshold:
        image_filter_parameter_label_->setText(tr("二值阈值"));
        image_filter_parameter_slider_->setRange(0, 255);
        image_filter_parameter_slider_->setValue(128);
        image_filter_parameter_slider_->setEnabled(true);
        break;
    default:
        image_filter_parameter_label_->setText(tr("参数（当前功能无需设置）"));
        image_filter_parameter_slider_->setRange(0, 0);
        image_filter_parameter_slider_->setValue(0);
        image_filter_parameter_slider_->setEnabled(false);
        break;
    }
}

void CameraMainWindow::updateImageFilterPipelineUi()
{
    if (!image_filter_pipeline_label_) return;
    if (image_filter_pipeline_.empty()) {
        image_filter_pipeline_label_->setText(tr("处理链：无（处理为非破坏性，可随时恢复原图）"));
        return;
    }
    QStringList descriptions;
    descriptions.reserve(static_cast<qsizetype>(image_filter_pipeline_.size()));
    for (const ImageFilterStep& step : image_filter_pipeline_) {
        descriptions.push_back(imageFilterStepDescription(step));
    }
    image_filter_pipeline_label_->setText(tr("处理链（%1/12）：%2")
        .arg(static_cast<int>(image_filter_pipeline_.size()))
        .arg(descriptions.join(tr(" → "))));
}

void CameraMainWindow::applySelectedImageFilter()
{
    if (!currentVisibleFrame().IsValid()) {
        QMessageBox::information(this, tr("图像处理"), tr("请先打开图像或连接相机。"));
        return;
    }
    if (image_filter_pipeline_.size() >= 12) {
        QMessageBox::information(this, tr("图像处理"), tr("处理链最多包含 12 个步骤，请先撤销或恢复原图。"));
        return;
    }
    const ImageFilterKind kind = static_cast<ImageFilterKind>(image_filter_combo_->currentData().toInt());
    image_filter_pipeline_.push_back({kind, image_filter_parameter_slider_->integerValue()});
    if (smart_count_cancel_token_) smart_count_cancel_token_->store(true);
    smart_target_result_ = {};
    ++image_generation_;
    updateImageFilterPipelineUi();
    updateSmartTargetUi();
    updateImagePresentation();
    statusBar()->showMessage(tr("已添加图像处理：%1").arg(imageFilterName(kind)), 3500);
}

void CameraMainWindow::undoImageFilter()
{
    if (image_filter_pipeline_.empty()) {
        statusBar()->showMessage(tr("当前没有可撤销的图像处理"), 2500);
        return;
    }
    image_filter_pipeline_.pop_back();
    if (smart_count_cancel_token_) smart_count_cancel_token_->store(true);
    smart_target_result_ = {};
    ++image_generation_;
    updateImageFilterPipelineUi();
    updateSmartTargetUi();
    updateImagePresentation();
    statusBar()->showMessage(tr("已撤销上一步图像处理"), 2500);
}

void CameraMainWindow::clearImageFilters()
{
    if (image_filter_pipeline_.empty()) {
        statusBar()->showMessage(tr("当前已经是未滤镜处理状态"), 2500);
        return;
    }
    image_filter_pipeline_.clear();
    if (smart_count_cancel_token_) smart_count_cancel_token_->store(true);
    smart_target_result_ = {};
    ++image_generation_;
    updateImageFilterPipelineUi();
    updateSmartTargetUi();
    updateImagePresentation();
    statusBar()->showMessage(tr("已清除处理链并恢复原图"), 3000);
}

void CameraMainWindow::updateImagePresentation()
{
    adjustments_.brightness = brightness_slider_ ? brightness_slider_->value() : 0;
    adjustments_.contrast = contrast_slider_ ? contrast_slider_->value() : 0;
    adjustments_.gamma_tenths = gamma_slider_ ? gamma_slider_->value() : 10;
    adjustments_.window_level = level_slider_ ? level_slider_->value() : 128;
    adjustments_.window_width = width_slider_ ? width_slider_->value() : 256;
    if (palette_combo_) {
        palette_ = PseudoColorMapper::PaletteAtIndex(palette_combo_->currentIndex());
    }
    if (histogram_channel_combo_) {
        histogram_channel_ = static_cast<HistogramChannel>(histogram_channel_combo_->currentIndex());
    }

    bool showing_fusion = fusion_enabled_ && !channels_.empty();
    if (!showing_fusion && current_source_identity_ == QStringLiteral("camera-live") &&
        !latest_camera_image_.isNull() && hasNeutralPresentation()) {
        presentLiveCameraImage();
        return;
    }
    ImageFrame presentation_source;
    if (showing_fusion) {
        FluorescenceFusionOptions fusion_options;
        fusion_options.blend_mode = fluorescence_blend_mode_;
        presentation_source = ChannelFusionEngine::Fuse(channels_, fusion_options);
        if (!presentation_source.IsValid()) {
            showing_fusion = false;
            presentation_source = sourceFrame();
        }
    } else {
        presentation_source = sourceFrame();
    }
    if (presentation_source.IsValid()) {
        ImageFrame adjusted = showing_fusion
            ? std::move(presentation_source) : ApplyAdjustments(presentation_source, adjustments_);
        ImageFrame filtered = ImageFilterProcessor::ApplyPipeline(adjusted, image_filter_pipeline_);
        display_frame_ = showing_fusion ? std::move(filtered) : PseudoColorMapper::Apply(filtered, palette_);
    } else {
        display_frame_ = {};
    }
    const QImage presentation_image = qImageFromFrame(display_frame_);
    canvas_->setImage(presentation_image);
    canvas_->setProperty("directCameraPreview", false);
    if (yolo_workspace_ &&
        (current_source_identity_ != QStringLiteral("camera-live") ||
         (function_tabs_ && function_tabs_->currentWidget() == yolo_workspace_))) {
        yolo_workspace_->setCurrentImage(
            presentation_image, current_source_, current_source_identity_);
    }
    rebuildOverlays();
    if (histogram_ &&
        (current_source_identity_ != QStringLiteral("camera-live") || histogram_->isVisible())) {
        const HistogramData data = ComputeHistogram(display_frame_, histogram_channel_);
        const QColor colors[] = {
            QColor(120, 190, 255), QColor(239, 68, 68), QColor(34, 197, 94), QColor(59, 130, 246)};
        histogram_->setHistogram(data, colors[static_cast<int>(histogram_channel_)]);
    }
}

ImageFrame CameraMainWindow::currentVisibleFrame() const
{
    return display_frame_.IsValid() ? display_frame_ : sourceFrame();
}

void CameraMainWindow::onCanvasPoints(CanvasTool tool, QVector<QPointF> points)
{
    std::optional<MeasurementReference> added_measurement;
    if (tool == CanvasTool::CameraRoi && points.size() == 2) {
        const QRectF bounds(points[0], points[1]);
        const QRectF normalized = bounds.normalized().intersected(
            QRectF(0.0, 0.0, latest_camera_image_.width(), latest_camera_image_.height()));
        camera_ui_updating_ = true;
        camera_roi_spins_[0]->setValue(std::max(0, static_cast<int>(std::floor(normalized.left()))));
        camera_roi_spins_[1]->setValue(std::max(0, static_cast<int>(std::floor(normalized.top()))));
        camera_roi_spins_[2]->setValue(std::max(1, static_cast<int>(std::ceil(normalized.width()))));
        camera_roi_spins_[3]->setValue(std::max(1, static_cast<int>(std::ceil(normalized.height()))));
        camera_ui_updating_ = false;
        restoreToolAfterCameraRoi(tr("ROI 已框选，正在应用…"));
        applyCameraRoiInputs();
        return;
    }
    if (ai_annotation_active_ && yolo_workspace_ &&
        (tool == CanvasTool::Rectangle || tool == CanvasTool::Polygon)) {
        ai_annotation_active_ = false;
        canvas_->setTool(CanvasTool::None);
        canvas_->setEdgeSnappingEnabled(edge_snap_check_ && edge_snap_check_->isChecked());
        yolo_workspace_->acceptCanvasAnnotation(tool, std::move(points));
        return;
    }
    if (tool == CanvasTool::SmartCountSample && points.size() == 2) {
        QRectF bounds(points[0], points[1]);
        bounds = bounds.normalized().intersected(
            QRectF(0.0, 0.0, currentVisibleFrame().width, currentVisibleFrame().height));
        if (bounds.width() < 6.0 || bounds.height() < 6.0) {
            statusBar()->showMessage(tr("框选区域太小，请完整框住一个目标"), 3500);
        } else {
            smart_target_samples_.push_back({
                static_cast<int>(std::floor(bounds.left())),
                static_cast<int>(std::floor(bounds.top())),
                std::max(6, static_cast<int>(std::ceil(bounds.width()))),
                std::max(6, static_cast<int>(std::ceil(bounds.height())))});
            smart_target_result_ = {};
            updateSmartTargetUi();
            rebuildOverlays();
            statusBar()->showMessage(tr("已框选 %1 个样本，可继续框选或开始自动计数")
                .arg(static_cast<int>(smart_target_samples_.size())), 4000);
        }
        canvas_->setTool(CanvasTool::SmartCountSample);
        return;
    }
    if (tool == CanvasTool::Calibration && points.size() == 2) {
        const double pixel_distance = QLineF(points[0], points[1]).length();
        if (!std::isfinite(pixel_distance) || pixel_distance < 1.0) {
            QMessageBox::warning(this, tr("标定失败"), tr("两个标定点距离太近，请重新选择。"));
            setMeasurementTool(CanvasTool::Calibration, tr("请重新选择距离更远的两个标定点"));
            return;
        }

        const MeasurementUnit initial_unit = CalibrationProfile::CalibrationUnitAtIndex(
            calibration_unit_combo_->currentIndex());
        CalibrationDialog dialog(
            pixel_distance, calibration_length_spin_->value(), initial_unit, this);
        dialog.setWindowTitle(tr("%1 物镜两点标定").arg(currentObjectiveLabel()));
        if (dialog.exec() != QDialog::Accepted) {
            canvas_->setTool(CanvasTool::None);
            statusBar()->showMessage(tr("已取消标定，原标定未改变"), 3500);
            return;
        }

        const CalibrationProfile candidate = dialog.profile();
        if (!candidate.IsCalibrated()) {
            QMessageBox::warning(this, tr("标定失败"), tr("真实长度必须大于零。"));
            setMeasurementTool(CanvasTool::Calibration, tr("请重新选择标定线的两个端点"));
            return;
        }

        calibration_length_spin_->setValue(dialog.realLength());
        calibration_unit_combo_->setCurrentIndex(
            dialog.unit() == MeasurementUnit::Millimeters ? 1 : 0);
        calibration_ = candidate;
        if (selected_objective_index_ >= 0 &&
            selected_objective_index_ < static_cast<int>(objective_calibrations_.size())) {
            objective_calibrations_[static_cast<std::size_t>(selected_objective_index_)] = calibration_;
        }
        updateCalibrationUi();
        saveObjectiveCalibrationMemory();
        statusBar()->showMessage(
            tr("%1 物镜标定已完成（%2 px），已有测量结果已重新换算")
                .arg(currentObjectiveLabel())
                .arg(pixel_distance, 0, 'f', 2),
            5000);
    } else if (tool == CanvasTool::ProfileLine && points.size() == 2) {
        profile_line_points_ = points;
        const ImageFrame frame = currentVisibleFrame();
        auto* dialog = new ProfileAnalysisDialog(
            frame,
            imagePoint(points.at(0)),
            imagePoint(points.at(1)),
            calibration_,
            display_unit_,
            current_source_,
            this);
        dialog->show();
        rebuildOverlays();
        statusBar()->showMessage(tr("剖线测量已生成"), 4000);
    } else if (tool == CanvasTool::Length && points.size() == 2) {
        added_measurement = MeasurementReference{MeasurementKind::Length, measurements_.LengthCount()};
        measurements_.AddLength(
            MeasurementNameFormatter::NextDefaultName(MeasurementKind::Length, measurements_),
            imagePoint(points[0]), imagePoint(points[1]));
    } else if (tool == CanvasTool::Angle && points.size() == 3) {
        added_measurement = MeasurementReference{MeasurementKind::Angle, measurements_.AngleCount()};
        measurements_.AddAngle(
            MeasurementNameFormatter::NextDefaultName(MeasurementKind::Angle, measurements_),
            imagePoint(points[0]), imagePoint(points[1]), imagePoint(points[2]));
    } else if (tool == CanvasTool::Rectangle && points.size() == 2) {
        added_measurement = MeasurementReference{MeasurementKind::RectangleArea, measurements_.RectangleCount()};
        measurements_.AddRectangleArea(
            MeasurementNameFormatter::NextDefaultName(MeasurementKind::RectangleArea, measurements_),
            imagePoint(points[0]), imagePoint(points[1]));
    } else if (tool == CanvasTool::Polygon && points.size() >= 3) {
        added_measurement = MeasurementReference{MeasurementKind::PolygonArea, measurements_.PolygonCount()};
        std::vector<ImagePoint> polygon;
        polygon.reserve(static_cast<std::size_t>(points.size()));
        for (const QPointF& point : points) {
            polygon.push_back(imagePoint(point));
        }
        measurements_.AddPolygonArea(
            MeasurementNameFormatter::NextDefaultName(MeasurementKind::PolygonArea, measurements_),
            std::move(polygon));
    } else if (tool == CanvasTool::Point && points.size() == 1) {
        added_measurement = MeasurementReference{MeasurementKind::Point, measurements_.PointCount()};
        measurements_.AddPoint(
            MeasurementNameFormatter::NextDefaultName(MeasurementKind::Point, measurements_),
            imagePoint(points[0]));
    } else if (tool == CanvasTool::Polyline && points.size() >= 2) {
        added_measurement = MeasurementReference{MeasurementKind::Polyline, measurements_.PolylineCount()};
        std::vector<ImagePoint> polyline;
        polyline.reserve(static_cast<std::size_t>(points.size()));
        for (const QPointF& point : points) polyline.push_back(imagePoint(point));
        measurements_.AddPolyline(
            MeasurementNameFormatter::NextDefaultName(MeasurementKind::Polyline, measurements_),
            std::move(polyline));
    } else if (tool == CanvasTool::Circle && points.size() == 2) {
        added_measurement = MeasurementReference{MeasurementKind::Circle, measurements_.CircleCount()};
        measurements_.AddCircle(
            MeasurementNameFormatter::NextDefaultName(MeasurementKind::Circle, measurements_),
            imagePoint(points[0]), imagePoint(points[1]));
    } else if (tool == CanvasTool::Ellipse && points.size() == 2) {
        added_measurement = MeasurementReference{MeasurementKind::Ellipse, measurements_.EllipseCount()};
        measurements_.AddEllipse(
            MeasurementNameFormatter::NextDefaultName(MeasurementKind::Ellipse, measurements_),
            imagePoint(points[0]), imagePoint(points[1]));
    }
    const bool repeatable_measurement = added_measurement.has_value() ||
        tool == CanvasTool::ProfileLine;
    if (!repeatable_measurement) {
        canvas_->setTool(CanvasTool::None);
    }
    if (added_measurement) {
        measurements_.SetStyle(*added_measurement, global_measurement_style_);
    }
    updateMeasurementList();
    if (added_measurement) {
        measurement_list_->setCurrentRow(static_cast<int>(measurements_.FlatIndexOf(*added_measurement)));
        statusBar()->showMessage(tr("测量已添加，可继续使用当前工具测量"), 3000);
    }
}

void CameraMainWindow::setMeasurementTool(CanvasTool tool, const QString& hint)
{
    if (!currentVisibleFrame().IsValid()) {
        canvas_->setTool(CanvasTool::None);
        QMessageBox::information(this, tr("测量"), tr("请先打开图像或连接相机。"));
        return;
    }
    ai_annotation_active_ = false;
    canvas_->setEdgeSnappingEnabled(edge_snap_check_ && edge_snap_check_->isChecked());
    canvas_->setTool(tool);
    statusBar()->showMessage(hint);
}

void CameraMainWindow::updateMeasurementList()
{
    const int previous = measurement_list_->currentRow();
    measurement_list_->clear();
    const std::vector<std::wstring> lines = MeasurementFormatter::FormatCollection(
        measurements_, calibration_, display_unit_);
    for (const std::wstring& line : lines) {
        measurement_list_->addItem(QString::fromStdWString(line));
    }
    if (measurement_count_label_) {
        measurement_count_label_->setText(tr("测量结果（%1）").arg(measurement_list_->count()));
    }
    if (measurement_list_->count() > 0) {
        measurement_list_->setCurrentRow(std::clamp(previous, 0, measurement_list_->count() - 1));
    }
    updateMeasurementStyleUi();
    rebuildOverlays();
}

void CameraMainWindow::updateCalibrationUi()
{
    if (!calibration_label_) {
        return;
    }
    calibration_label_->setText(calibration_.IsCalibrated()
        ? tr("%1：%2 µm / px")
              .arg(currentObjectiveLabel())
              .arg(calibration_.MicronsPerPixel(), 0, 'g', 10)
        : tr("%1：未标定").arg(currentObjectiveLabel()));
}

void CameraMainWindow::selectObjective(int index, bool rememberSelection)
{
    if (objective_labels_.empty() || objective_calibrations_.empty()) {
        return;
    }
    selected_objective_index_ = std::clamp(
        index, 0, static_cast<int>(objective_labels_.size()) - 1);
    calibration_ = objective_calibrations_[static_cast<std::size_t>(selected_objective_index_)];
    updateCalibrationUi();
    updateMeasurementList();
    if (rememberSelection) {
        saveObjectiveCalibrationMemory();
        statusBar()->showMessage(calibration_.IsCalibrated()
            ? tr("已切换到 %1 物镜，并恢复该倍率标定").arg(currentObjectiveLabel())
            : tr("已切换到 %1 物镜；该倍率尚未标定").arg(currentObjectiveLabel()),
            3500);
    }
}

void CameraMainWindow::refreshObjectiveControls()
{
    if (!objective_combo_ || objective_labels_.empty()) return;
    selected_objective_index_ = std::clamp(
        selected_objective_index_, 0, static_cast<int>(objective_labels_.size()) - 1);
    const QSignalBlocker blocker(objective_combo_);
    objective_combo_->clear();
    for (std::size_t index = 0; index < objective_labels_.size(); ++index) {
        const QString label = QString::fromStdWString(objective_labels_[index]);
        const CalibrationProfile profile = index < objective_calibrations_.size()
            ? objective_calibrations_[index] : CalibrationProfile::Uncalibrated();
        objective_combo_->addItem(profile.IsCalibrated()
            ? tr("%1  ·  %2 µm/px").arg(label).arg(profile.MicronsPerPixel(), 0, 'g', 8)
            : tr("%1  ·  未标定").arg(label));
    }
    objective_combo_->setCurrentIndex(selected_objective_index_);
    updateCalibrationUi();
    updateMeasurementList();
}

void CameraMainWindow::loadObjectiveCalibrationMemory()
{
    QSettings settings;
    ObjectiveCalibrationState state = ObjectiveCalibrationSettings::Load(settings);
    objective_labels_ = std::move(state.labels);
    objective_calibrations_ = std::move(state.calibrations);
    selected_objective_index_ = state.selected_index;
    if (objective_calibrations_.size() < objective_labels_.size()) {
        objective_calibrations_.resize(
            objective_labels_.size(), CalibrationProfile::Uncalibrated());
    }
    selected_objective_index_ = std::clamp(
        selected_objective_index_, 0, static_cast<int>(objective_labels_.size()) - 1);
    calibration_ = objective_calibrations_[static_cast<std::size_t>(selected_objective_index_)];
    refreshObjectiveControls();
}

void CameraMainWindow::saveObjectiveCalibrationMemory() const
{
    QSettings settings;
    ObjectiveCalibrationSettings::Save(settings, ObjectiveCalibrationState{
        objective_labels_, objective_calibrations_, selected_objective_index_});
}

QString CameraMainWindow::currentObjectiveLabel() const
{
    if (selected_objective_index_ < 0 ||
        selected_objective_index_ >= static_cast<int>(objective_labels_.size())) {
        return tr("未知倍率");
    }
    return QString::fromStdWString(
        objective_labels_[static_cast<std::size_t>(selected_objective_index_)]);
}

QVector<CanvasOverlay> CameraMainWindow::measurementOverlays() const
{
    QVector<CanvasOverlay> overlays;
    const int selected = measurement_list_ ? measurement_list_->currentRow() : -1;
    auto append = [this, &overlays, selected](CanvasOverlay overlay) {
        overlay.color = QColor(global_measurement_style_.red,
            global_measurement_style_.green, global_measurement_style_.blue);
        overlay.highlighted = overlays.size() == selected;
        overlay.editable = true;
        overlay.source_index = overlays.size();
        overlays.push_back(std::move(overlay));
    };
    for (const LengthMeasurement& measurement : measurements_.Lengths()) {
        const MeasurementResult result = measurement.Evaluate(calibration_, display_unit_);
        append({CanvasTool::Length,
            {{measurement.First().x, measurement.First().y}, {measurement.Second().x, measurement.Second().y}},
            QString::fromStdWString(MeasurementFormatter::FormatResultLine(result)), QColor(76, 201, 240)});
    }
    for (const AngleMeasurement& measurement : measurements_.Angles()) {
        append({CanvasTool::Angle,
            {{measurement.First().x, measurement.First().y}, {measurement.Vertex().x, measurement.Vertex().y},
                {measurement.Second().x, measurement.Second().y}},
            QString::fromStdWString(MeasurementFormatter::FormatLine(measurement)), QColor(251, 146, 60)});
    }
    for (const RectangleAreaMeasurement& measurement : measurements_.Rectangles()) {
        append({CanvasTool::Rectangle,
            {{measurement.First().x, measurement.First().y}, {measurement.Second().x, measurement.Second().y}},
            QString::fromStdWString(MeasurementFormatter::FormatLine(measurement, calibration_, display_unit_)),
            QColor(74, 222, 128)});
    }
    for (const PolygonAreaMeasurement& measurement : measurements_.Polygons()) {
        QVector<QPointF> points;
        for (const ImagePoint& point : measurement.Points()) {
            points.push_back({point.x, point.y});
        }
        append({CanvasTool::Polygon, points,
            QString::fromStdWString(MeasurementFormatter::FormatLine(measurement, calibration_, display_unit_)),
            QColor(192, 132, 252)});
    }
    for (const PointMeasurement& measurement : measurements_.Points()) {
        append({CanvasTool::Point, {{measurement.Point().x, measurement.Point().y}},
            QString::fromStdWString(MeasurementFormatter::FormatLine(measurement, calibration_, display_unit_)),
            QColor(250, 204, 21)});
    }
    for (const PolylineMeasurement& measurement : measurements_.Polylines()) {
        QVector<QPointF> points;
        for (const ImagePoint& point : measurement.Points()) points.push_back({point.x, point.y});
        append({CanvasTool::Polyline, points,
            QString::fromStdWString(MeasurementFormatter::FormatLine(measurement, calibration_, display_unit_)),
            QColor(45, 212, 191)});
    }
    for (const CircleMeasurement& measurement : measurements_.Circles()) {
        append({CanvasTool::Circle,
            {{measurement.Center().x, measurement.Center().y}, {measurement.Edge().x, measurement.Edge().y}},
            QString::fromStdWString(MeasurementFormatter::FormatLine(measurement, calibration_, display_unit_)),
            QColor(244, 114, 182)});
    }
    for (const EllipseMeasurement& measurement : measurements_.Ellipses()) {
        append({CanvasTool::Ellipse,
            {{measurement.First().x, measurement.First().y}, {measurement.Second().x, measurement.Second().y}},
            QString::fromStdWString(MeasurementFormatter::FormatLine(measurement, calibration_, display_unit_)),
            QColor(129, 140, 248)});
    }
    if (profile_line_points_.size() == 2) {
        overlays.push_back({CanvasTool::ProfileLine, profile_line_points_, tr("剖线"), QColor(255, 202, 58)});
    }
    return overlays;
}

QRectF CameraMainWindow::measurementBounds(MeasurementReference reference) const
{
    QVector<QPointF> points;
    switch (reference.kind) {
    case MeasurementKind::Length: {
        if (reference.index >= measurements_.Lengths().size()) return {};
        const auto& item = measurements_.Lengths()[reference.index];
        points = {{item.First().x, item.First().y}, {item.Second().x, item.Second().y}};
        break;
    }
    case MeasurementKind::Angle: {
        if (reference.index >= measurements_.Angles().size()) return {};
        const auto& item = measurements_.Angles()[reference.index];
        points = {{item.First().x, item.First().y}, {item.Vertex().x, item.Vertex().y},
            {item.Second().x, item.Second().y}};
        break;
    }
    case MeasurementKind::RectangleArea: {
        if (reference.index >= measurements_.Rectangles().size()) return {};
        const auto& item = measurements_.Rectangles()[reference.index];
        points = {{item.First().x, item.First().y}, {item.Second().x, item.Second().y}};
        break;
    }
    case MeasurementKind::PolygonArea: {
        if (reference.index >= measurements_.Polygons().size()) return {};
        for (const ImagePoint& point : measurements_.Polygons()[reference.index].Points())
            points.push_back({point.x, point.y});
        break;
    }
    case MeasurementKind::Point: {
        if (reference.index >= measurements_.Points().size()) return {};
        const ImagePoint point = measurements_.Points()[reference.index].Point();
        points = {{point.x, point.y}};
        break;
    }
    case MeasurementKind::Polyline: {
        if (reference.index >= measurements_.Polylines().size()) return {};
        for (const ImagePoint& point : measurements_.Polylines()[reference.index].Points())
            points.push_back({point.x, point.y});
        break;
    }
    case MeasurementKind::Circle: {
        if (reference.index >= measurements_.Circles().size()) return {};
        const auto& item = measurements_.Circles()[reference.index];
        const double radius = item.PixelRadius();
        return QRectF(item.Center().x - radius, item.Center().y - radius,
            radius * 2.0, radius * 2.0);
    }
    case MeasurementKind::Ellipse: {
        if (reference.index >= measurements_.Ellipses().size()) return {};
        const auto& item = measurements_.Ellipses()[reference.index];
        return QRectF(QPointF(item.First().x, item.First().y),
            QPointF(item.Second().x, item.Second().y)).normalized();
    }
    case MeasurementKind::None:
        return {};
    }
    if (points.isEmpty()) return {};
    double left = points.first().x();
    double right = left;
    double top = points.first().y();
    double bottom = top;
    for (const QPointF& point : points) {
        left = std::min(left, point.x());
        right = std::max(right, point.x());
        top = std::min(top, point.y());
        bottom = std::max(bottom, point.y());
    }
    constexpr double minimumSize = 12.0;
    if (right - left < minimumSize) {
        const double center = (left + right) / 2.0;
        left = center - minimumSize / 2.0;
        right = center + minimumSize / 2.0;
    }
    if (bottom - top < minimumSize) {
        const double center = (top + bottom) / 2.0;
        top = center - minimumSize / 2.0;
        bottom = center + minimumSize / 2.0;
    }
    return QRectF(QPointF(left, top), QPointF(right, bottom));
}

void CameraMainWindow::focusSelectedMeasurement()
{
    const int row = measurement_list_ ? measurement_list_->currentRow() : -1;
    const auto reference = row >= 0
        ? measurements_.AtFlatIndex(static_cast<std::size_t>(row)) : std::nullopt;
    if (!reference) return;
    if (canvas_->focusOnImageRect(measurementBounds(*reference))) {
        statusBar()->showMessage(tr("已定位到测量项：%1")
            .arg(QString::fromStdWString(measurements_.Name(*reference))), 3000);
    }
}

void CameraMainWindow::enterMeasurementSelectionMode()
{
    ai_annotation_active_ = false;
    canvas_->setTool(CanvasTool::None);
    canvas_->setFocus(Qt::OtherFocusReason);
    statusBar()->showMessage(
        tr("选择模式：单击选择测量，拖动控制点修改形状，拖动对象整体移动"), 5000);
}

void CameraMainWindow::renameSelectedMeasurement()
{
    const int row = measurement_list_ ? measurement_list_->currentRow() : -1;
    const std::optional<MeasurementReference> reference = row >= 0
        ? measurements_.AtFlatIndex(static_cast<std::size_t>(row)) : std::nullopt;
    if (!reference) {
        statusBar()->showMessage(tr("请先选择要重命名的测量结果"), 2500);
        return;
    }
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, tr("重命名测量"), tr("名称"), QLineEdit::Normal,
        QString::fromStdWString(measurements_.Name(*reference)), &accepted).trimmed();
    if (accepted && !name.isEmpty() && measurements_.SetName(*reference, name.toStdWString())) {
        updateMeasurementList();
        statusBar()->showMessage(tr("测量名称已更新：%1").arg(name), 2500);
    }
}

void CameraMainWindow::chooseSelectedMeasurementColor()
{
    const QColor current(global_measurement_style_.red,
        global_measurement_style_.green, global_measurement_style_.blue);
    const QColor selected = QColorDialog::getColor(
        current, this, tr("设置全局测量颜色"), QColorDialog::DontUseNativeDialog);
    if (!selected.isValid()) return;
    global_measurement_style_ = {
        static_cast<std::uint8_t>(selected.red()),
        static_cast<std::uint8_t>(selected.green()),
        static_cast<std::uint8_t>(selected.blue())};
    applyGlobalMeasurementColor();
    saveMeasurementPreferences();
    updateMeasurementStyleUi();
    rebuildOverlays();
    statusBar()->showMessage(tr("全局测量颜色已更新，并应用到全部测量"), 3000);
}

void CameraMainWindow::resetSelectedMeasurementColor()
{
    global_measurement_style_ = {76, 201, 240};
    applyGlobalMeasurementColor();
    saveMeasurementPreferences();
    updateMeasurementStyleUi();
    rebuildOverlays();
    statusBar()->showMessage(tr("已恢复系统默认测量颜色"), 2500);
}

void CameraMainWindow::updateMeasurementStyleUi()
{
    if (!measurement_color_button_ && !measurement_color_action_) return;
    if (measurement_color_button_) measurement_color_button_->setEnabled(true);
    if (measurement_reset_color_button_) measurement_reset_color_button_->setEnabled(true);
    const QColor color(global_measurement_style_.red,
        global_measurement_style_.green, global_measurement_style_.blue);
    QPixmap swatch(30, 30);
    swatch.fill(color);
    const QString tool_tip = tr("设置全局测量颜色（%1），适用于现有及后续测量")
        .arg(color.name(QColor::HexRgb).toUpper());
    if (measurement_color_button_) {
        measurement_color_button_->setIcon(QIcon(swatch));
        measurement_color_button_->setIconSize(swatch.size());
        measurement_color_button_->setToolTip(tool_tip);
    }
    if (measurement_color_action_) {
        measurement_color_action_->setIcon(QIcon(swatch));
        measurement_color_action_->setToolTip(tool_tip);
        if (measurement_toolbar_) {
            if (auto* toolbar_button = qobject_cast<QToolButton*>(
                    measurement_toolbar_->widgetForAction(measurement_color_action_))) {
                toolbar_button->setIconSize(swatch.size());
            }
        }
    }
}

void CameraMainWindow::applyGlobalMeasurementColor()
{
    measurements_.SetStyles(std::vector<MeasurementOverlayStyle>(
        measurements_.Count(), global_measurement_style_));
}

void CameraMainWindow::loadMeasurementPreferences()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("Measurement"));
    const QColor color = settings.value(
        QStringLiteral("globalColor"), QColor(76, 201, 240)).value<QColor>();
    settings.endGroup();
    if (color.isValid()) {
        global_measurement_style_ = {
            static_cast<std::uint8_t>(color.red()),
            static_cast<std::uint8_t>(color.green()),
            static_cast<std::uint8_t>(color.blue())};
    }
    applyGlobalMeasurementColor();
    updateMeasurementStyleUi();
}

void CameraMainWindow::saveMeasurementPreferences() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("Measurement"));
    settings.setValue(QStringLiteral("globalColor"), QColor(
        global_measurement_style_.red,
        global_measurement_style_.green,
        global_measurement_style_.blue));
    settings.endGroup();
    settings.sync();
}

QVector<CanvasOverlay> CameraMainWindow::smartTargetOverlays() const
{
    QVector<CanvasOverlay> overlays;
    overlays.reserve(static_cast<int>(smart_target_samples_.size() + smart_target_result_.matches.size()));
    for (std::size_t index = 0; index < smart_target_samples_.size(); ++index) {
        const SmartTargetRegion& region = smart_target_samples_[index];
        overlays.push_back({CanvasTool::SmartCountSample,
            {{static_cast<double>(region.x), static_cast<double>(region.y)},
             {static_cast<double>(region.x + region.width), static_cast<double>(region.y + region.height)}},
            tr("样本 %1").arg(static_cast<int>(index + 1)), QColor(250, 176, 5)});
    }
    const int selected = smart_result_list_ ? smart_result_list_->currentRow() : -1;
    for (std::size_t index = 0; index < smart_target_result_.matches.size(); ++index) {
        const SmartTargetMatch& match = smart_target_result_.matches[index];
        const SmartTargetRegion& region = match.region;
        CanvasOverlay overlay{CanvasTool::SmartCountResult,
            {{static_cast<double>(region.x), static_cast<double>(region.y)},
             {static_cast<double>(region.x + region.width), static_cast<double>(region.y + region.height)}},
            tr("#%1 · %2%").arg(static_cast<int>(index + 1)).arg(match.confidence * 100.0, 0, 'f', 1),
            QColor(34, 211, 153)};
        overlay.highlighted = static_cast<int>(index) == selected;
        overlays.push_back(std::move(overlay));
    }
    return overlays;
}

void CameraMainWindow::rebuildOverlays()
{
    QVector<CanvasOverlay> overlays = measurementOverlays();
    overlays += smartTargetOverlays();
    overlays += ai_overlays_;
    canvas_->setOverlays(std::move(overlays));
}

void CameraMainWindow::clearMeasurements()
{
    measurements_.Clear();
    profile_line_points_.clear();
    updateMeasurementList();
    statusBar()->showMessage(tr("测量结果已清空"), 3000);
}

void CameraMainWindow::updateSmartTargetUi()
{
    if (!smart_sample_label_) return;
    smart_sample_label_->setText(tr("已框选 %1 个样本")
        .arg(static_cast<int>(smart_target_samples_.size())));

    const int previous_row = smart_result_list_ ? smart_result_list_->currentRow() : -1;
    if (smart_result_list_) {
        smart_result_list_->clear();
        for (std::size_t index = 0; index < smart_target_result_.matches.size(); ++index) {
            const SmartTargetMatch& match = smart_target_result_.matches[index];
            smart_result_list_->addItem(tr("目标 %1 · 相似度 %2% · 位置 (%3, %4)")
                .arg(static_cast<int>(index + 1))
                .arg(match.confidence * 100.0, 0, 'f', 1)
                .arg(match.region.x)
                .arg(match.region.y));
        }
        if (smart_result_list_->count() > 0) {
            smart_result_list_->setCurrentRow(std::clamp(previous_row, 0, smart_result_list_->count() - 1));
        }
    }

    if (smart_result_label_) {
        if (smart_count_running_) {
            smart_result_label_->setText(tr("正在分析整幅图像…"));
        } else if (smart_target_result_.succeeded) {
            smart_result_label_->setText(tr("识别结果：%1 个目标 · 用时 %2 ms")
                .arg(static_cast<int>(smart_target_result_.matches.size()))
                .arg(smart_target_result_.elapsed_milliseconds, 0, 'f', 0));
        } else if (smart_target_result_.canceled) {
            smart_result_label_->setText(tr("识别已取消"));
        } else if (!SmartTargetCounter::IsAvailable()) {
            smart_result_label_->setText(tr("当前构建未启用 OpenCV，智能计数不可用"));
        } else {
            smart_result_label_->setText(tr("识别结果：尚未运行"));
        }
    }

    if (smart_count_button_) {
        smart_count_button_->setText(smart_count_running_ ? tr("取消查找") : tr("自动查找并计数"));
        smart_count_button_->setEnabled(smart_count_running_ ||
            (SmartTargetCounter::IsAvailable() && !smart_target_samples_.empty() && currentVisibleFrame().IsValid()));
    }
    if (smart_select_button_) smart_select_button_->setEnabled(!smart_count_running_);
    if (smart_similarity_slider_) smart_similarity_slider_->setEnabled(!smart_count_running_);
    if (smart_scale_tolerance_slider_) smart_scale_tolerance_slider_->setEnabled(!smart_count_running_);
    if (smart_count_progress_) smart_count_progress_->setVisible(smart_count_running_);
}

void CameraMainWindow::startSmartTargetSampleSelection()
{
    if (!currentVisibleFrame().IsValid()) {
        setMeasurementTool(CanvasTool::SmartCountSample,
            tr("请选择目标外接矩形的两个对角点"));
        return;
    }
    smart_count_session_active_ = true;
    setMeasurementTool(CanvasTool::SmartCountSample,
        tr("请选择目标外接矩形的两个对角点；可连续框选多个，按 Esc 结束"));
}

void CameraMainWindow::runSmartTargetCounting()
{
    if (smart_count_running_) {
        if (smart_count_cancel_token_) smart_count_cancel_token_->store(true);
        smart_count_button_->setText(tr("正在取消…"));
        smart_count_button_->setEnabled(false);
        statusBar()->showMessage(tr("正在取消智能计数…"));
        return;
    }
    if (!SmartTargetCounter::IsAvailable()) {
        QMessageBox::information(this, tr("智能目标计数"), tr("请使用启用了 OpenCV 的版本。"));
        return;
    }
    const ImageFrame frame = currentVisibleFrame();
    if (!frame.IsValid()) {
        QMessageBox::information(this, tr("智能目标计数"), tr("请先打开图像或连接相机。"));
        return;
    }
    if (smart_target_samples_.empty()) {
        QMessageBox::information(this, tr("智能目标计数"), tr("请先框选至少一个典型目标。"));
        return;
    }

    canvas_->setTool(CanvasTool::None);
    SmartTargetCountOptions options;
    options.similarity_threshold = smart_similarity_slider_->value();
    options.scale_tolerance = smart_scale_tolerance_slider_->value() / 100.0;
    options.scale_steps = options.scale_tolerance > 0.0 ? 5 : 1;
    const std::vector<SmartTargetRegion> samples = smart_target_samples_;
    const quint64 generation = image_generation_;
    smart_count_cancel_token_ = std::make_shared<std::atomic_bool>(false);
    const auto cancel_token = smart_count_cancel_token_;
    smart_target_result_ = {};
    smart_count_running_ = true;
    smart_count_progress_->setValue(0);
    updateSmartTargetUi();
    rebuildOverlays();
    statusBar()->showMessage(tr("正在后台查找相似目标…"));

    auto* watcher = new QFutureWatcher<SmartTargetCountResult>(this);
    connect(watcher, &QFutureWatcher<SmartTargetCountResult>::finished, this,
        [this, watcher, cancel_token, generation] {
            SmartTargetCountResult result = watcher->result();
            watcher->deleteLater();
            smart_count_running_ = false;
            smart_count_cancel_token_.reset();
            if (generation != image_generation_) {
                smart_target_result_ = {};
                updateSmartTargetUi();
                rebuildOverlays();
                statusBar()->showMessage(tr("图像已变化，已丢弃旧的智能计数结果"), 4000);
                return;
            }
            smart_target_result_ = std::move(result);
            updateSmartTargetUi();
            rebuildOverlays();
            if (smart_target_result_.succeeded) {
                statusBar()->showMessage(tr("智能计数完成：共找到 %1 个目标")
                    .arg(static_cast<int>(smart_target_result_.matches.size())), 6000);
            } else if (smart_target_result_.canceled) {
                statusBar()->showMessage(tr("智能计数已取消"), 3000);
            } else {
                QMessageBox::warning(this, tr("智能目标计数"),
                    QString::fromStdWString(smart_target_result_.message));
            }
        });
    QPointer<QProgressBar> progress = smart_count_progress_;
    watcher->setFuture(QtConcurrent::run([frame, samples, options, cancel_token, progress] {
        const auto report_progress = [progress](int value) {
            if (!progress) return;
            QMetaObject::invokeMethod(progress, [progress, value] {
                if (progress) progress->setValue(std::clamp(value, 0, 100));
            }, Qt::QueuedConnection);
        };
        return SmartTargetCounter::Count(frame, samples, options, cancel_token.get(), report_progress);
    }));
}

void CameraMainWindow::clearSmartTargetCounting()
{
    if (smart_count_cancel_token_) smart_count_cancel_token_->store(true);
    smart_count_session_active_ = false;
    smart_target_samples_.clear();
    smart_target_result_ = {};
    if (canvas_->tool() == CanvasTool::SmartCountSample) canvas_->setTool(CanvasTool::None);
    updateSmartTargetUi();
    rebuildOverlays();
    statusBar()->showMessage(tr("智能计数样本和结果已清除；实时预览已恢复"), 3500);
}

void CameraMainWindow::deleteSelectedMeasurement()
{
    const int row = measurement_list_->currentRow();
    if (row >= 0 && measurements_.EraseAtFlatIndex(static_cast<std::size_t>(row))) {
        updateMeasurementList();
    }
}

void CameraMainWindow::exportMeasurements()
{
    if (measurements_.Empty()) {
        QMessageBox::information(this, tr("导出测量"), tr("当前没有测量结果。"));
        return;
    }
    QString file_name = QFileDialog::getSaveFileName(this, tr("导出测量 CSV"),
        QStringLiteral("measurements.csv"), tr("CSV (*.csv)"));
    if (file_name.isEmpty()) {
        return;
    }
    if (QFileInfo(file_name).suffix().isEmpty()) {
        file_name += QStringLiteral(".csv");
    }
    std::wstring error;
    if (!MeasurementCsvExporter::Save(
            std::filesystem::path(file_name.toStdWString()), measurements_, calibration_, display_unit_,
            currentObjectiveLabel().toStdWString(), error)) {
        QMessageBox::warning(this, tr("导出失败"), errorText(error));
        return;
    }
    statusBar()->showMessage(tr("测量数据已导出：%1").arg(file_name), 5000);
}

void CameraMainWindow::show3DView()
{
    const ImageFrame frame = currentVisibleFrame();
    if (!frame.IsValid()) {
        QMessageBox::information(this, tr("3D 高度图"), tr("请先打开图像或连接相机。"));
        return;
    }
    auto* dialog = new ImageSurface3DDialog(qImageFromFrame(frame), current_source_, this);
    dialog->show();
    statusBar()->showMessage(tr("已打开图像 3D 高度图"), 3000);
}

void CameraMainWindow::startProfileMeasurement()
{
    setMeasurementTool(CanvasTool::ProfileLine, tr("请在图像上选择剖线的两个端点"));
}

void CameraMainWindow::showPointCloudWorkspace()
{
    auto* dialog = new PointCloudDialog(this);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    statusBar()->showMessage(tr("已打开 3D 点云工作台"), 3000);
}

void CameraMainWindow::refreshFluorescencePresetList(int selectedRow)
{
    if (!fluorescence_preset_list_) return;
    if (selectedRow < 0) selectedRow = fluorescence_preset_list_->currentRow();
    const QSignalBlocker blocker(fluorescence_preset_list_);
    fluorescence_preset_list_->clear();
    for (std::size_t index = 0; index < fluorescence_capture_presets_.size(); ++index) {
        const FluorescenceCapturePreset& preset = fluorescence_capture_presets_[index];
        auto* item = new QListWidgetItem(tr("%1. %2  ·  %3 ms")
            .arg(index + 1)
            .arg(QString::fromStdWString(preset.dye_name))
            .arg(preset.exposure_ms, 0, 'f', 2));
        item->setForeground(QColor(preset.color.r, preset.color.g, preset.color.b));
        fluorescence_preset_list_->addItem(item);
    }
    if (!fluorescence_capture_presets_.empty()) {
        fluorescence_preset_list_->setCurrentRow(std::clamp(
            selectedRow, 0, static_cast<int>(fluorescence_capture_presets_.size()) - 1));
    }
    updateFluorescencePresetEditor();
    updateFluorescenceCaptureUi();
}

void CameraMainWindow::updateFluorescencePresetEditor()
{
    if (!fluorescence_preset_list_ || !fluorescence_preset_exposure_spin_ ||
        !fluorescence_preset_color_button_ || !dye_combo_) return;
    const int row = fluorescence_preset_list_->currentRow();
    if (row >= 0 && row < static_cast<int>(fluorescence_capture_presets_.size())) {
        const FluorescenceCapturePreset& preset =
            fluorescence_capture_presets_[static_cast<std::size_t>(row)];
        fluorescence_preset_exposure_spin_->setValue(preset.exposure_ms);
        fluorescence_preset_editor_color_ = preset.color;
        for (std::size_t index = 0; index < dyes_.size(); ++index) {
            if (dyes_[index].name == preset.dye_name) {
                const QSignalBlocker blocker(dye_combo_);
                dye_combo_->setCurrentIndex(static_cast<int>(index));
                break;
            }
        }
    } else if (dye_combo_->currentIndex() >= 0 &&
        dye_combo_->currentIndex() < static_cast<int>(dyes_.size())) {
        fluorescence_preset_editor_color_ =
            dyes_[static_cast<std::size_t>(dye_combo_->currentIndex())].color;
    }
    const QColor color(fluorescence_preset_editor_color_.r,
        fluorescence_preset_editor_color_.g, fluorescence_preset_editor_color_.b);
    QPixmap swatch(22, 22);
    swatch.fill(color);
    fluorescence_preset_color_button_->setIcon(QIcon(swatch));
    fluorescence_preset_color_button_->setIconSize(swatch.size());
    fluorescence_preset_color_button_->setText(
        tr("伪彩 %1").arg(color.name(QColor::HexRgb).toUpper()));
}

void CameraMainWindow::saveFluorescenceCapturePresets() const
{
    QSettings settings;
    FluorescenceCaptureSettings::Save(settings, fluorescence_capture_presets_);
}

void CameraMainWindow::startFluorescenceCaptureSequence()
{
    if (fluorescence_capture_presets_.empty()) {
        QMessageBox::information(this, tr("逐通道采集"), tr("请先建立至少一个采集预设。"));
        return;
    }
    if (!camera_open_) {
        QMessageBox::information(this, tr("逐通道采集"), tr("请先打开相机。"));
        return;
    }
    if (!channels_.empty() && QMessageBox::question(
            this, tr("逐通道采集"),
            tr("开始新序列会清除当前已采集的荧光通道，是否继续？")) != QMessageBox::Yes) {
        return;
    }
    channels_.clear();
    fusion_check_->setChecked(false);
    FluorescenceCaptureSequence::Start(
        fluorescence_capture_state_, fluorescence_capture_presets_.size());
    refreshFluorescenceChannelList();
    applyCurrentFluorescencePreset();
}

void CameraMainWindow::applyCurrentFluorescencePreset()
{
    if (!FluorescenceCaptureSequence::IsActive(fluorescence_capture_state_)) return;
    const FluorescenceCapturePreset& preset =
        fluorescence_capture_presets_[static_cast<std::size_t>(
            fluorescence_capture_state_.current_index)];
    if (!FluorescenceCaptureSequence::RequestExposure(
            fluorescence_capture_state_, preset.exposure_ms,
            latest_camera_sequence_)) return;
    {
        const QSignalBlocker blocker(exposure_spin_);
        exposure_spin_->setValue(preset.exposure_ms);
    }
    QMetaObject::invokeMethod(camera_worker_, "setExposure", Qt::QueuedConnection,
        Q_ARG(double, preset.exposure_ms));
    updateFluorescenceCaptureUi();
    statusBar()->showMessage(tr("已应用 %1 通道曝光 %2 ms；请切换对应滤光片并等待画面稳定")
        .arg(QString::fromStdWString(preset.dye_name))
        .arg(preset.exposure_ms, 0, 'f', 2), 6000);
}

void CameraMainWindow::captureCurrentFluorescencePreset()
{
    if (!FluorescenceCaptureSequence::IsActive(fluorescence_capture_state_)) return;
    if (!FluorescenceCaptureSequence::CanCapture(
            fluorescence_capture_state_, latest_camera_sequence_)) {
        statusBar()->showMessage(tr("正在等待曝光成功应用及新相机帧，请稍候。"), 2500);
        return;
    }
    ImageFrame frame = latestCameraFrame();
    if (!frame.IsValid()) {
        statusBar()->showMessage(tr("当前没有可采集的相机帧。"), 2500);
        return;
    }
    const FluorescenceCapturePreset& preset =
        fluorescence_capture_presets_[static_cast<std::size_t>(
            fluorescence_capture_state_.current_index)];
    FluorescenceChannel channel;
    channel.name = preset.dye_name;
    channel.frame = std::move(frame);
    channel.color = preset.color;
    channel.exposure_ms = preset.exposure_ms;
    channels_.push_back(std::move(channel));
    refreshFluorescenceChannelList(static_cast<int>(channels_.size()) - 1);
    const FluorescenceCaptureAdvance advance = FluorescenceCaptureSequence::Capture(
        fluorescence_capture_state_, latest_camera_sequence_);
    if (advance == FluorescenceCaptureAdvance::Complete) {
        fusion_check_->setChecked(true);
        showAllFluorescenceChannels();
        updateFluorescenceCaptureUi();
        statusBar()->showMessage(tr("全部荧光通道采集完成，已生成融合预览。"), 5000);
        return;
    }
    applyCurrentFluorescencePreset();
}

void CameraMainWindow::cancelFluorescenceCaptureSequence()
{
    FluorescenceCaptureSequence::Cancel(fluorescence_capture_state_);
    updateFluorescenceCaptureUi();
    statusBar()->showMessage(tr("逐通道采集已取消，已采集通道予以保留。"), 3500);
}

void CameraMainWindow::updateFluorescenceCaptureUi()
{
    if (!fluorescence_capture_status_label_) return;
    const bool active = FluorescenceCaptureSequence::IsActive(fluorescence_capture_state_);
    fluorescence_capture_start_button_->setEnabled(!active && !fluorescence_capture_presets_.empty());
    fluorescence_capture_button_->setEnabled(
        FluorescenceCaptureSequence::CanCapture(
            fluorescence_capture_state_, latest_camera_sequence_));
    fluorescence_capture_cancel_button_->setEnabled(active);
    fluorescence_preset_group_->setEnabled(!active);
    if (!active) {
        fluorescence_capture_status_label_->setText(
            fluorescence_capture_presets_.empty()
                ? tr("尚无采集预设。")
                : tr("已配置 %1 个通道。开始后将依次应用曝光、采集并融合。")
                    .arg(fluorescence_capture_presets_.size()));
        return;
    }
    const FluorescenceCapturePreset& preset =
        fluorescence_capture_presets_[static_cast<std::size_t>(
            fluorescence_capture_state_.current_index)];
    fluorescence_capture_status_label_->setText(
        tr("步骤 %1/%2：%3，曝光 %4 ms。请切换对应滤光片，待画面稳定后采集。")
            .arg(fluorescence_capture_state_.current_index + 1)
            .arg(fluorescence_capture_presets_.size())
            .arg(QString::fromStdWString(preset.dye_name))
            .arg(preset.exposure_ms, 0, 'f', 2));
}

void CameraMainWindow::addFluorescenceChannel()
{
    const ImageFrame frame = currentVisibleFrame();
    if (!frame.IsValid()) {
        QMessageBox::information(this, tr("荧光通道"), tr("当前没有可添加的图像帧。"));
        return;
    }
    const int index = dye_combo_->currentIndex();
    const DyeProfile dye = index >= 0 && index < static_cast<int>(dyes_.size())
        ? dyes_[static_cast<std::size_t>(index)] : DyeLibrary::FallbackDye();
    const FluorescenceChannelListActionResult result =
        FluorescenceChannelListActions::AddCurrentFrame(channels_, frame, dye);
    if (result.changed) {
        channels_.back().exposure_ms = exposure_spin_->value();
        refreshFluorescenceChannelList(static_cast<int>(channels_.size()) - 1);
        fusion_check_->setChecked(true);
    }
    statusBar()->showMessage(QString::fromStdWString(result.message), 4000);
}

void CameraMainWindow::clearFluorescenceChannels()
{
    FluorescenceChannelListActions::Clear(channels_);
    refreshFluorescenceChannelList();
    fusion_check_->setChecked(false);
    updateImagePresentation();
}

void CameraMainWindow::refreshFluorescenceChannelList(int selectedRow)
{
    if (!channel_list_) {
        return;
    }
    if (selectedRow < 0) {
        selectedRow = channel_list_->currentRow();
    }
    const QSignalBlocker blocker(channel_list_);
    channel_list_->clear();
    for (const FluorescenceChannel& channel : channels_) {
        channel_list_->addItem(
            QString::fromStdWString(FluorescenceFormatter::FormatChannelLine(channel)));
    }
    if (!channels_.empty()) {
        selectedRow = std::clamp(selectedRow, 0, static_cast<int>(channels_.size()) - 1);
        channel_list_->setCurrentRow(selectedRow);
    }
    updateFluorescenceChannelUi();
}

void CameraMainWindow::updateFluorescenceChannelUi()
{
    if (!channel_list_ || !channel_visible_check_ || !channel_black_slider_ ||
        !channel_white_slider_ || !fluorescence_statistics_label_) {
        return;
    }
    const int row = channel_list_->currentRow();
    if (row < 0 || row >= static_cast<int>(channels_.size())) {
        channel_visible_check_->setChecked(false);
        channel_black_slider_->setValue(0);
        channel_white_slider_->setValue(255);
        fluorescence_statistics_label_->setText(
            tr("选择通道后显示强度与曝光质量。"));
        fluorescence_statistics_label_->setProperty("exposureState", QStringLiteral("none"));
        return;
    }

    const FluorescenceChannel& channel = channels_[static_cast<std::size_t>(row)];
    channel_visible_check_->setChecked(channel.visible);
    channel_black_slider_->setValue(channel.black_level);
    channel_white_slider_->setValue(channel.white_level);
    const FluorescenceChannelStatistics statistics =
        FluorescenceChannelAnalysis::Analyze(channel.frame);
    if (!statistics.IsValid()) {
        fluorescence_statistics_label_->setText(tr("该通道尚无图像数据。"));
        fluorescence_statistics_label_->setProperty("exposureState", QStringLiteral("none"));
        return;
    }

    QString state;
    switch (statistics.exposure) {
    case FluorescenceExposureState::Saturated:
        state = tr("饱和：建议降低曝光或增益");
        fluorescence_statistics_label_->setProperty("exposureState", QStringLiteral("warning"));
        break;
    case FluorescenceExposureState::Underexposed:
        state = tr("欠曝：建议提高曝光或增益");
        fluorescence_statistics_label_->setProperty("exposureState", QStringLiteral("warning"));
        break;
    case FluorescenceExposureState::LowContrast:
        state = tr("低对比度：检查背景、焦点和染色");
        fluorescence_statistics_label_->setProperty("exposureState", QStringLiteral("caution"));
        break;
    case FluorescenceExposureState::Balanced:
        state = tr("曝光范围正常");
        fluorescence_statistics_label_->setProperty("exposureState", QStringLiteral("ok"));
        break;
    case FluorescenceExposureState::NoData:
    default:
        state = tr("无数据");
        fluorescence_statistics_label_->setProperty("exposureState", QStringLiteral("none"));
        break;
    }
    fluorescence_statistics_label_->setText(
        tr("%1\n原始范围 %2–%3，均值 %4；255 饱和像素 %5%\n建议显示范围 %6–%7")
            .arg(state)
            .arg(static_cast<int>(statistics.minimum))
            .arg(static_cast<int>(statistics.maximum))
            .arg(statistics.mean, 0, 'f', 1)
            .arg(statistics.clipped_fraction * 100.0, 0, 'f', 2)
            .arg(static_cast<int>(statistics.suggested_black_level))
            .arg(static_cast<int>(statistics.suggested_white_level)));
    fluorescence_statistics_label_->style()->unpolish(fluorescence_statistics_label_);
    fluorescence_statistics_label_->style()->polish(fluorescence_statistics_label_);
}

void CameraMainWindow::autoLevelSelectedFluorescenceChannel()
{
    const int row = channel_list_ ? channel_list_->currentRow() : -1;
    if (row < 0 || row >= static_cast<int>(channels_.size())) {
        statusBar()->showMessage(tr("请先选择荧光通道。"), 3000);
        return;
    }
    FluorescenceChannel& channel = channels_[static_cast<std::size_t>(row)];
    const FluorescenceChannelStatistics statistics =
        FluorescenceChannelAnalysis::Analyze(channel.frame);
    if (!FluorescenceChannelAnalysis::ApplySuggestedLevels(channel, statistics)) {
        statusBar()->showMessage(tr("该通道没有可用于自动拉伸的图像数据。"), 3500);
        return;
    }
    refreshFluorescenceChannelList(row);
    updateImagePresentation();
    statusBar()->showMessage(
        tr("已按 1%–99.8% 分位设置显示范围；原始像素未修改。"), 4000);
}

void CameraMainWindow::removeSelectedFluorescenceChannel()
{
    const int row = channel_list_ ? channel_list_->currentRow() : -1;
    const FluorescenceChannelListActionResult result =
        FluorescenceChannelListActions::RemoveSelected(channels_, row);
    refreshFluorescenceChannelList(
        result.selected_index ? static_cast<int>(*result.selected_index) : -1);
    if (!result.show_fusion_preview && fusion_check_) {
        fusion_check_->setChecked(false);
    }
    updateImagePresentation();
    statusBar()->showMessage(QString::fromStdWString(result.message), 3500);
}

void CameraMainWindow::isolateSelectedFluorescenceChannel()
{
    const int row = channel_list_ ? channel_list_->currentRow() : -1;
    const FluorescenceChannelListActionResult result =
        FluorescenceChannelListActions::ShowOnlySelected(channels_, row);
    refreshFluorescenceChannelList(
        result.selected_index ? static_cast<int>(*result.selected_index) : row);
    if (result.changed && fusion_check_) {
        fusion_check_->setChecked(true);
    }
    updateImagePresentation();
    statusBar()->showMessage(QString::fromStdWString(result.message), 3500);
}

void CameraMainWindow::showAllFluorescenceChannels()
{
    const int row = channel_list_ ? channel_list_->currentRow() : -1;
    const FluorescenceChannelListActionResult result =
        FluorescenceChannelListActions::ShowAll(channels_);
    refreshFluorescenceChannelList(row);
    if (result.changed && fusion_check_) {
        fusion_check_->setChecked(true);
    }
    updateImagePresentation();
    statusBar()->showMessage(QString::fromStdWString(result.message), 3500);
}

void CameraMainWindow::toggleFusion(bool enabled)
{
    fusion_enabled_ = enabled;
    updateImagePresentation();
    statusBar()->showMessage(enabled ? tr("正在显示荧光融合") : tr("已返回当前图像"), 3000);
}

void CameraMainWindow::addStitchTile()
{
    if (busy_ || live_stitch_active_) return;
    const ImageFrame frame = currentVisibleFrame();
    if (!frame.IsValid()) {
        QMessageBox::information(this, tr("图像拼接"), tr("当前没有可添加的图像帧。"));
        return;
    }
    const int search_percent = ProcessingParameterRules::SearchPercentFromOverlap(
        stitch_overlap_slider_->integerValue());
    const StitchTileListActionResult result = StitchTileListActions::AddCurrentFrame(
        stitch_tiles_, frame, search_percent);
    if (result.changed) {
        stitch_tile_sources_.push_back(current_source_.isEmpty()
            ? tr("当前图像") : current_source_);
        invalidateStitchResult();
        refreshStitchTileList(static_cast<int>(stitch_tiles_.size()) - 1);
    }
    statusBar()->showMessage(QString::fromStdWString(result.message), 4000);
}

void CameraMainWindow::buildStitch()
{
    if (busy_) return;
    if (stitch_tiles_.size() < 2) {
        QMessageBox::information(this, tr("图像拼接"), tr("至少需要两帧图像。"));
        return;
    }
    if (stitch_layout_combo_->currentData().toInt() == static_cast<int>(StitchLayoutMode::Grid) &&
        stitch_rows_spin_->value() * stitch_cols_spin_->value() < static_cast<int>(stitch_tiles_.size())) {
        stitch_rows_spin_->setValue(
            (static_cast<int>(stitch_tiles_.size()) + stitch_cols_spin_->value() - 1) /
            stitch_cols_spin_->value());
        statusBar()->showMessage(tr("网格行数已按源图数量自动扩展"), 3000);
    }
    if (live_stitch_active_) stopLiveStitch(false);
    startStitchJob(stitch_tiles_, stitchOptionsFromUi(), true);
}

void CameraMainWindow::retryStitch()
{
    if (busy_ || !stitch_retry_available_ || stitch_retry_tiles_.size() < 2) return;
    if (live_stitch_active_) stopLiveStitch(false);
    stitch_tiles_ = stitch_retry_tiles_;
    stitch_tile_sources_ = stitch_retry_sources_;
    refreshStitchTileList();
    startStitchJob(stitch_retry_tiles_, stitch_retry_options_, false);
}

void CameraMainWindow::startStitchJob(
    std::vector<StitchTile> tiles,
    StitchProcessingOptions options,
    bool rememberForRetry)
{
    if (busy_ || tiles.size() < 2) return;
    if (rememberForRetry) {
        stitch_retry_tiles_ = tiles;
        stitch_retry_sources_ = stitch_tile_sources_;
        stitch_retry_options_ = options;
        stitch_retry_available_ = true;
    }
    stitch_retry_button_->setEnabled(false);
    stitch_cancel_token_ = std::make_shared<std::atomic_bool>(false);
    const std::shared_ptr<std::atomic_bool> cancel_token = stitch_cancel_token_;
    stitch_progress_->setValue(0);
    stitch_start_button_->setEnabled(false);
    stitch_cancel_button_->setEnabled(true);
    stitch_backend_label_->setText(tr("正在配准与融合，请稍候…"));
    setBusy(true, tr("正在后台生成拼接图…"));
    auto* watcher = new QFutureWatcher<ProcessingJobResult>(this);
    connect(watcher, &QFutureWatcher<ProcessingJobResult>::finished, this, [this, watcher, cancel_token] {
        const ProcessingJobResult result = watcher->result();
        watcher->deleteLater();
        const bool canceled = cancel_token->load();
        stitch_cancel_token_.reset();
        stitch_start_button_->setEnabled(true);
        stitch_cancel_button_->setEnabled(false);
        stitch_retry_button_->setEnabled(stitch_retry_available_);
        stitch_progress_->setValue(result.succeeded ? 100 : 0);
        setBusy(false, QString::fromStdWString(result.status));
        if (result.succeeded) {
            stitch_result_ = result.image;
            stitch_result_metadata_ = result.stitch_metadata;
            stitch_save_button_->setEnabled(true);
            if (result.stitch_metadata.tiles.size() == stitch_tiles_.size()) {
                for (std::size_t index = 0; index < stitch_tiles_.size(); ++index) {
                    const StitchResultTileMetadata& metadata = result.stitch_metadata.tiles[index];
                    stitch_tiles_[index].offset_x = metadata.offset_x;
                    stitch_tiles_[index].offset_y = metadata.offset_y;
                    stitch_tiles_[index].estimated_position = metadata.estimated_position;
                }
                refreshStitchTileList(stitch_tile_list_->currentRow());
            }
            stitch_backend_label_->setText(tr("完成 · %1 · %2 个配准关系 · 输出 %3 × %4")
                .arg(QString::fromStdWString(result.stitch_metadata.backend))
                .arg(result.stitch_metadata.relation_count)
                .arg(result.image.width)
                .arg(result.image.height));
            setCurrentFrame(result.image, tr("拼接结果"));
        } else if (canceled) {
            stitch_backend_label_->setText(tr("拼接已取消，可调整参数后重新生成。"));
        } else {
            stitch_backend_label_->setText(tr("拼接失败，请检查图像重叠区域或尝试其他配准方法。"));
            QMessageBox::warning(this, tr("图像拼接"), tr("未能生成有效的拼接图。"));
        }
    });
    QPointer<QProgressBar> progress = stitch_progress_;
    watcher->setFuture(QtConcurrent::run([tiles, options, cancel_token, progress] {
        auto report_progress = [progress](int value) {
            if (!progress) return;
            QMetaObject::invokeMethod(progress, [progress, value] {
                if (progress) progress->setValue(std::clamp(value, 0, 100));
            }, Qt::QueuedConnection);
        };
        return ProcessingJobExecutor::RunStitch(
            1, tiles, options, cancel_token.get(), report_progress);
    }));
}

StitchProcessingOptions CameraMainWindow::stitchOptionsFromUi() const
{
    StitchProcessingOptions options;
    options.layout_mode = static_cast<StitchLayoutMode>(stitch_layout_combo_->currentData().toInt());
    options.grid_rows = stitch_rows_spin_->value();
    options.grid_cols = stitch_cols_spin_->value();
    options.overlap_percent = stitch_overlap_slider_->integerValue();
    options.registration_method = static_cast<StitchRegistrationMethod>(
        stitch_registration_combo_->currentData().toInt());
    options.transform_model = static_cast<StitchTransformModel>(
        stitch_transform_combo_->currentData().toInt());
    options.blend_mode = static_cast<StitchBlendMode>(stitch_blend_combo_->currentData().toInt());
    return options;
}

void CameraMainWindow::refreshStitchTileList(int selectedRow)
{
    while (stitch_tile_sources_.size() < static_cast<qsizetype>(stitch_tiles_.size())) {
        stitch_tile_sources_.push_back(tr("图像 %1").arg(stitch_tile_sources_.size() + 1));
    }
    while (stitch_tile_sources_.size() > static_cast<qsizetype>(stitch_tiles_.size())) {
        stitch_tile_sources_.removeLast();
    }
    stitch_tile_list_->clear();
    for (std::size_t index = 0; index < stitch_tiles_.size(); ++index) {
        const StitchTile& tile = stitch_tiles_[index];
        const QString placement = index == 0
            ? tr("基准")
            : tile.estimated_position ? tr("待精配准") : tr("已配准");
        stitch_tile_list_->addItem(tr("%1. %2  ·  %3 × %4  ·  (%5, %6)  ·  %7")
            .arg(index + 1)
            .arg(stitch_tile_sources_.at(static_cast<qsizetype>(index)))
            .arg(tile.frame.width)
            .arg(tile.frame.height)
            .arg(tile.offset_x)
            .arg(tile.offset_y)
            .arg(placement));
    }
    if (selectedRow >= 0 && selectedRow < stitch_tile_list_->count()) {
        stitch_tile_list_->setCurrentRow(selectedRow);
    }
    updateProcessingLabels();
}

void CameraMainWindow::deleteSelectedStitchTile()
{
    if (busy_ || live_stitch_active_) return;
    const int row = stitch_tile_list_->currentRow();
    const StitchTileListActionResult result = StitchTileListActions::DeleteSelected(stitch_tiles_, row);
    if (result.changed && row >= 0 && row < stitch_tile_sources_.size()) {
        stitch_tile_sources_.removeAt(row);
        invalidateStitchResult();
    }
    refreshStitchTileList(result.next_selection.value_or(-1));
    statusBar()->showMessage(QString::fromStdWString(result.message), 4000);
}

void CameraMainWindow::importStitchFiles(const QStringList& files, const QString& sourceDescription)
{
    if (busy_ || live_stitch_active_ || files.isEmpty()) return;
    const int previous_count = static_cast<int>(stitch_tiles_.size());
    const int search_percent = ProcessingParameterRules::SearchPercentFromOverlap(
        stitch_overlap_slider_->integerValue());
    int failed = 0;
    for (const QString& file : files) {
        QImageReader reader(file);
        reader.setAutoTransform(true);
        const QImage image = reader.read();
        if (image.isNull()) {
            ++failed;
            continue;
        }
        const StitchTileListActionResult result = StitchTileListActions::AddCurrentFrame(
            stitch_tiles_, imageFrameFromQImage(image), search_percent);
        if (result.changed) stitch_tile_sources_.push_back(QFileInfo(file).fileName());
    }
    const int added = static_cast<int>(stitch_tiles_.size()) - previous_count;
    if (added > 0) invalidateStitchResult();
    refreshStitchTileList(added > 0 ? static_cast<int>(stitch_tiles_.size()) - 1 : -1);
    statusBar()->showMessage(tr("从 %1 导入 %2 张图像%3")
        .arg(sourceDescription)
        .arg(added)
        .arg(failed > 0 ? tr("，%1 张读取失败").arg(failed) : QString()), 5000);
}

void CameraMainWindow::startLiveStitch()
{
    if (live_stitch_active_ || busy_) return;
    if (!camera_open_ || latest_camera_image_.isNull()) {
        QMessageBox::information(this, tr("实时拼接"), tr("请先打开相机并等待图像出现。"));
        return;
    }
    live_stitch_active_ = true;
    ++live_stitch_generation_;
    live_stitch_evaluating_ = false;
    live_stitch_preview_pending_ = false;
    live_stitch_out_of_range_candidate_count_ = 0;
    live_stitch_missing_match_count_ = 0;
    live_stitch_out_of_range_warning_ = false;
    live_stitch_warning_timer_.invalidate();
    clearLiveStitchPreviewCache();
    ensureLiveStitchPreviewCache();
    canvas_->setLivePreviewOverlay(latest_camera_image_);
    live_stitch_start_button_->setEnabled(false);
    live_stitch_stop_button_->setEnabled(true);
    live_stitch_interval_slider_->setEnabled(false);
    live_stitch_status_label_->setText(tr("实时拼接已启动，请缓慢移动载物台并保持视野重叠。"));
    live_stitch_timer_->start(live_stitch_interval_slider_->integerValue());
    if (!stitch_tiles_.empty()) refreshLiveStitchPreview();
    evaluateLiveStitch();
}

void CameraMainWindow::stopLiveStitch(bool showStatus)
{
    if (!live_stitch_active_) return;
    live_stitch_active_ = false;
    ++live_stitch_generation_;
    live_stitch_timer_->stop();
    canvas_->setLivePreviewOverlay({});
    clearLiveStitchPreviewCache();
    live_stitch_out_of_range_candidate_count_ = 0;
    live_stitch_missing_match_count_ = 0;
    live_stitch_out_of_range_warning_ = false;
    live_stitch_start_button_->setEnabled(true);
    live_stitch_stop_button_->setEnabled(false);
    live_stitch_interval_slider_->setEnabled(true);
    live_stitch_status_label_->setText(tr("实时拼接已停止，共保留 %1 张源图。").arg(stitch_tiles_.size()));
    if (!latest_camera_image_.isNull()) {
        if (hasNeutralPresentation()) presentLiveCameraImage();
        else setCurrentFrame(latestCameraFrame(), tr("MUCam 实时预览"), QStringLiteral("camera-live"));
    }
    if (showStatus) statusBar()->showMessage(tr("实时拼接已停止"), 4000);
}

void CameraMainWindow::evaluateLiveStitch()
{
    if (!live_stitch_active_ || live_stitch_evaluating_ || latest_camera_image_.isNull()) return;
    live_stitch_evaluating_ = true;
    ensureLiveStitchPreviewCache();
    std::vector<LiveStitchPreviewTile> references;
    std::vector<std::pair<int, int>> reference_offsets;
    const std::size_t first_reference = live_stitch_preview_cache_.size() >
            static_cast<std::size_t>(kLiveStitchReferenceTileCount)
        ? live_stitch_preview_cache_.size() - static_cast<std::size_t>(kLiveStitchReferenceTileCount)
        : 0;
    references.reserve(live_stitch_preview_cache_.size() - first_reference);
    reference_offsets.reserve(live_stitch_preview_cache_.size() - first_reference);
    for (std::size_t index = first_reference; index < live_stitch_preview_cache_.size(); ++index) {
        references.push_back(live_stitch_preview_cache_[index]);
        reference_offsets.emplace_back(
            stitch_tiles_[index].offset_x,
            stitch_tiles_[index].offset_y);
    }
    ImageFrame candidate = latestCameraFrame();
    const int registration_scale = std::max(1, live_stitch_preview_cache_scale_);
    const quint64 generation = live_stitch_generation_;
    LiveStitchCaptureOptions options;
    options.max_registration_edge = kLiveStitchRegistrationMaxEdge;
    options.min_movement_percent = kLiveStitchMinMovementPercent;
    options.min_overlap_percent = kLiveStitchMinOverlapPercent;
    options.search_percent = std::max(
        ProcessingParameterRules::SearchPercentFromOverlap(
            stitch_overlap_slider_->integerValue()),
        100 - kLiveStitchMinOverlapPercent);
    options.fast_mode = true;
    options.reference_tile_count = kLiveStitchReferenceTileCount;
    auto* watcher = new QFutureWatcher<LiveStitchEvaluation>(this);
    connect(watcher, &QFutureWatcher<LiveStitchEvaluation>::finished, this, [this, watcher] {
        LiveStitchEvaluation evaluation = watcher->result();
        watcher->deleteLater();
        if (evaluation.generation != live_stitch_generation_) return;
        live_stitch_evaluating_ = false;
        if (!live_stitch_active_ ||
            evaluation.baseTileCount != stitch_tiles_.size()) return;

        const LiveStitchCaptureDecision& decision = evaluation.decision;
        if (decision.should_capture) {
            live_stitch_out_of_range_candidate_count_ = 0;
            live_stitch_missing_match_count_ = 0;
            live_stitch_out_of_range_warning_ = false;
            StitchTile tile;
            tile.frame = std::move(evaluation.frame);
            tile.offset_x = decision.tile_offset_x;
            tile.offset_y = decision.tile_offset_y;
            tile.estimated_position = !decision.registration_valid && !decision.first_tile;
            stitch_tiles_.push_back(std::move(tile));
            stitch_tile_sources_.push_back(tr("实时 %1").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
            invalidateStitchResult();
            refreshStitchTileList(static_cast<int>(stitch_tiles_.size()) - 1);
            live_stitch_status_label_->setText(decision.first_tile
                ? tr("已采集基准图像，请移动载物台。")
                : tr("已采集第 %1 张 · 移动 %2% · 重叠 %3% · 配准 %4 ms")
                    .arg(stitch_tiles_.size()).arg(decision.movement_percent)
                    .arg(decision.overlap_percent).arg(evaluation.elapsedMs));
            refreshLiveStitchPreview();
        } else if (decision.match_missing) {
            live_stitch_out_of_range_candidate_count_ = 0;
            ++live_stitch_missing_match_count_;
            const bool confirmed =
                live_stitch_missing_match_count_ >= kLiveStitchMissingMatchWarningFrames;
            live_stitch_out_of_range_warning_ = confirmed;
            if (confirmed) {
                if (!live_stitch_warning_timer_.isValid() ||
                    live_stitch_warning_timer_.elapsed() >= kLiveStitchWarningBeepMinIntervalMs) {
                    QApplication::beep();
                    live_stitch_warning_timer_.restart();
                }
                live_stitch_status_label_->setText(
                    tr("警告：连续多帧无法找到重叠区域，请向上一视野缓慢移回。"));
            } else {
                live_stitch_status_label_->setText(tr("正在确认重叠区域，请保持缓慢移动。"));
            }
        } else if (decision.out_of_range_warning) {
            live_stitch_missing_match_count_ = 0;
            ++live_stitch_out_of_range_candidate_count_;
            const bool confirmed = live_stitch_out_of_range_candidate_count_ >=
                kLiveStitchOutOfRangeWarningFrames;
            live_stitch_out_of_range_warning_ = confirmed;
            if (confirmed) {
                if (!live_stitch_warning_timer_.isValid() ||
                    live_stitch_warning_timer_.elapsed() >= kLiveStitchWarningBeepMinIntervalMs) {
                    QApplication::beep();
                    live_stitch_warning_timer_.restart();
                }
                live_stitch_status_label_->setText(
                    tr("警告：重叠率低于 %1%，请向上一视野移回。")
                        .arg(kLiveStitchMinOverlapPercent));
            } else {
                live_stitch_status_label_->setText(
                    tr("配准暂不稳定，请保持至少 %1% 重叠。")
                        .arg(kLiveStitchMinOverlapPercent));
            }
        } else {
            live_stitch_out_of_range_candidate_count_ = 0;
            live_stitch_missing_match_count_ = 0;
            live_stitch_out_of_range_warning_ = false;
            live_stitch_status_label_->setText(tr("等待移动 · 当前移动 %1% · 重叠 %2%")
                .arg(decision.movement_percent).arg(decision.overlap_percent));
        }
    });
    const std::size_t base_tile_count = stitch_tiles_.size();
    watcher->setFuture(QtConcurrent::run(
        [references = std::move(references), reference_offsets = std::move(reference_offsets),
         candidate = std::move(candidate), options, generation, base_tile_count,
         registration_scale]() mutable {
        QElapsedTimer elapsed;
        elapsed.start();
        LiveStitchEvaluation evaluation;
        evaluation.baseTileCount = base_tile_count;
        evaluation.generation = generation;
        std::vector<StitchTile> registration_tiles;
        registration_tiles.reserve(references.size());
        for (const LiveStitchPreviewTile& reference : references) {
            if (!reference.frame || !reference.frame->IsValid()) continue;
            StitchTile tile;
            tile.frame = *reference.frame;
            tile.offset_x = reference.offset_x;
            tile.offset_y = reference.offset_y;
            tile.estimated_position = reference.estimated_position;
            registration_tiles.push_back(std::move(tile));
        }
        const ImageFrame registration_candidate =
            LiveStitchPreviewBuilder::DownsampleFrame(candidate, registration_scale);
        evaluation.decision = LiveStitchCapturePlanner::Evaluate(
            registration_tiles, registration_candidate, options);
        if (registration_scale > 1 && !evaluation.decision.first_tile) {
            evaluation.decision.dx *= registration_scale;
            evaluation.decision.dy *= registration_scale;
            const std::size_t reference_index = evaluation.decision.reference_tile_index;
            if (reference_index < reference_offsets.size()) {
                evaluation.decision.tile_offset_x =
                    reference_offsets[reference_index].first + evaluation.decision.dx;
                evaluation.decision.tile_offset_y =
                    reference_offsets[reference_index].second + evaluation.decision.dy;
            } else {
                evaluation.decision.tile_offset_x *= registration_scale;
                evaluation.decision.tile_offset_y *= registration_scale;
            }
        }
        evaluation.elapsedMs = elapsed.elapsed();
        evaluation.frame = std::move(candidate);
        return evaluation;
    }));
}

void CameraMainWindow::refreshLiveStitchPreview()
{
    if (!live_stitch_active_ || stitch_tiles_.empty()) return;
    if (live_stitch_preview_running_) {
        live_stitch_preview_pending_ = true;
        return;
    }
    live_stitch_preview_running_ = true;
    live_stitch_preview_pending_ = false;
    ensureLiveStitchPreviewCache();
    const std::vector<LiveStitchPreviewTile> tiles = live_stitch_preview_cache_;
    const quint64 generation = live_stitch_generation_;
    LiveStitchPreviewOptions options;
    options.max_preview_edge = kLiveStitchPreviewMaxEdge;
    options.overlap_percent = stitch_overlap_slider_->integerValue();
    options.blend_mode = static_cast<StitchBlendMode>(stitch_blend_combo_->currentData().toInt());
    auto* watcher = new QFutureWatcher<LiveStitchPreviewResult>(this);
    connect(watcher, &QFutureWatcher<LiveStitchPreviewResult>::finished, this, [this, watcher, generation] {
        const LiveStitchPreviewResult preview = watcher->result();
        watcher->deleteLater();
        live_stitch_preview_running_ = false;
        if (live_stitch_active_ && generation == live_stitch_generation_ && preview.image.IsValid()) {
            setCurrentFrame(preview.image, tr("实时拼接预览"), QStringLiteral("live-stitch-preview"));
        }
        if (live_stitch_active_ && live_stitch_preview_pending_) {
            refreshLiveStitchPreview();
        }
    });
    watcher->setFuture(QtConcurrent::run([tiles, options] {
        return LiveStitchPreviewBuilder::Build(tiles, options);
    }));
}

void CameraMainWindow::clearLiveStitchPreviewCache()
{
    live_stitch_preview_cache_.clear();
    live_stitch_preview_cache_keys_.clear();
    live_stitch_preview_cache_scale_ = 0;
}

quint64 CameraMainWindow::stitchTileFingerprint(const StitchTile& tile)
{
    quint64 value = static_cast<quint64>(reinterpret_cast<quintptr>(tile.frame.bgr.data()));
    const auto mix = [&value](quint64 part) {
        value ^= part + 0x9e3779b97f4a7c15ULL + (value << 6U) + (value >> 2U);
    };
    mix(tile.frame.sequence);
    mix(static_cast<quint64>(static_cast<qint64>(tile.offset_x)));
    mix(static_cast<quint64>(static_cast<qint64>(tile.offset_y)));
    mix(static_cast<quint64>(tile.frame.width));
    mix(static_cast<quint64>(tile.frame.height));
    return value;
}

void CameraMainWindow::rebuildLiveStitchPreviewCache(int scale)
{
    clearLiveStitchPreviewCache();
    live_stitch_preview_cache_scale_ = std::max(1, scale);
    live_stitch_preview_cache_.reserve(stitch_tiles_.size());
    live_stitch_preview_cache_keys_.reserve(stitch_tiles_.size());
    for (const StitchTile& tile : stitch_tiles_) {
        if (!tile.frame.IsValid()) continue;
        ImageFrame preview = LiveStitchPreviewBuilder::DownsampleFrame(
            tile.frame, live_stitch_preview_cache_scale_);
        if (!preview.IsValid()) continue;
        LiveStitchPreviewTile cached;
        cached.frame = std::make_shared<ImageFrame>(std::move(preview));
        cached.offset_x = static_cast<int>(std::lround(
            static_cast<double>(tile.offset_x) / live_stitch_preview_cache_scale_));
        cached.offset_y = static_cast<int>(std::lround(
            static_cast<double>(tile.offset_y) / live_stitch_preview_cache_scale_));
        cached.estimated_position = tile.estimated_position;
        live_stitch_preview_cache_.push_back(std::move(cached));
        live_stitch_preview_cache_keys_.push_back(stitchTileFingerprint(tile));
    }
}

void CameraMainWindow::ensureLiveStitchPreviewCache()
{
    if (stitch_tiles_.empty()) {
        clearLiveStitchPreviewCache();
        return;
    }
    int required_scale = 1;
    for (const StitchTile& tile : stitch_tiles_) {
        required_scale = std::max(required_scale,
            LiveStitchPreviewBuilder::DownsampleScaleFor(
                tile.frame, kLiveStitchPreviewTileMaxEdge));
    }
    bool prefix_matches = live_stitch_preview_cache_scale_ == required_scale &&
        live_stitch_preview_cache_keys_.size() <= stitch_tiles_.size();
    for (std::size_t index = 0;
         prefix_matches && index < live_stitch_preview_cache_keys_.size(); ++index) {
        prefix_matches = live_stitch_preview_cache_keys_[index] ==
            stitchTileFingerprint(stitch_tiles_[index]);
    }
    if (!prefix_matches) {
        rebuildLiveStitchPreviewCache(required_scale);
        return;
    }
    for (std::size_t index = live_stitch_preview_cache_keys_.size();
         index < stitch_tiles_.size(); ++index) {
        const StitchTile& tile = stitch_tiles_[index];
        ImageFrame preview = LiveStitchPreviewBuilder::DownsampleFrame(tile.frame, required_scale);
        if (!preview.IsValid()) {
            rebuildLiveStitchPreviewCache(required_scale);
            return;
        }
        LiveStitchPreviewTile cached;
        cached.frame = std::make_shared<ImageFrame>(std::move(preview));
        cached.offset_x = static_cast<int>(std::lround(
            static_cast<double>(tile.offset_x) / required_scale));
        cached.offset_y = static_cast<int>(std::lround(
            static_cast<double>(tile.offset_y) / required_scale));
        cached.estimated_position = tile.estimated_position;
        live_stitch_preview_cache_.push_back(std::move(cached));
        live_stitch_preview_cache_keys_.push_back(stitchTileFingerprint(tile));
    }
}

void CameraMainWindow::saveStitchResult()
{
    if (!stitch_result_.IsValid()) return;
    QString file_name = QFileDialog::getSaveFileName(
        this, tr("导出拼接结果"), QStringLiteral("stitched-image.png"),
        tr("PNG 图像 (*.png);;TIFF 图像 (*.tif *.tiff);;JPEG 图像 (*.jpg *.jpeg)"));
    if (file_name.isEmpty()) return;
    if (QFileInfo(file_name).suffix().isEmpty()) file_name += QStringLiteral(".png");
    QImageWriter writer(file_name);
    if (!writer.write(qImageFromFrame(stitch_result_))) {
        QMessageBox::warning(this, tr("导出失败"), writer.errorString());
        return;
    }
    QString metadata_message;
    if (stitch_result_metadata_.available) {
        std::filesystem::path metadata_path(file_name.toStdWString());
        metadata_path.replace_extension(L".stitch.txt");
        const ExportActionResult metadata_result =
            ExportActions::SaveStitchMetadata(metadata_path, stitch_result_metadata_);
        metadata_message = metadata_result.saved
            ? tr("；元数据：%1").arg(QString::fromStdWString(metadata_path.wstring()))
            : tr("；元数据导出失败：%1").arg(QString::fromStdWString(metadata_result.message));
    }
    statusBar()->showMessage(tr("拼接结果已导出：%1%2").arg(file_name, metadata_message), 7000);
}

void CameraMainWindow::importStitchFiles(QStringList files)
{
    importStitchFiles(files, tr("导入文件"));
}

void CameraMainWindow::invalidateStitchResult()
{
    stitch_result_ = {};
    stitch_result_metadata_ = {};
    if (stitch_save_button_) stitch_save_button_->setEnabled(false);
}

void CameraMainWindow::addEdfFrame()
{
    const EdfStackListActionResult result = EdfStackListActions::AddCurrentFrame(edf_stack_, currentVisibleFrame());
    updateProcessingLabels();
    statusBar()->showMessage(QString::fromStdWString(result.message), 3000);
}

void CameraMainWindow::buildEdf()
{
    if (busy_) return;
    if (edf_stack_.size() < 2) {
        QMessageBox::information(this, tr("景深扩展"), tr("至少需要两帧焦平面图像。"));
        return;
    }
    const std::vector<ImageFrame> frames = edf_stack_;
    setBusy(true, tr("正在后台生成 EDF 图…"));
    auto* watcher = new QFutureWatcher<EdfResult>(this);
    connect(watcher, &QFutureWatcher<EdfResult>::finished, this, [this, watcher] {
        edf_result_ = watcher->result();
        watcher->deleteLater();
        const bool ok = edf_result_.composite_frame.IsValid();
        focus_map_button_->setEnabled(edf_result_.focus_map.IsValid());
        setBusy(false, ok ? tr("EDF 处理完成") : tr("EDF 处理失败"));
        if (ok) {
            setCurrentFrame(edf_result_.composite_frame, tr("EDF 合成图"));
        } else {
            QMessageBox::warning(this, tr("景深扩展"), tr("未能生成有效的 EDF 图。"));
        }
    });
    watcher->setFuture(QtConcurrent::run([frames] {
        return EdfProcessor::ComposeFocusStack(frames, EdfOptions{});
    }));
}

void CameraMainWindow::showEdfFocusMap()
{
    if (edf_result_.focus_map.IsValid()) {
        setCurrentFrame(edf_result_.focus_map, tr("EDF 焦点图"));
    }
}

void CameraMainWindow::clearProcessing()
{
    if (busy_) return;
    if (live_stitch_active_) stopLiveStitch(false);
    stitch_tiles_.clear();
    stitch_tile_sources_.clear();
    if (stitch_tile_list_) stitch_tile_list_->clear();
    edf_stack_.clear();
    edf_result_ = {};
    stitch_result_ = {};
    stitch_result_metadata_ = {};
    stitch_retry_tiles_.clear();
    stitch_retry_sources_.clear();
    stitch_retry_available_ = false;
    if (stitch_retry_button_) stitch_retry_button_->setEnabled(false);
    if (stitch_save_button_) stitch_save_button_->setEnabled(false);
    focus_map_button_->setEnabled(false);
    updateProcessingLabels();
    statusBar()->showMessage(tr("处理队列已清空"), 3000);
}

void CameraMainWindow::updateProcessingLabels()
{
    if (stitch_count_label_) {
        stitch_count_label_->setText(tr("已加入 %1 帧").arg(stitch_tiles_.size()));
    }
    if (edf_count_label_) {
        edf_count_label_->setText(tr("已加入 %1 帧").arg(edf_stack_.size()));
    }
}

void CameraMainWindow::setBusy(bool busy, const QString& message)
{
    busy_ = busy;
    statusBar()->showMessage(message);
}

ImageFrame CameraMainWindow::imageFrameFromQImage(const QImage& image, quint64 sequence, quint32 timestamp)
{
    const QImage bgr = image.convertToFormat(QImage::Format_BGR888);
    ImageFrame frame;
    frame.width = bgr.width();
    frame.height = bgr.height();
    frame.stride = bgr.bytesPerLine();
    frame.sequence = sequence;
    frame.timestamp = timestamp;
    frame.bgr.resize(static_cast<std::size_t>(frame.stride) * static_cast<std::size_t>(frame.height));
    for (int row = 0; row < frame.height; ++row) {
        std::memcpy(
            frame.bgr.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(frame.stride),
            bgr.constScanLine(row),
            static_cast<std::size_t>(frame.stride));
    }
    return frame;
}

QImage CameraMainWindow::qImageFromFrame(const ImageFrame& frame)
{
    if (!frame.IsValid() || frame.stride < frame.width * 3 ||
        frame.bgr.size() < static_cast<std::size_t>(frame.stride) * static_cast<std::size_t>(frame.height)) {
        return {};
    }
    return QImage(
        frame.bgr.data(), frame.width, frame.height, frame.stride, QImage::Format_BGR888).copy();
}

ImagePoint CameraMainWindow::imagePoint(const QPointF& point)
{
    return {point.x(), point.y()};
}

void CameraMainWindow::closeEvent(QCloseEvent* event)
{
    if (live_stitch_active_) stopLiveStitch(false);
    saveObjectiveCalibrationMemory();
    QSettings settings;
    settings.beginGroup(QStringLiteral("MainWindow"));
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.setValue(QStringLiteral("state"), saveState());
    settings.endGroup();
    if (camera_thread_.isRunning()) {
        QMetaObject::invokeMethod(camera_worker_, "stopCamera", Qt::BlockingQueuedConnection);
    }
    event->accept();
}

void CameraMainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void CameraMainWindow::dropEvent(QDropEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    QStringList local_files;
    const QStringList supported_suffixes{
        QStringLiteral("bmp"), QStringLiteral("png"), QStringLiteral("jpg"),
        QStringLiteral("jpeg"), QStringLiteral("tif"), QStringLiteral("tiff")};
    for (const QUrl& url : urls) {
        if (!url.isLocalFile()) continue;
        const QString path = url.toLocalFile();
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (supported_suffixes.contains(suffix)) {
            local_files.push_back(path);
        }
    }
    if (local_files.size() > 1) {
        importStitchFiles(local_files, tr("拖放文件"));
        if (function_tabs_) function_tabs_->setCurrentIndex(3);
        event->acceptProposedAction();
    } else if (local_files.size() == 1 && loadImageFile(local_files.front())) {
        event->acceptProposedAction();
    }
}

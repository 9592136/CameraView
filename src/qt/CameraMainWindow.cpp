#include "CameraMainWindow.h"

#include "CameraWorker.h"
#include "HistogramWidget.h"
#include "ImageSurface3DDialog.h"
#include "MeasurementToolButton.h"
#include "ProfileAnalysisDialog.h"
#include "ai/YoloWorkspaceWidget.h"
#include "domain/MeasurementFormatter.h"
#include "domain/MeasurementNameFormatter.h"
#include "imaging/ChannelFusionEngine.h"
#include "imaging/DyeLibrary.h"
#include "imaging/EdfStackListActions.h"
#include "imaging/FluorescenceChannelListActions.h"
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
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
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
#include <QSaveFile>
#include <QShortcut>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringConverter>
#include <QTabWidget>
#include <QToolBar>
#include <QStyle>
#include <QTimer>
#include <QTransform>
#include <QTextStream>
#include <QTime>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace {

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

} // namespace

CameraMainWindow::CameraMainWindow(QWidget* parent)
    : QMainWindow(parent), dyes_(DyeLibrary::DefaultDyes())
{
    setupUi();
    setupMenusAndToolbar();
    QSettings settings;
    settings.beginGroup(QStringLiteral("MainWindow"));
    restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("state")).toByteArray());
    settings.endGroup();
    setAcceptDrops(true);

    camera_worker_ = new CameraWorker;
    camera_worker_->moveToThread(&camera_thread_);
    connect(&camera_thread_, &QThread::started, camera_worker_, &CameraWorker::initialize);
    connect(&camera_thread_, &QThread::finished, camera_worker_, &QObject::deleteLater);
    connect(camera_worker_, &CameraWorker::devicesReady, this, &CameraMainWindow::onDevicesReady);
    connect(camera_worker_, &CameraWorker::frameReady, this, &CameraMainWindow::onCameraFrame);
    connect(camera_worker_, &CameraWorker::cameraStateChanged, this,
        [this](bool opened, const QString& message) {
            camera_open_ = opened;
            if (!opened && live_stitch_active_) stopLiveStitch(false);
            camera_state_label_->setText(message);
            statusBar()->showMessage(message, 5000);
        });
    connect(camera_worker_, &CameraWorker::operationFinished, this,
        [this](const QString& message, bool success) {
            statusBar()->showMessage(message, 5000);
            if (!success) {
                camera_state_label_->setText(message);
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
    setMinimumSize(980, 640);

    canvas_ = new ImageCanvas;
    canvas_->setObjectName(QStringLiteral("ImageViewport"));
    setCentralWidget(canvas_);
    connect(canvas_, &ImageCanvas::pointsCommitted, this, &CameraMainWindow::onCanvasPoints);
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
    statusBar()->addWidget(source_label_, 1);
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
    file_menu->addAction(tr("保存诊断报告…"), this, [this] {
        QString file_name = QFileDialog::getSaveFileName(
            this, tr("保存诊断报告"), QStringLiteral("CameraView-diagnostics.txt"), tr("文本文件 (*.txt)"));
        if (file_name.isEmpty()) {
            return;
        }
        QSaveFile file(file_name);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, tr("保存失败"), file.errorString());
            return;
        }
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        stream << "CameraView Qt diagnostics\n"
               << "Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n"
               << "Qt: " << QT_VERSION_STR << "\n"
               << "Camera: " << camera_state_label_->text() << "\n"
               << "Source: " << current_source_ << "\n"
               << "Frame: " << current_frame_.width << " x " << current_frame_.height << "\n"
               << "Calibration: " << (calibration_.IsCalibrated()
                    ? QString::number(calibration_.MicronsPerPixel(), 'g', 10) + " um/px" : "none") << "\n"
               << "Measurements: " << measurements_.Count() << "\n"
               << "Fluorescence channels: " << channels_.size() << "\n"
               << "Stitch frames: " << stitch_tiles_.size() << "\n"
               << "EDF frames: " << edf_stack_.size() << "\n";
        if (!file.commit()) {
            QMessageBox::warning(this, tr("保存失败"), file.errorString());
        } else {
            statusBar()->showMessage(tr("诊断报告已保存：%1").arg(file_name), 5000);
        }
    });
    file_menu->addSeparator();
    file_menu->addAction(tr("退出"), QKeySequence::Quit, this, &QWidget::close);

    QMenu* camera_menu = menuBar()->addMenu(tr("相机(&C)"));
    camera_menu->addAction(tr("刷新设备"), this, &CameraMainWindow::refreshDevices);
    camera_menu->addAction(tr("打开相机"), this, &CameraMainWindow::openSelectedCamera);
    camera_menu->addAction(tr("停止相机"), this, &CameraMainWindow::stopCamera);

    QMenu* image_menu = menuBar()->addMenu(tr("图像(&I)"));
    auto transform_frame = [this](const QTransform& transform, const QString& label) {
        if (!current_frame_.IsValid()) {
            return;
        }
        if (!measurements_.Empty() && QMessageBox::question(
                this, tr("变换图像"), tr("图像变换会清空当前测量结果，是否继续？")) != QMessageBox::Yes) {
            return;
        }
        measurements_.Clear();
        updateMeasurementList();
        const QImage transformed = qImageFromFrame(current_frame_).transformed(transform);
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
    QAction* fit_action = toolbar->addAction(
        style()->standardIcon(QStyle::SP_BrowserReload), tr("适合窗口"), canvas_, &ImageCanvas::fitToView);
    fit_action->setShortcut(QKeySequence(Qt::Key_F));
    QAction* length_action = toolbar->addAction(tr("长度"), this,
        [this] { setMeasurementTool(CanvasTool::Length, tr("请在图像上选择两个点")); });
    length_action->setIcon(measurementToolIcon(MeasurementToolGlyph::Length));
    length_action->setShortcut(QKeySequence(Qt::Key_L));
    QAction* angle_action = toolbar->addAction(tr("角度"), this,
        [this] { setMeasurementTool(CanvasTool::Angle, tr("请依次选择端点、顶点、端点")); });
    angle_action->setIcon(measurementToolIcon(MeasurementToolGlyph::Angle));
    profile_action->setIcon(measurementToolIcon(MeasurementToolGlyph::Profile));
    toolbar->addAction(profile_action);
    toolbar->addSeparator();
    toolbar->addAction(surface_action);
}

QWidget* CameraMainWindow::buildCameraPage()
{
    auto* page = new QWidget;
    auto* layout = panelLayout(page);
    auto* form = new QFormLayout;
    device_combo_ = new QComboBox;
    form->addRow(tr("设备"), device_combo_);
    exposure_spin_ = new QDoubleSpinBox;
    exposure_spin_->setRange(0.01, 10000.0);
    exposure_spin_->setValue(10.0);
    exposure_spin_->setSuffix(tr(" ms"));
    form->addRow(tr("曝光"), exposure_spin_);
    gain_spin_ = new QDoubleSpinBox;
    gain_spin_->setRange(0.01, 100.0);
    gain_spin_->setValue(1.0);
    form->addRow(tr("增益"), gain_spin_);
    layout->addLayout(form);

    auto* refresh = new QPushButton(tr("刷新"));
    auto* open = new QPushButton(tr("打开"));
    auto* stop = new QPushButton(tr("停止"));
    setButtonRole(open, "primary");
    setButtonRole(stop, "danger");
    layout->addWidget(buttonRow({refresh, open, stop}));
    connect(refresh, &QPushButton::clicked, this, &CameraMainWindow::refreshDevices);
    connect(open, &QPushButton::clicked, this, &CameraMainWindow::openSelectedCamera);
    connect(stop, &QPushButton::clicked, this, &CameraMainWindow::stopCamera);

    auto* apply_exposure = addButton(layout, tr("应用曝光"));
    auto* auto_exposure = addButton(layout, tr("自动曝光"));
    auto* apply_gain = addButton(layout, tr("应用增益"));
    auto* white_balance = addButton(layout, tr("白平衡"));
    connect(apply_exposure, &QPushButton::clicked, this, [this] {
        QMetaObject::invokeMethod(camera_worker_, "setExposure", Qt::QueuedConnection,
            Q_ARG(double, exposure_spin_->value()));
    });
    connect(auto_exposure, &QPushButton::clicked, this, [this] {
        QMetaObject::invokeMethod(camera_worker_, "autoExposure", Qt::QueuedConnection);
    });
    connect(apply_gain, &QPushButton::clicked, this, [this] {
        QMetaObject::invokeMethod(camera_worker_, "setGain", Qt::QueuedConnection,
            Q_ARG(double, gain_spin_->value()));
    });
    connect(white_balance, &QPushButton::clicked, this, [this] {
        QMetaObject::invokeMethod(camera_worker_, "whiteBalance", Qt::QueuedConnection);
    });
    camera_state_label_ = new QLabel(tr("正在初始化 MUCam SDK…"));
    camera_state_label_->setWordWrap(true);
    layout->addWidget(camera_state_label_);
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
    dye_combo_ = new QComboBox;
    for (const DyeProfile& dye : dyes_) {
        dye_combo_->addItem(QString::fromStdWString(dye.name));
    }
    layout->addWidget(new QLabel(tr("染料")));
    layout->addWidget(dye_combo_);
    auto* add = addButton(layout, tr("当前帧添加为通道"));
    auto* clear = addButton(layout, tr("清空通道"));
    fusion_check_ = new QCheckBox(tr("显示融合预览"));
    layout->addWidget(fusion_check_);
    channel_list_ = new QListWidget;
    layout->addWidget(channel_list_, 1);
    auto* channel_form = new QFormLayout;
    channel_visible_check_ = new QCheckBox(tr("可见"));
    channel_visible_check_->setChecked(true);
    channel_black_spin_ = new QSpinBox;
    channel_black_spin_->setRange(0, 254);
    channel_white_spin_ = new QSpinBox;
    channel_white_spin_->setRange(1, 255);
    channel_white_spin_->setValue(255);
    channel_form->addRow(tr("通道"), channel_visible_check_);
    channel_form->addRow(tr("黑电平"), channel_black_spin_);
    channel_form->addRow(tr("白电平"), channel_white_spin_);
    layout->addLayout(channel_form);
    auto* apply_channel = addButton(layout, tr("应用通道设置"));
    connect(add, &QPushButton::clicked, this, &CameraMainWindow::addFluorescenceChannel);
    connect(clear, &QPushButton::clicked, this, &CameraMainWindow::clearFluorescenceChannels);
    connect(fusion_check_, &QCheckBox::toggled, this, &CameraMainWindow::toggleFusion);
    connect(channel_list_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= static_cast<int>(channels_.size())) {
            return;
        }
        const FluorescenceChannel& channel = channels_[static_cast<std::size_t>(row)];
        channel_visible_check_->setChecked(channel.visible);
        channel_black_spin_->setValue(channel.black_level);
        channel_white_spin_->setValue(channel.white_level);
    });
    connect(apply_channel, &QPushButton::clicked, this, [this] {
        const int row = channel_list_->currentRow();
        if (row < 0 || row >= static_cast<int>(channels_.size())) {
            return;
        }
        if (channel_black_spin_->value() >= channel_white_spin_->value()) {
            QMessageBox::warning(this, tr("通道设置"), tr("黑电平必须小于白电平。"));
            return;
        }
        FluorescenceChannel& channel = channels_[static_cast<std::size_t>(row)];
        channel.visible = channel_visible_check_->isChecked();
        channel.black_level = static_cast<unsigned char>(channel_black_spin_->value());
        channel.white_level = static_cast<unsigned char>(channel_white_spin_->value());
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
    live_stitch_interval_spin_ = new QSpinBox;
    live_stitch_interval_spin_->setRange(250, 10000);
    live_stitch_interval_spin_->setValue(750);
    live_stitch_interval_spin_->setSuffix(tr(" ms"));
    live_form->addRow(tr("检测间隔"), live_stitch_interval_spin_);
    live_layout->addLayout(live_form);
    live_stitch_start_button_ = new QPushButton(tr("开始实时拼接"));
    live_stitch_stop_button_ = new QPushButton(tr("停止"));
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
    stitch_overlap_spin_ = new QSpinBox;
    stitch_overlap_spin_->setObjectName(QStringLiteral("StitchOverlapSpin"));
    stitch_overlap_spin_->setRange(
        ProcessingParameterRules::MinStitchOverlapPercent(),
        ProcessingParameterRules::MaxStitchOverlapPercent());
    stitch_overlap_spin_->setValue(ProcessingParameterRules::DefaultStitchOverlapPercent());
    stitch_overlap_spin_->setSuffix(QStringLiteral(" %"));
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
    settings_form->addRow(tr("预计重叠"), stitch_overlap_spin_);
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
    auto* tile_actions = new QHBoxLayout;
    auto* move_up = new QPushButton(tr("上移"));
    auto* move_down = new QPushButton(tr("下移"));
    auto* delete_tile = new QPushButton(tr("删除"));
    auto* clear_tiles = new QPushButton(tr("清空"));
    setButtonRole(delete_tile, "danger");
    setButtonRole(clear_tiles, "danger");
    tile_actions->addWidget(move_up);
    tile_actions->addWidget(move_down);
    tile_actions->addWidget(delete_tile);
    tile_actions->addWidget(clear_tiles);
    stitch_layout->addLayout(tile_actions);
    stitch_start_button_ = addButton(stitch_layout, tr("生成拼接图"));
    setButtonRole(stitch_start_button_, "primary");
    stitch_progress_ = new QProgressBar;
    stitch_progress_->setObjectName(QStringLiteral("StitchProgress"));
    stitch_progress_->setRange(0, 100);
    stitch_progress_->setValue(0);
    stitch_layout->addWidget(stitch_progress_);
    stitch_cancel_button_ = addButton(stitch_layout, tr("取消拼接"));
    stitch_cancel_button_->setEnabled(false);
    setButtonRole(stitch_cancel_button_, "danger");
    stitch_backend_label_ = new QLabel(tr("后端：OpenCV 优先，缺失时使用内置算法"));
    stitch_backend_label_->setWordWrap(true);
    stitch_layout->addWidget(stitch_backend_label_);
    stitch_save_button_ = addButton(stitch_layout, tr("导出拼接结果…"));
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
        const QStringList names = dir.entryList(
            {QStringLiteral("*.bmp"), QStringLiteral("*.png"), QStringLiteral("*.jpg"),
             QStringLiteral("*.jpeg"), QStringLiteral("*.tif"), QStringLiteral("*.tiff")},
            QDir::Files, QDir::Name);
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
    calibration_length_spin_ = new QDoubleSpinBox;
    calibration_length_spin_->setRange(0.001, 1000000.0);
    calibration_length_spin_->setValue(100.0);
    calibration_unit_combo_ = new QComboBox;
    calibration_unit_combo_->addItems({tr("µm"), tr("mm")});
    calibration_form->addRow(tr("真实长度"), calibration_length_spin_);
    calibration_form->addRow(tr("单位"), calibration_unit_combo_);
    auto* calibrate = new QPushButton(tr("两点标定"));
    calibrate->setIcon(measurementToolIcon(MeasurementToolGlyph::Calibration));
    calibrate->setIconSize(QSize(22, 22));
    auto* clear_calibration = new QPushButton(tr("清除标定"));
    calibration_form->addRow(buttonRow({calibrate, clear_calibration}));
    calibration_label_ = new QLabel(tr("未标定"));
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
    edge_snap_radius_spin_ = new QSpinBox;
    edge_snap_radius_spin_->setRange(2, 40);
    edge_snap_radius_spin_->setValue(12);
    edge_snap_radius_spin_->setSuffix(tr(" px"));
    edge_form->addRow(edge_snap_check_);
    edge_form->addRow(tr("搜索半径"), edge_snap_radius_spin_);
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
    smart_similarity_spin_ = new QDoubleSpinBox;
    smart_similarity_spin_->setObjectName(QStringLiteral("SmartCountSimilaritySpin"));
    smart_similarity_spin_->setRange(0.40, 0.99);
    smart_similarity_spin_->setSingleStep(0.01);
    smart_similarity_spin_->setDecimals(2);
    smart_similarity_spin_->setValue(0.78);
    smart_scale_tolerance_spin_ = new QSpinBox;
    smart_scale_tolerance_spin_->setRange(0, 40);
    smart_scale_tolerance_spin_->setValue(15);
    smart_scale_tolerance_spin_->setSuffix(tr(" %"));
    smart_form->addRow(tr("相似度阈值"), smart_similarity_spin_);
    smart_form->addRow(tr("尺寸变化范围"), smart_scale_tolerance_spin_);
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
    measurement_list_->setToolTip(tr("单击高亮，双击在图像中定位；F2 重命名，Delete 删除"));
    layout->addWidget(measurement_list_, 1);
    auto* rename_selected = new QPushButton(tr("重命名"));
    auto* delete_selected = new QPushButton(tr("删除选中"));
    auto* clear = new QPushButton(tr("清空"));
    layout->addWidget(buttonRow({rename_selected, delete_selected, clear}));
    auto* export_csv = addButton(layout, tr("导出测量 CSV"));
    setButtonRole(clear, "danger");
    setButtonRole(export_csv, "primary");

    connect(calibrate, &QPushButton::clicked, this, [this] {
        setMeasurementTool(CanvasTool::Calibration, tr("请在图像上选择标定线的两个端点"));
    });
    connect(clear_calibration, &QPushButton::clicked, this, [this] {
        calibration_ = CalibrationProfile::Uncalibrated();
        calibration_label_->setText(tr("未标定"));
        updateMeasurementList();
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
    connect(edge_snap_radius_spin_, qOverload<int>(&QSpinBox::valueChanged),
        canvas_, &ImageCanvas::setEdgeSnapRadius);
    connect(smart_select_button_, &QPushButton::clicked, this, [this] {
        smart_count_session_active_ = true;
        setMeasurementTool(CanvasTool::SmartCountSample,
            tr("请选择目标外接矩形的两个对角点；可连续框选多个，按 Esc 结束"));
    });
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
    auto rename_current = [this] {
        const int row = measurement_list_->currentRow();
        const std::optional<MeasurementReference> reference = row >= 0
            ? measurements_.AtFlatIndex(static_cast<std::size_t>(row)) : std::nullopt;
        if (!reference) {
            return;
        }
        bool accepted = false;
        const QString name = QInputDialog::getText(
            this, tr("重命名测量"), tr("名称"), QLineEdit::Normal,
            QString::fromStdWString(measurements_.Name(*reference)), &accepted).trimmed();
        if (accepted && !name.isEmpty() && measurements_.SetName(*reference, name.toStdWString())) {
            updateMeasurementList();
        }
    };
    connect(rename_selected, &QPushButton::clicked, this, rename_current);
    connect(measurement_list_, &QListWidget::currentRowChanged, this,
        [this](int) { rebuildOverlays(); });
    connect(measurement_list_, &QListWidget::itemDoubleClicked, this,
        [this](QListWidgetItem*) { focusSelectedMeasurement(); });
    auto* rename_shortcut = new QShortcut(QKeySequence(Qt::Key_F2), measurement_list_);
    connect(rename_shortcut, &QShortcut::activated, this, rename_current);
    auto* delete_shortcut = new QShortcut(QKeySequence::Delete, measurement_list_);
    connect(delete_shortcut, &QShortcut::activated, this, &CameraMainWindow::deleteSelectedMeasurement);
    connect(delete_selected, &QPushButton::clicked, this, &CameraMainWindow::deleteSelectedMeasurement);
    connect(clear, &QPushButton::clicked, this, &CameraMainWindow::clearMeasurements);
    connect(export_csv, &QPushButton::clicked, this, &CameraMainWindow::exportMeasurements);
    updateSmartTargetUi();
    return page;
}

QWidget* CameraMainWindow::buildProjectPage()
{
    auto* page = new QWidget;
    auto* layout = panelLayout(page);
    auto* open = addButton(layout, tr("打开项目…"));
    auto* save = addButton(layout, tr("保存项目…"));
    setButtonRole(save, "primary");
    layout->addWidget(new QLabel(tr("项目文件保存标定、测量、染料、荧光通道配置以及处理参数。图像数据请单独导出。")));
    layout->itemAt(2)->widget()->setProperty("wordWrap", true);
    layout->addStretch();
    connect(open, &QPushButton::clicked, this, &CameraMainWindow::openProject);
    connect(save, &QPushButton::clicked, this, &CameraMainWindow::saveProject);
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
    const ProjectDocument document = ProjectSessionMapper::ToDocument(
        calibration_, measurements_, dyes_, channels_, EdfOptions{}, 85,
        {L"Default"}, {calibration_}, 0, true);
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
    calibration_ = state.calibration;
    measurements_ = std::move(state.measurements);
    dyes_ = std::move(state.dye_profiles);
    channels_ = std::move(state.fluorescence_channels);
    if (dyes_.empty()) {
        dyes_ = DyeLibrary::DefaultDyes();
    }
    dye_combo_->clear();
    for (const DyeProfile& dye : dyes_) {
        dye_combo_->addItem(QString::fromStdWString(dye.name));
    }
    channel_list_->clear();
    for (const FluorescenceChannel& channel : channels_) {
        channel_list_->addItem(QString::fromStdWString(channel.name));
    }
    calibration_label_->setText(calibration_.IsCalibrated()
        ? tr("%1 µm / px").arg(calibration_.MicronsPerPixel(), 0, 'g', 7)
        : tr("未标定"));
    updateMeasurementList();
    updateImagePresentation();
    statusBar()->showMessage(tr("项目已打开：%1").arg(file_name), 5000);
}

void CameraMainWindow::refreshDevices()
{
    camera_state_label_->setText(tr("正在刷新设备…"));
    QMetaObject::invokeMethod(camera_worker_, "refreshDevices", Qt::QueuedConnection);
}

void CameraMainWindow::openSelectedCamera()
{
    const int row = device_combo_->currentIndex();
    const int device_index = row >= 0 && row < camera_indices_.size() ? camera_indices_[row] : -1;
    camera_state_label_->setText(tr("正在打开相机…"));
    QMetaObject::invokeMethod(camera_worker_, "openCamera", Qt::QueuedConnection,
        Q_ARG(int, device_index), Q_ARG(double, exposure_spin_->value()));
}

void CameraMainWindow::stopCamera()
{
    if (live_stitch_active_) stopLiveStitch(false);
    QMetaObject::invokeMethod(camera_worker_, "stopCamera", Qt::QueuedConnection);
}

void CameraMainWindow::onDevicesReady(QStringList labels, QVector<int> indices, QString diagnostic)
{
    camera_indices_ = std::move(indices);
    device_combo_->clear();
    device_combo_->addItems(labels);
    camera_state_label_->setText(diagnostic);
    statusBar()->showMessage(diagnostic, 5000);
}

void CameraMainWindow::onCameraFrame(QImage image, quint64 sequence, quint32 timestamp)
{
    latest_camera_frame_ = imageFrameFromQImage(image, sequence, timestamp);
    if (!busy_ && !live_stitch_active_ && !smart_count_session_active_) {
        setCurrentFrame(latest_camera_frame_, tr("MUCam 实时预览"), QStringLiteral("camera-live"));
    }
}

void CameraMainWindow::setCurrentFrame(
    ImageFrame frame,
    const QString& source,
    const QString& sourceIdentity)
{
    const QString new_identity = sourceIdentity.isEmpty() ? source : sourceIdentity;
    if (!current_source_identity_.isEmpty() && current_source_identity_ != new_identity) {
        profile_line_points_.clear();
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

    if (fusion_enabled_ && !channels_.empty()) {
        display_frame_ = ChannelFusionEngine::Fuse(channels_);
    } else if (current_frame_.IsValid()) {
        ImageFrame adjusted = ApplyAdjustments(current_frame_, adjustments_);
        display_frame_ = PseudoColorMapper::Apply(adjusted, palette_);
    } else {
        display_frame_ = {};
    }
    canvas_->setImage(qImageFromFrame(display_frame_));
    if (yolo_workspace_) {
        yolo_workspace_->setCurrentImage(
            qImageFromFrame(display_frame_), current_source_, current_source_identity_);
    }
    rebuildOverlays();
    if (histogram_) {
        const HistogramData data = ComputeHistogram(display_frame_, histogram_channel_);
        const QColor colors[] = {
            QColor(120, 190, 255), QColor(239, 68, 68), QColor(34, 197, 94), QColor(59, 130, 246)};
        histogram_->setHistogram(data, colors[static_cast<int>(histogram_channel_)]);
    }
}

ImageFrame CameraMainWindow::currentVisibleFrame() const
{
    return display_frame_.IsValid() ? display_frame_ : current_frame_;
}

void CameraMainWindow::onCanvasPoints(CanvasTool tool, QVector<QPointF> points)
{
    std::optional<MeasurementReference> added_measurement;
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
        const MeasurementUnit unit = calibration_unit_combo_->currentIndex() == 0
            ? MeasurementUnit::Micrometers : MeasurementUnit::Millimeters;
        calibration_ = CalibrationProfile::FromTwoPointCalibration(
            imagePoint(points[0]), imagePoint(points[1]), calibration_length_spin_->value(), unit);
        if (!calibration_.IsCalibrated()) {
            QMessageBox::warning(this, tr("标定失败"), tr("两个标定点不能重合。"));
        } else {
            calibration_label_->setText(tr("%1 µm / px").arg(calibration_.MicronsPerPixel(), 0, 'g', 7));
            statusBar()->showMessage(tr("标定已完成"), 5000);
        }
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
    canvas_->setTool(CanvasTool::None);
    updateMeasurementList();
    if (added_measurement) {
        measurement_list_->setCurrentRow(static_cast<int>(measurements_.FlatIndexOf(*added_measurement)));
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
    rebuildOverlays();
}

QVector<CanvasOverlay> CameraMainWindow::measurementOverlays() const
{
    QVector<CanvasOverlay> overlays;
    const int selected = measurement_list_ ? measurement_list_->currentRow() : -1;
    auto append = [&overlays, selected](CanvasOverlay overlay) {
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
    if (smart_similarity_spin_) smart_similarity_spin_->setEnabled(!smart_count_running_);
    if (smart_scale_tolerance_spin_) smart_scale_tolerance_spin_->setEnabled(!smart_count_running_);
    if (smart_count_progress_) smart_count_progress_->setVisible(smart_count_running_);
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
    options.similarity_threshold = smart_similarity_spin_->value();
    options.scale_tolerance = smart_scale_tolerance_spin_->value() / 100.0;
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
            std::filesystem::path(file_name.toStdWString()), measurements_, calibration_, display_unit_, L"Default", error)) {
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

void CameraMainWindow::addFluorescenceChannel()
{
    if (!current_frame_.IsValid()) {
        QMessageBox::information(this, tr("荧光通道"), tr("当前没有可添加的图像帧。"));
        return;
    }
    const int index = dye_combo_->currentIndex();
    const DyeProfile dye = index >= 0 && index < static_cast<int>(dyes_.size())
        ? dyes_[static_cast<std::size_t>(index)] : DyeLibrary::FallbackDye();
    const FluorescenceChannelListActionResult result =
        FluorescenceChannelListActions::AddCurrentFrame(channels_, current_frame_, dye);
    if (result.changed) {
        const FluorescenceChannel& channel = channels_.back();
        channel_list_->addItem(QString::fromStdWString(channel.name));
        channel_list_->setCurrentRow(channel_list_->count() - 1);
        fusion_check_->setChecked(true);
    }
    statusBar()->showMessage(QString::fromStdWString(result.message), 4000);
}

void CameraMainWindow::clearFluorescenceChannels()
{
    FluorescenceChannelListActions::Clear(channels_);
    channel_list_->clear();
    fusion_check_->setChecked(false);
    updateImagePresentation();
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
    if (!current_frame_.IsValid()) {
        QMessageBox::information(this, tr("图像拼接"), tr("当前没有可添加的图像帧。"));
        return;
    }
    const int search_percent = ProcessingParameterRules::SearchPercentFromOverlap(stitch_overlap_spin_->value());
    const StitchTileListActionResult result = StitchTileListActions::AddCurrentFrame(
        stitch_tiles_, current_frame_, search_percent);
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
    const std::vector<StitchTile> tiles = stitch_tiles_;
    const StitchProcessingOptions options = stitchOptionsFromUi();
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
        stitch_progress_->setValue(result.succeeded ? 100 : 0);
        setBusy(false, QString::fromStdWString(result.status));
        if (result.succeeded) {
            stitch_result_ = result.image;
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
    options.overlap_percent = stitch_overlap_spin_->value();
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
    const int search_percent = ProcessingParameterRules::SearchPercentFromOverlap(stitch_overlap_spin_->value());
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
    if (!camera_open_ || !latest_camera_frame_.IsValid()) {
        QMessageBox::information(this, tr("实时拼接"), tr("请先打开相机并等待图像出现。"));
        return;
    }
    live_stitch_active_ = true;
    ++live_stitch_generation_;
    live_stitch_evaluating_ = false;
    live_stitch_preview_pending_ = false;
    live_stitch_start_button_->setEnabled(false);
    live_stitch_stop_button_->setEnabled(true);
    live_stitch_interval_spin_->setEnabled(false);
    live_stitch_status_label_->setText(tr("实时拼接已启动，请缓慢移动载物台并保持视野重叠。"));
    live_stitch_timer_->start(live_stitch_interval_spin_->value());
    evaluateLiveStitch();
}

void CameraMainWindow::stopLiveStitch(bool showStatus)
{
    if (!live_stitch_active_) return;
    live_stitch_active_ = false;
    ++live_stitch_generation_;
    live_stitch_timer_->stop();
    live_stitch_start_button_->setEnabled(true);
    live_stitch_stop_button_->setEnabled(false);
    live_stitch_interval_spin_->setEnabled(true);
    live_stitch_status_label_->setText(tr("实时拼接已停止，共保留 %1 张源图。").arg(stitch_tiles_.size()));
    if (latest_camera_frame_.IsValid()) {
        setCurrentFrame(latest_camera_frame_, tr("MUCam 实时预览"), QStringLiteral("camera-live"));
    }
    if (showStatus) statusBar()->showMessage(tr("实时拼接已停止"), 4000);
}

void CameraMainWindow::evaluateLiveStitch()
{
    if (!live_stitch_active_ || live_stitch_evaluating_ || !latest_camera_frame_.IsValid()) return;
    live_stitch_evaluating_ = true;
    const std::vector<StitchTile> tiles = stitch_tiles_;
    const ImageFrame candidate = latest_camera_frame_;
    const quint64 generation = live_stitch_generation_;
    LiveStitchCaptureOptions options;
    options.min_movement_percent = 20;
    options.min_overlap_percent = std::max(5, stitch_overlap_spin_->value() - 10);
    options.search_percent = ProcessingParameterRules::SearchPercentFromOverlap(stitch_overlap_spin_->value());
    options.reference_tile_count = 3;
    auto* watcher = new QFutureWatcher<LiveStitchEvaluation>(this);
    connect(watcher, &QFutureWatcher<LiveStitchEvaluation>::finished, this, [this, watcher] {
        const LiveStitchEvaluation evaluation = watcher->result();
        watcher->deleteLater();
        if (evaluation.generation != live_stitch_generation_) return;
        live_stitch_evaluating_ = false;
        if (!live_stitch_active_ ||
            evaluation.baseTileCount != stitch_tiles_.size()) return;

        const LiveStitchCaptureDecision& decision = evaluation.decision;
        if (decision.should_capture) {
            StitchTile tile;
            tile.frame = evaluation.frame;
            tile.offset_x = decision.tile_offset_x;
            tile.offset_y = decision.tile_offset_y;
            tile.estimated_position = !decision.registration_valid && !decision.first_tile;
            stitch_tiles_.push_back(std::move(tile));
            stitch_tile_sources_.push_back(tr("实时 %1").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
            invalidateStitchResult();
            refreshStitchTileList(static_cast<int>(stitch_tiles_.size()) - 1);
            live_stitch_status_label_->setText(decision.first_tile
                ? tr("已采集基准图像，请移动载物台。")
                : tr("已采集第 %1 张 · 移动 %2% · 重叠 %3%")
                    .arg(stitch_tiles_.size()).arg(decision.movement_percent).arg(decision.overlap_percent));
            refreshLiveStitchPreview();
        } else if (decision.out_of_range_warning || decision.match_missing) {
            live_stitch_status_label_->setText(tr("未采集：重叠区域不足或配准不稳定，请向上一视野缓慢移回。"));
        } else {
            live_stitch_status_label_->setText(tr("等待移动 · 当前移动 %1% · 重叠 %2%")
                .arg(decision.movement_percent).arg(decision.overlap_percent));
        }
    });
    watcher->setFuture(QtConcurrent::run([tiles, candidate, options, generation] {
        LiveStitchEvaluation evaluation;
        evaluation.baseTileCount = tiles.size();
        evaluation.frame = candidate;
        evaluation.generation = generation;
        evaluation.decision = LiveStitchCapturePlanner::Evaluate(tiles, candidate, options);
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
    const std::vector<StitchTile> tiles = stitch_tiles_;
    const quint64 generation = live_stitch_generation_;
    LiveStitchPreviewOptions options;
    options.overlap_percent = stitch_overlap_spin_->value();
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
    statusBar()->showMessage(tr("拼接结果已导出：%1").arg(file_name), 5000);
}

void CameraMainWindow::invalidateStitchResult()
{
    stitch_result_ = {};
    if (stitch_save_button_) stitch_save_button_->setEnabled(false);
}

void CameraMainWindow::addEdfFrame()
{
    const EdfStackListActionResult result = EdfStackListActions::AddCurrentFrame(edf_stack_, current_frame_);
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
    if (!urls.isEmpty() && urls.front().isLocalFile() && loadImageFile(urls.front().toLocalFile())) {
        event->acceptProposedAction();
    }
}

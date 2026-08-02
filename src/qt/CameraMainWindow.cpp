#include "CameraMainWindow.h"

#include "CameraWorker.h"
#include "HistogramWidget.h"
#include "ai/YoloWorkspaceWidget.h"
#include "domain/MeasurementFormatter.h"
#include "domain/MeasurementNameFormatter.h"
#include "imaging/ChannelFusionEngine.h"
#include "imaging/DyeLibrary.h"
#include "imaging/EdfStackListActions.h"
#include "imaging/FluorescenceChannelListActions.h"
#include "imaging/HistogramCalculator.h"
#include "storage/MeasurementCsvExporter.h"
#include "storage/ProjectRepository.h"
#include "storage/ProjectSessionMapper.h"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QDateTime>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImageReader>
#include <QImageWriter>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QPainter>
#include <QSaveFile>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringConverter>
#include <QTabWidget>
#include <QToolBar>
#include <QTransform>
#include <QTextStream>
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
    setCentralWidget(canvas_);
    connect(canvas_, &ImageCanvas::pointsCommitted, this, &CameraMainWindow::onCanvasPoints);
    connect(canvas_, &ImageCanvas::imagePositionChanged, this, [this](const QPointF& point) {
        coordinate_label_->setText(tr("X %1  Y %2").arg(point.x(), 0, 'f', 1).arg(point.y(), 0, 'f', 1));
    });
    connect(canvas_, &ImageCanvas::zoomChanged, this, [this](double zoom) {
        zoom_label_->setText(tr("缩放 %1%").arg(qRound(zoom * 100.0)));
    });

    auto* dock = new QDockWidget(tr("功能"), this);
    dock->setObjectName(QStringLiteral("FunctionDock"));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setMinimumWidth(320);
    function_tabs_ = new QTabWidget;
    function_tabs_->setDocumentMode(true);
    function_tabs_->addTab(buildCameraPage(), tr("相机"));
    function_tabs_->addTab(buildImagePage(), tr("图像"));
    function_tabs_->addTab(buildFluorescencePage(), tr("荧光"));
    function_tabs_->addTab(buildProcessingPage(), tr("处理"));
    function_tabs_->addTab(buildMeasurementPage(), tr("测量"));
    yolo_workspace_ = new YoloWorkspaceWidget;
    function_tabs_->addTab(yolo_workspace_, tr("AI"));
    function_tabs_->addTab(buildProjectPage(), tr("项目"));
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
            canvas_->setTool(tool);
            statusBar()->showMessage(hint);
        });
    connect(yolo_workspace_, &YoloWorkspaceWidget::imageOpenRequested, this,
        [this](const QString& path) {
            if (!loadImageFile(path)) yolo_workspace_->cancelPendingDatasetImageOpen();
        });
    connect(yolo_workspace_, &YoloWorkspaceWidget::statusMessage, this,
        [this](const QString& message) { statusBar()->showMessage(message, 7000); });
    dock->setWidget(function_tabs_);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    source_label_ = new QLabel(tr("无图像"));
    coordinate_label_ = new QLabel(tr("X —  Y —"));
    zoom_label_ = new QLabel(tr("缩放 100%"));
    statusBar()->addWidget(source_label_, 1);
    statusBar()->addPermanentWidget(coordinate_label_);
    statusBar()->addPermanentWidget(zoom_label_);
    statusBar()->showMessage(tr("就绪"));

    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget { background: #20252c; color: #e5e9ef; }
        QMenuBar, QMenu, QToolBar, QStatusBar { background: #292f38; }
        QTabWidget::pane { border: 1px solid #3b4450; }
        QTabBar::tab { background: #2d343e; padding: 8px 10px; }
        QTabBar::tab:selected { background: #3b82f6; }
        QPushButton { background: #36404c; border: 1px solid #4b5867; border-radius: 4px; padding: 6px; }
        QPushButton:hover { background: #425064; }
        QPushButton:pressed { background: #2563eb; }
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QListWidget {
            background: #171b20; border: 1px solid #46515f; border-radius: 3px; padding: 4px;
        }
        QGroupBox { border: 1px solid #3b4450; border-radius: 5px; margin-top: 10px; padding-top: 8px; }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }
        QSlider::groove:horizontal { height: 5px; background: #3b4450; }
        QSlider::handle:horizontal { width: 14px; margin: -5px 0; border-radius: 7px; background: #60a5fa; }
    )"));
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

    QMenu* view_menu = menuBar()->addMenu(tr("视图(&V)"));
    view_menu->addAction(tr("适合窗口"), QKeySequence(Qt::Key_F), canvas_, &ImageCanvas::fitToView);

    QMenu* help_menu = menuBar()->addMenu(tr("帮助(&H)"));
    help_menu->addAction(tr("关于"), this, [this] {
        QMessageBox::about(this, tr("关于 CameraView"),
            tr("CameraView Qt\n\n基于 Qt 6 的 MUCam 工业相机预览、图像处理与显微测量应用。"));
    });

    QToolBar* toolbar = addToolBar(tr("主工具栏"));
    toolbar->setObjectName(QStringLiteral("MainToolbar"));
    toolbar->setMovable(false);
    toolbar->addAction(open_action);
    toolbar->addAction(export_action_);
    toolbar->addSeparator();
    toolbar->addAction(tr("适合窗口"), canvas_, &ImageCanvas::fitToView);
    toolbar->addAction(tr("长度"), this, [this] { setMeasurementTool(CanvasTool::Length, tr("请在图像上选择两个点")); });
    toolbar->addAction(tr("角度"), this, [this] { setMeasurementTool(CanvasTool::Angle, tr("请依次选择端点、顶点、端点")); });
}

QWidget* CameraMainWindow::buildCameraPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
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
    auto* layout = new QVBoxLayout(page);
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
    auto* layout = new QVBoxLayout(page);
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
    auto* layout = new QVBoxLayout(page);
    auto* stitch_group = new QGroupBox(tr("图像拼接"));
    auto* stitch_layout = new QVBoxLayout(stitch_group);
    stitch_count_label_ = new QLabel;
    stitch_layout->addWidget(stitch_count_label_);
    auto* add_tile = addButton(stitch_layout, tr("添加当前帧"));
    auto* import_tiles = addButton(stitch_layout, tr("导入多张图像…"));
    auto* stitch = addButton(stitch_layout, tr("生成拼接图"));
    layout->addWidget(stitch_group);

    auto* edf_group = new QGroupBox(tr("景深扩展 EDF"));
    auto* edf_layout = new QVBoxLayout(edf_group);
    edf_count_label_ = new QLabel;
    edf_layout->addWidget(edf_count_label_);
    auto* add_edf = addButton(edf_layout, tr("添加当前帧"));
    auto* import_edf = addButton(edf_layout, tr("导入焦平面图像…"));
    auto* build_edf = addButton(edf_layout, tr("生成 EDF 图"));
    focus_map_button_ = addButton(edf_layout, tr("显示焦点图"));
    focus_map_button_->setEnabled(false);
    layout->addWidget(edf_group);
    auto* clear = addButton(layout, tr("清空处理队列"));
    layout->addStretch();

    connect(add_tile, &QPushButton::clicked, this, &CameraMainWindow::addStitchTile);
    connect(import_tiles, &QPushButton::clicked, this, [this] {
        const QStringList files = QFileDialog::getOpenFileNames(
            this, tr("导入拼接图像"), {}, tr("图像 (*.bmp *.png *.jpg *.jpeg *.tif *.tiff)"));
        for (const QString& file : files) {
            QImageReader reader(file);
            const QImage image = reader.read();
            if (image.isNull()) {
                continue;
            }
            const ImageFrame previous = current_frame_;
            current_frame_ = imageFrameFromQImage(image);
            addStitchTile();
            current_frame_ = previous;
        }
        updateProcessingLabels();
    });
    connect(stitch, &QPushButton::clicked, this, &CameraMainWindow::buildStitch);
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
    auto* layout = new QVBoxLayout(page);
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
    auto* clear_calibration = new QPushButton(tr("清除标定"));
    calibration_form->addRow(buttonRow({calibrate, clear_calibration}));
    calibration_label_ = new QLabel(tr("未标定"));
    calibration_form->addRow(calibration_label_);
    layout->addWidget(calibration_group);

    auto* tool_group = new QGroupBox(tr("测量工具"));
    auto* tool_layout = new QVBoxLayout(tool_group);
    auto* length = new QPushButton(tr("长度"));
    auto* angle = new QPushButton(tr("角度"));
    auto* rectangle = new QPushButton(tr("矩形面积"));
    auto* polygon = new QPushButton(tr("多边形面积（双击完成）"));
    tool_layout->addWidget(buttonRow({length, angle}));
    tool_layout->addWidget(buttonRow({rectangle, polygon}));
    display_unit_combo_ = new QComboBox;
    display_unit_combo_->addItems({tr("像素"), tr("µm"), tr("mm")});
    display_unit_combo_->setCurrentIndex(1);
    tool_layout->addWidget(display_unit_combo_);
    layout->addWidget(tool_group);

    measurement_list_ = new QListWidget;
    layout->addWidget(measurement_list_, 1);
    auto* rename_selected = new QPushButton(tr("重命名"));
    auto* delete_selected = new QPushButton(tr("删除选中"));
    auto* clear = new QPushButton(tr("清空"));
    layout->addWidget(buttonRow({rename_selected, delete_selected, clear}));
    auto* export_csv = addButton(layout, tr("导出测量 CSV"));

    connect(calibrate, &QPushButton::clicked, this, [this] {
        setMeasurementTool(CanvasTool::Calibration, tr("请在图像上选择标定线的两个端点"));
    });
    connect(clear_calibration, &QPushButton::clicked, this, [this] {
        calibration_ = CalibrationProfile::Uncalibrated();
        calibration_label_->setText(tr("未标定"));
        updateMeasurementList();
    });
    connect(length, &QPushButton::clicked, this, [this] { setMeasurementTool(CanvasTool::Length, tr("请选择长度的两个端点")); });
    connect(angle, &QPushButton::clicked, this, [this] { setMeasurementTool(CanvasTool::Angle, tr("请选择端点、顶点和端点")); });
    connect(rectangle, &QPushButton::clicked, this, [this] { setMeasurementTool(CanvasTool::Rectangle, tr("请选择矩形的两个对角点")); });
    connect(polygon, &QPushButton::clicked, this, [this] { setMeasurementTool(CanvasTool::Polygon, tr("依次选择顶点，双击完成多边形")); });
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
    connect(measurement_list_, &QListWidget::itemDoubleClicked, this,
        [rename_current](QListWidgetItem*) { rename_current(); });
    connect(delete_selected, &QPushButton::clicked, this, &CameraMainWindow::deleteSelectedMeasurement);
    connect(clear, &QPushButton::clicked, this, &CameraMainWindow::clearMeasurements);
    connect(export_csv, &QPushButton::clicked, this, &CameraMainWindow::exportMeasurements);
    return page;
}

QWidget* CameraMainWindow::buildProjectPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* open = addButton(layout, tr("打开项目…"));
    auto* save = addButton(layout, tr("保存项目…"));
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
    export_overlays += ai_overlays_;
    for (const CanvasOverlay& overlay : export_overlays) {
        if (overlay.points.isEmpty()) {
            continue;
        }
        painter.setPen(QPen(overlay.color, pen_width));
        painter.setBrush(Qt::NoBrush);
        if (overlay.kind == CanvasTool::Rectangle && overlay.points.size() >= 2) {
            painter.drawRect(QRectF(overlay.points[0], overlay.points[1]).normalized());
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
    if (busy_) {
        return;
    }
    setCurrentFrame(imageFrameFromQImage(image, sequence, timestamp), tr("MUCam 实时预览"));
}

void CameraMainWindow::setCurrentFrame(
    ImageFrame frame,
    const QString& source,
    const QString& sourceIdentity)
{
    current_frame_ = std::move(frame);
    current_source_ = source;
    current_source_identity_ = sourceIdentity.isEmpty() ? source : sourceIdentity;
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
    if (ai_annotation_active_ && yolo_workspace_ &&
        (tool == CanvasTool::Rectangle || tool == CanvasTool::Polygon)) {
        ai_annotation_active_ = false;
        canvas_->setTool(CanvasTool::None);
        yolo_workspace_->acceptCanvasAnnotation(tool, std::move(points));
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
    } else if (tool == CanvasTool::Length && points.size() == 2) {
        measurements_.AddLength(
            MeasurementNameFormatter::NextDefaultName(MeasurementKind::Length, measurements_),
            imagePoint(points[0]), imagePoint(points[1]));
    } else if (tool == CanvasTool::Angle && points.size() == 3) {
        measurements_.AddAngle(
            MeasurementNameFormatter::NextDefaultName(MeasurementKind::Angle, measurements_),
            imagePoint(points[0]), imagePoint(points[1]), imagePoint(points[2]));
    } else if (tool == CanvasTool::Rectangle && points.size() == 2) {
        measurements_.AddRectangleArea(
            MeasurementNameFormatter::NextDefaultName(MeasurementKind::RectangleArea, measurements_),
            imagePoint(points[0]), imagePoint(points[1]));
    } else if (tool == CanvasTool::Polygon && points.size() >= 3) {
        std::vector<ImagePoint> polygon;
        polygon.reserve(static_cast<std::size_t>(points.size()));
        for (const QPointF& point : points) {
            polygon.push_back(imagePoint(point));
        }
        measurements_.AddPolygonArea(
            MeasurementNameFormatter::NextDefaultName(MeasurementKind::PolygonArea, measurements_),
            std::move(polygon));
    }
    canvas_->setTool(CanvasTool::None);
    updateMeasurementList();
}

void CameraMainWindow::setMeasurementTool(CanvasTool tool, const QString& hint)
{
    if (!currentVisibleFrame().IsValid()) {
        QMessageBox::information(this, tr("测量"), tr("请先打开图像或连接相机。"));
        return;
    }
    ai_annotation_active_ = false;
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
    if (measurement_list_->count() > 0) {
        measurement_list_->setCurrentRow(std::clamp(previous, 0, measurement_list_->count() - 1));
    }
    rebuildOverlays();
}

QVector<CanvasOverlay> CameraMainWindow::measurementOverlays() const
{
    QVector<CanvasOverlay> overlays;
    for (const LengthMeasurement& measurement : measurements_.Lengths()) {
        const MeasurementResult result = measurement.Evaluate(calibration_, display_unit_);
        overlays.push_back({CanvasTool::Length,
            {{measurement.First().x, measurement.First().y}, {measurement.Second().x, measurement.Second().y}},
            QString::fromStdWString(MeasurementFormatter::FormatResultLine(result)), QColor(76, 201, 240)});
    }
    for (const AngleMeasurement& measurement : measurements_.Angles()) {
        overlays.push_back({CanvasTool::Angle,
            {{measurement.First().x, measurement.First().y}, {measurement.Vertex().x, measurement.Vertex().y},
                {measurement.Second().x, measurement.Second().y}},
            QString::fromStdWString(MeasurementFormatter::FormatLine(measurement)), QColor(251, 146, 60)});
    }
    for (const RectangleAreaMeasurement& measurement : measurements_.Rectangles()) {
        overlays.push_back({CanvasTool::Rectangle,
            {{measurement.First().x, measurement.First().y}, {measurement.Second().x, measurement.Second().y}},
            QString::fromStdWString(MeasurementFormatter::FormatLine(measurement, calibration_, display_unit_)),
            QColor(74, 222, 128)});
    }
    for (const PolygonAreaMeasurement& measurement : measurements_.Polygons()) {
        QVector<QPointF> points;
        for (const ImagePoint& point : measurement.Points()) {
            points.push_back({point.x, point.y});
        }
        overlays.push_back({CanvasTool::Polygon, points,
            QString::fromStdWString(MeasurementFormatter::FormatLine(measurement, calibration_, display_unit_)),
            QColor(192, 132, 252)});
    }
    return overlays;
}

void CameraMainWindow::rebuildOverlays()
{
    QVector<CanvasOverlay> overlays = measurementOverlays();
    overlays += ai_overlays_;
    canvas_->setOverlays(std::move(overlays));
}

void CameraMainWindow::clearMeasurements()
{
    measurements_.Clear();
    updateMeasurementList();
    statusBar()->showMessage(tr("测量结果已清空"), 3000);
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
    if (!current_frame_.IsValid()) {
        QMessageBox::information(this, tr("图像拼接"), tr("当前没有可添加的图像帧。"));
        return;
    }
    StitchTile tile;
    tile.frame = current_frame_;
    if (!stitch_tiles_.empty()) {
        const int columns = 4;
        const int index = static_cast<int>(stitch_tiles_.size());
        tile.offset_x = (index % columns) * std::max(1, current_frame_.width * 3 / 4);
        tile.offset_y = (index / columns) * std::max(1, current_frame_.height * 3 / 4);
        tile.estimated_position = true;
    }
    stitch_tiles_.push_back(std::move(tile));
    updateProcessingLabels();
    statusBar()->showMessage(tr("已添加拼接帧，共 %1 帧").arg(stitch_tiles_.size()), 3000);
}

void CameraMainWindow::buildStitch()
{
    if (stitch_tiles_.size() < 2) {
        QMessageBox::information(this, tr("图像拼接"), tr("至少需要两帧图像。"));
        return;
    }
    const std::vector<StitchTile> tiles = stitch_tiles_;
    setBusy(true, tr("正在后台生成拼接图…"));
    auto* watcher = new QFutureWatcher<ImageFrame>(this);
    connect(watcher, &QFutureWatcher<ImageFrame>::finished, this, [this, watcher] {
        const ImageFrame result = watcher->result();
        watcher->deleteLater();
        setBusy(false, result.IsValid() ? tr("拼接完成") : tr("拼接失败"));
        if (result.IsValid()) {
            setCurrentFrame(result, tr("拼接结果"));
        } else {
            QMessageBox::warning(this, tr("图像拼接"), tr("未能生成有效的拼接图。"));
        }
    });
    watcher->setFuture(QtConcurrent::run([tiles] {
        return ImageStitcher::StitchAverage(tiles, StitchBlendMode::Linear);
    }));
}

void CameraMainWindow::addEdfFrame()
{
    const EdfStackListActionResult result = EdfStackListActions::AddCurrentFrame(edf_stack_, current_frame_);
    updateProcessingLabels();
    statusBar()->showMessage(QString::fromStdWString(result.message), 3000);
}

void CameraMainWindow::buildEdf()
{
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
    stitch_tiles_.clear();
    edf_stack_.clear();
    edf_result_ = {};
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
    function_tabs_->setEnabled(!busy);
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

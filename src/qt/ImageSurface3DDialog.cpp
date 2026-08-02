#include "ImageSurface3DDialog.h"

#include "ImageSurface3DWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

ImageSurface3DDialog::ImageSurface3DDialog(
    const QImage& image,
    const QString& sourceName,
    QWidget* parent)
    : QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("图像 3D 高度图 · %1").arg(sourceName.isEmpty() ? tr("当前图像") : sourceName));
    resize(1120, 760);
    setMinimumSize(820, 560);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);
    surface_ = new ImageSurface3DWidget;
    surface_->setImage(image);
    root->addWidget(surface_, 1);

    auto* controls = new QGroupBox(tr("3D 显示设置"));
    controls->setMinimumWidth(260);
    controls->setMaximumWidth(320);
    auto* controls_layout = new QVBoxLayout(controls);
    auto* form = new QFormLayout;
    auto* height_slider = new QSlider(Qt::Horizontal);
    height_slider->setObjectName(QStringLiteral("SurfaceHeightSlider"));
    height_slider->setRange(5, 500);
    height_slider->setValue(125);
    auto* height_value = new QLabel(QStringLiteral("1.25×"));
    auto* height_row = new QWidget;
    auto* height_layout = new QHBoxLayout(height_row);
    height_layout->setContentsMargins(0, 0, 0, 0);
    height_layout->addWidget(height_slider, 1);
    height_layout->addWidget(height_value);
    auto* resolution_spin = new QSpinBox;
    resolution_spin->setObjectName(QStringLiteral("SurfaceResolutionSpin"));
    resolution_spin->setRange(16, 180);
    resolution_spin->setValue(80);
    resolution_spin->setSuffix(tr(" 点"));
    auto* height_channel_combo = new QComboBox;
    height_channel_combo->setObjectName(QStringLiteral("SurfaceHeightChannelCombo"));
    height_channel_combo->addItem(tr("亮度"), static_cast<int>(SurfaceHeightChannel::Luminance));
    height_channel_combo->addItem(tr("红色通道"), static_cast<int>(SurfaceHeightChannel::Red));
    height_channel_combo->addItem(tr("绿色通道"), static_cast<int>(SurfaceHeightChannel::Green));
    height_channel_combo->addItem(tr("蓝色通道"), static_cast<int>(SurfaceHeightChannel::Blue));
    auto* color_combo = new QComboBox;
    color_combo->setObjectName(QStringLiteral("SurfaceColorCombo"));
    color_combo->addItem(tr("高度伪彩"), static_cast<int>(SurfaceColorMode::HeightMap));
    color_combo->addItem(tr("原图颜色"), static_cast<int>(SurfaceColorMode::Original));
    color_combo->addItem(tr("灰度"), static_cast<int>(SurfaceColorMode::Grayscale));
    auto* mesh_check = new QCheckBox(tr("显示网格线"));
    mesh_check->setChecked(true);
    auto* backend_label = new QLabel(surface_->renderBackend());
    backend_label->setObjectName(QStringLiteral("SurfaceRenderBackend"));
    backend_label->setWordWrap(true);
    backend_label->setStyleSheet(QStringLiteral("color: #93a4b8;"));
    form->addRow(tr("高度倍率"), height_row);
    form->addRow(tr("网格精度"), resolution_spin);
    form->addRow(tr("高度通道"), height_channel_combo);
    form->addRow(tr("表面着色"), color_combo);
    form->addRow(QString(), mesh_check);
    form->addRow(tr("渲染后端"), backend_label);
    controls_layout->addLayout(form);

    auto* note = new QLabel(tr(
        "高度由所选亮度或 RGB 通道映射，用于分别观察各通道的纹理、边缘和强度起伏；"
        "它不是没有深度标定时的真实物理高度。"));
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #93a4b8;"));
    controls_layout->addWidget(note);
    controls_layout->addStretch();
    auto* reset = new QPushButton(tr("复位视角"));
    auto* export_button = new QPushButton(tr("导出当前视图…"));
    export_button->setProperty("role", QStringLiteral("primary"));
    auto* close_button = new QPushButton(tr("关闭"));
    controls_layout->addWidget(reset);
    controls_layout->addWidget(export_button);
    controls_layout->addWidget(close_button);
    root->addWidget(controls);

    connect(height_slider, &QSlider::valueChanged, this, [this, height_value](int value) {
        const double scale = value / 100.0;
        height_value->setText(QStringLiteral("%1×").arg(scale, 0, 'f', 2));
        surface_->setVerticalScale(scale);
    });
    connect(resolution_spin, qOverload<int>(&QSpinBox::valueChanged),
        surface_, &ImageSurface3DWidget::setResolution);
    connect(height_channel_combo, qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this, height_channel_combo] {
            surface_->setHeightChannel(static_cast<SurfaceHeightChannel>(
                height_channel_combo->currentData().toInt()));
        });
    connect(color_combo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, color_combo] {
        surface_->setColorMode(static_cast<SurfaceColorMode>(color_combo->currentData().toInt()));
    });
    connect(mesh_check, &QCheckBox::toggled, surface_, &ImageSurface3DWidget::setMeshVisible);
    connect(surface_, &ImageSurface3DWidget::renderBackendChanged,
        this, [backend_label](const QString& description, bool hardware) {
            backend_label->setText(hardware
                ? QObject::tr("%1（硬件加速）").arg(description)
                : QObject::tr("%1（软件回退）").arg(description));
        });
    connect(reset, &QPushButton::clicked, surface_, &ImageSurface3DWidget::resetView);
    connect(export_button, &QPushButton::clicked, this, &ImageSurface3DDialog::exportView);
    connect(close_button, &QPushButton::clicked, this, &QDialog::close);
}

void ImageSurface3DDialog::exportView()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("导出 3D 视图"), QStringLiteral("CameraView-3D.png"), tr("PNG 图像 (*.png)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".png");
    if (!surface_->grab().save(path, "PNG")) {
        QMessageBox::warning(this, tr("导出失败"), tr("无法保存 3D 视图。"));
    }
}

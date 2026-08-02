#include "ImageSurface3DWidget.h"

#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

struct ProjectedPoint {
    QPointF point;
    double depth = 0.0;
};

struct SurfaceFace {
    std::array<QPointF, 4> points;
    QColor color;
    double depth = 0.0;
};

constexpr int kInteractiveFaceBudget = 6000;

} // namespace

ImageSurface3DWidget::ImageSurface3DWidget(QWidget* parent) : QOpenGLWidget(parent)
{
    setObjectName(QStringLiteral("ImageSurface3D"));
    setMinimumSize(640, 440);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::OpenHandCursor);
}

void ImageSurface3DWidget::initializeGL()
{
    QOpenGLFunctions* functions = context() ? context()->functions() : nullptr;
    const char* renderer = functions
        ? reinterpret_cast<const char*>(functions->glGetString(GL_RENDERER))
        : nullptr;
    const QString renderer_name = renderer ? QString::fromLatin1(renderer) : tr("不可用");
    render_backend_ = tr("OpenGL · %1").arg(renderer_name);
    const QString lowered = renderer_name.toLower();
    hardware_accelerated_ = renderer &&
        !lowered.contains(QStringLiteral("software")) &&
        !lowered.contains(QStringLiteral("llvmpipe")) &&
        !lowered.contains(QStringLiteral("swiftshader")) &&
        !lowered.contains(QStringLiteral("gdi generic")) &&
        !lowered.contains(QStringLiteral("microsoft basic render"));
    emit renderBackendChanged(render_backend_, hardware_accelerated_);
}

void ImageSurface3DWidget::setImage(const QImage& image)
{
    source_ = image.convertToFormat(QImage::Format_RGB32);
    rebuildSurface();
}

void ImageSurface3DWidget::setVerticalScale(double scale)
{
    vertical_scale_ = std::clamp(scale, 0.05, 6.0);
    update();
}

void ImageSurface3DWidget::setResolution(int resolution)
{
    const int normalized = std::clamp(resolution, 16, 180);
    if (resolution_ == normalized) return;
    resolution_ = normalized;
    rebuildSurface();
}

void ImageSurface3DWidget::setHeightChannel(SurfaceHeightChannel channel)
{
    if (height_channel_ == channel) return;
    height_channel_ = channel;
    updateHeights();
}

void ImageSurface3DWidget::setColorMode(SurfaceColorMode mode)
{
    color_mode_ = mode;
    update();
}

void ImageSurface3DWidget::setMeshVisible(bool visible)
{
    mesh_visible_ = visible;
    update();
}

void ImageSurface3DWidget::resetView()
{
    yaw_degrees_ = -35.0;
    pitch_degrees_ = 52.0;
    view_scale_ = 1.0;
    pan_ = {};
    update();
}

float ImageSurface3DWidget::heightAt(int column, int row) const
{
    if (column < 0 || column >= columns_ || row < 0 || row >= rows_) return 0.0F;
    return heights_.at(row * columns_ + column);
}

void ImageSurface3DWidget::rebuildSurface()
{
    heights_.clear();
    original_colors_.clear();
    columns_ = 0;
    rows_ = 0;
    if (source_.isNull() || source_.width() < 2 || source_.height() < 2) {
        update();
        return;
    }

    if (source_.width() >= source_.height()) {
        columns_ = std::min(resolution_, source_.width());
        rows_ = std::max(2, qRound(columns_ * source_.height() / static_cast<double>(source_.width())));
    } else {
        rows_ = std::min(resolution_, source_.height());
        columns_ = std::max(2, qRound(rows_ * source_.width() / static_cast<double>(source_.height())));
    }
    original_colors_.reserve(columns_ * rows_);
    for (int row = 0; row < rows_; ++row) {
        const int y = qRound(row * (source_.height() - 1) / static_cast<double>(rows_ - 1));
        for (int column = 0; column < columns_; ++column) {
            const int x = qRound(column * (source_.width() - 1) / static_cast<double>(columns_ - 1));
            const QColor color = source_.pixelColor(x, y);
            original_colors_.push_back(color);
        }
    }
    updateHeights();
}

void ImageSurface3DWidget::updateHeights()
{
    heights_.clear();
    heights_.reserve(original_colors_.size());
    for (const QColor& color : original_colors_) {
        float height = 0.0F;
        switch (height_channel_) {
        case SurfaceHeightChannel::Red:
            height = static_cast<float>(color.redF());
            break;
        case SurfaceHeightChannel::Green:
            height = static_cast<float>(color.greenF());
            break;
        case SurfaceHeightChannel::Blue:
            height = static_cast<float>(color.blueF());
            break;
        case SurfaceHeightChannel::Luminance:
        default:
            height = static_cast<float>(
                (0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue()) / 255.0);
            break;
        }
        heights_.push_back(height);
    }
    update();
}

QColor ImageSurface3DWidget::surfaceColor(int index) const
{
    const double height = std::clamp(static_cast<double>(heights_.at(index)), 0.0, 1.0);
    switch (color_mode_) {
    case SurfaceColorMode::Original:
        return original_colors_.at(index);
    case SurfaceColorMode::Grayscale: {
        const int value = qRound(height * 255.0);
        return QColor(value, value, value);
    }
    case SurfaceColorMode::HeightMap:
    default:
        return QColor::fromHsvF((1.0 - height) * 0.68, 0.86, 0.95);
    }
}

void ImageSurface3DWidget::paintGL()
{
    QPainter painter(this);
    const bool interactive = drag_button_ != Qt::NoButton;
    painter.setRenderHint(QPainter::Antialiasing, !interactive);
    QLinearGradient background(rect().topLeft(), rect().bottomRight());
    background.setColorAt(0.0, QColor(9, 14, 20));
    background.setColorAt(1.0, QColor(19, 28, 38));
    painter.fillRect(rect(), background);

    if (!hasSurface()) {
        painter.setPen(QColor(135, 149, 168));
        painter.drawText(rect(), Qt::AlignCenter, tr("没有可显示的图像表面"));
        return;
    }

    const double yaw = yaw_degrees_ * kPi / 180.0;
    const double pitch = pitch_degrees_ * kPi / 180.0;
    const double cos_yaw = std::cos(yaw);
    const double sin_yaw = std::sin(yaw);
    const double cos_pitch = std::cos(pitch);
    const double sin_pitch = std::sin(pitch);
    const double aspect = source_.width() / static_cast<double>(source_.height());
    const double extent_x = aspect >= 1.0 ? aspect : 1.0;
    const double extent_z = aspect >= 1.0 ? 1.0 : 1.0 / aspect;
    const double projection_scale = std::min(width(), height()) * 1.45 * view_scale_;
    const QPointF center = rect().center() + pan_ + QPointF(0.0, height() * 0.08);

    auto project = [&](int column, int row) {
        const int index = row * columns_ + column;
        const double x = (column / static_cast<double>(columns_ - 1) - 0.5) * 2.0 * extent_x;
        const double z = (row / static_cast<double>(rows_ - 1) - 0.5) * 2.0 * extent_z;
        const double y = heights_.at(index) * vertical_scale_;
        const double x_yaw = cos_yaw * x - sin_yaw * z;
        const double z_yaw = sin_yaw * x + cos_yaw * z;
        const double y_pitch = cos_pitch * y - sin_pitch * z_yaw;
        const double z_pitch = sin_pitch * y + cos_pitch * z_yaw;
        const double camera_depth = std::max(1.2, 5.0 + z_pitch);
        return ProjectedPoint{
            center + QPointF(
                x_yaw * projection_scale / camera_depth,
                -y_pitch * projection_scale / camera_depth),
            camera_depth};
    };

    QVector<ProjectedPoint> projected;
    projected.resize(columns_ * rows_);
    QVector<QColor> vertex_colors;
    vertex_colors.resize(columns_ * rows_);
    for (int row = 0; row < rows_; ++row) {
        for (int column = 0; column < columns_; ++column) {
            const int index = row * columns_ + column;
            projected[index] = project(column, row);
            vertex_colors[index] = surfaceColor(index);
        }
    }

    const int full_face_count = (columns_ - 1) * (rows_ - 1);
    int stride = 1;
    if (interactive && full_face_count > kInteractiveFaceBudget) {
        stride = std::max(2, static_cast<int>(std::ceil(
            std::sqrt(full_face_count / static_cast<double>(kInteractiveFaceBudget)))));
    }
    QVector<int> sampled_columns;
    QVector<int> sampled_rows;
    for (int column = 0; column < columns_ - 1; column += stride) sampled_columns.push_back(column);
    for (int row = 0; row < rows_ - 1; row += stride) sampled_rows.push_back(row);
    sampled_columns.push_back(columns_ - 1);
    sampled_rows.push_back(rows_ - 1);

    QVector<SurfaceFace> faces;
    faces.reserve((sampled_columns.size() - 1) * (sampled_rows.size() - 1));
    for (int row_index = 0; row_index + 1 < sampled_rows.size(); ++row_index) {
        const int top = sampled_rows.at(row_index);
        const int bottom = sampled_rows.at(row_index + 1);
        for (int column_index = 0; column_index + 1 < sampled_columns.size(); ++column_index) {
            const int left = sampled_columns.at(column_index);
            const int right = sampled_columns.at(column_index + 1);
            const int indices[] = {
                top * columns_ + left,
                top * columns_ + right,
                bottom * columns_ + right,
                bottom * columns_ + left};
            const ProjectedPoint& a = projected.at(indices[0]);
            const ProjectedPoint& b = projected.at(indices[1]);
            const ProjectedPoint& c = projected.at(indices[2]);
            const ProjectedPoint& d = projected.at(indices[3]);
            QColor color(
                (vertex_colors.at(indices[0]).red() + vertex_colors.at(indices[1]).red() +
                    vertex_colors.at(indices[2]).red() + vertex_colors.at(indices[3]).red()) / 4,
                (vertex_colors.at(indices[0]).green() + vertex_colors.at(indices[1]).green() +
                    vertex_colors.at(indices[2]).green() + vertex_colors.at(indices[3]).green()) / 4,
                (vertex_colors.at(indices[0]).blue() + vertex_colors.at(indices[1]).blue() +
                    vertex_colors.at(indices[2]).blue() + vertex_colors.at(indices[3]).blue()) / 4);
            const double facing = std::abs(
                (b.point.x() - a.point.x()) * (c.point.y() - a.point.y()) -
                (b.point.y() - a.point.y()) * (c.point.x() - a.point.x()));
            if (facing < 0.5) color = color.darker(125);
            faces.push_back({
                {a.point, b.point, c.point, d.point},
                color,
                (a.depth + b.depth + c.depth + d.depth) / 4.0});
        }
    }
    last_render_stride_ = stride;
    last_rendered_face_count_ = faces.size();
    std::sort(faces.begin(), faces.end(), [](const SurfaceFace& left, const SurfaceFace& right) {
        return left.depth > right.depth;
    });
    painter.setPen(mesh_visible_ ? QPen(QColor(11, 19, 28, interactive ? 45 : 75), 0.6) : Qt::NoPen);
    for (const SurfaceFace& face : faces) {
        painter.setBrush(face.color);
        painter.drawPolygon(face.points.data(), static_cast<int>(face.points.size()));
    }

    const QRect legend(width() - 34, 34, 12, std::max(100, height() / 3));
    for (int y = 0; y < legend.height(); ++y) {
        const double value = 1.0 - y / static_cast<double>(legend.height() - 1);
        QColor color = color_mode_ == SurfaceColorMode::HeightMap
            ? QColor::fromHsvF((1.0 - value) * 0.68, 0.86, 0.95)
            : QColor::fromRgbF(value, value, value);
        painter.setPen(color);
        painter.drawLine(legend.left(), legend.top() + y, legend.right(), legend.top() + y);
    }
    painter.setPen(QColor(218, 226, 238));
    painter.drawRect(legend);
    painter.drawText(QPoint(legend.left() - 6, legend.top() - 7), QStringLiteral("255"));
    painter.drawText(QPoint(legend.left(), legend.bottom() + 16), QStringLiteral("0"));
    painter.setPen(QColor(138, 151, 169));
    painter.drawText(
        QRect(14, height() - 30, width() - 28, 20),
        Qt::AlignLeft | Qt::AlignVCenter,
        tr("左键旋转 · 右键平移 · 滚轮缩放 · 双击复位"));
}

void ImageSurface3DWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton ||
        event->button() == Qt::MiddleButton) {
        drag_button_ = event->button();
        last_mouse_ = event->pos();
        setCursor(event->button() == Qt::LeftButton ? Qt::ClosedHandCursor : Qt::SizeAllCursor);
    }
}

void ImageSurface3DWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (drag_button_ == Qt::NoButton) return;
    const QPoint delta = event->pos() - last_mouse_;
    last_mouse_ = event->pos();
    if (drag_button_ == Qt::LeftButton) {
        yaw_degrees_ += delta.x() * 0.55;
        pitch_degrees_ = std::clamp(pitch_degrees_ + delta.y() * 0.4, 5.0, 85.0);
    } else {
        pan_ += QPointF(delta);
    }
    update();
}

void ImageSurface3DWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == drag_button_) {
        drag_button_ = Qt::NoButton;
        setCursor(Qt::OpenHandCursor);
        update();
    }
}

void ImageSurface3DWidget::mouseDoubleClickEvent(QMouseEvent*)
{
    resetView();
}

void ImageSurface3DWidget::wheelEvent(QWheelEvent* event)
{
    const double factor = event->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12;
    view_scale_ = std::clamp(view_scale_ * factor, 0.35, 4.0);
    update();
    event->accept();
}

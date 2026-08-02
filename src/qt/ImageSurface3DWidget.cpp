#include "ImageSurface3DWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

struct ProjectedPoint {
    QPointF point;
    double depth = 0.0;
};

struct SurfaceFace {
    QPolygonF polygon;
    QColor color;
    double depth = 0.0;
};

} // namespace

ImageSurface3DWidget::ImageSurface3DWidget(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("ImageSurface3D"));
    setMinimumSize(640, 440);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::OpenHandCursor);
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
    heights_.reserve(columns_ * rows_);
    original_colors_.reserve(columns_ * rows_);
    for (int row = 0; row < rows_; ++row) {
        const int y = qRound(row * (source_.height() - 1) / static_cast<double>(rows_ - 1));
        for (int column = 0; column < columns_; ++column) {
            const int x = qRound(column * (source_.width() - 1) / static_cast<double>(columns_ - 1));
            const QColor color = source_.pixelColor(x, y);
            const float luminance = static_cast<float>(
                (0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue()) / 255.0);
            heights_.push_back(luminance);
            original_colors_.push_back(color);
        }
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

void ImageSurface3DWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
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

    QVector<SurfaceFace> faces;
    faces.reserve((columns_ - 1) * (rows_ - 1) * 2);
    auto append_face = [&](int a_column, int a_row, int b_column, int b_row, int c_column, int c_row) {
        const ProjectedPoint a = project(a_column, a_row);
        const ProjectedPoint b = project(b_column, b_row);
        const ProjectedPoint c = project(c_column, c_row);
        const int ai = a_row * columns_ + a_column;
        const int bi = b_row * columns_ + b_column;
        const int ci = c_row * columns_ + c_column;
        QColor color(
            (surfaceColor(ai).red() + surfaceColor(bi).red() + surfaceColor(ci).red()) / 3,
            (surfaceColor(ai).green() + surfaceColor(bi).green() + surfaceColor(ci).green()) / 3,
            (surfaceColor(ai).blue() + surfaceColor(bi).blue() + surfaceColor(ci).blue()) / 3);
        const double facing = std::abs(
            (b.point.x() - a.point.x()) * (c.point.y() - a.point.y()) -
            (b.point.y() - a.point.y()) * (c.point.x() - a.point.x()));
        color = facing < 0.5 ? color.darker(125) : color;
        faces.push_back({QPolygonF{a.point, b.point, c.point}, color, (a.depth + b.depth + c.depth) / 3.0});
    };
    for (int row = 0; row + 1 < rows_; ++row) {
        for (int column = 0; column + 1 < columns_; ++column) {
            append_face(column, row, column + 1, row, column + 1, row + 1);
            append_face(column, row, column + 1, row + 1, column, row + 1);
        }
    }
    std::sort(faces.begin(), faces.end(), [](const SurfaceFace& left, const SurfaceFace& right) {
        return left.depth > right.depth;
    });
    painter.setPen(mesh_visible_ ? QPen(QColor(11, 19, 28, 75), 0.6) : Qt::NoPen);
    for (const SurfaceFace& face : faces) {
        painter.setBrush(face.color);
        painter.drawPolygon(face.polygon);
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

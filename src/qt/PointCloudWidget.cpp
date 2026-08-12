#include "PointCloudWidget.h"

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
constexpr int kRenderPointBudget = 120000;

} // namespace

PointCloudWidget::PointCloudWidget(QWidget* parent) : QOpenGLWidget(parent)
{
    setObjectName(QStringLiteral("PointCloudView"));
    setMinimumSize(640, 440);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
}

void PointCloudWidget::initializeGL()
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

void PointCloudWidget::setCloud(const PointCloud& cloud)
{
    cloud_ = cloud;
    if (!cloud_.Empty() && !cloud_.bounds.valid) cloud_.RecalculateBounds();
    highlighted_indices_.clear();
    selection_preview_indices_.clear();
    resetView();
}

void PointCloudWidget::setColorMode(PointCloudColorMode mode)
{
    color_mode_ = mode;
    update();
}

void PointCloudWidget::setPointSize(double size)
{
    point_size_ = std::clamp(size, 1.0, 12.0);
    update();
}

void PointCloudWidget::setAxesVisible(bool visible)
{
    axes_visible_ = visible;
    update();
}

void PointCloudWidget::setPickingEnabled(bool enabled)
{
    picking_enabled_ = enabled;
    if (enabled) box_selection_enabled_ = false;
    setCursor(enabled || box_selection_enabled_ ? Qt::CrossCursor : Qt::OpenHandCursor);
}

void PointCloudWidget::setBoxSelectionEnabled(bool enabled)
{
    box_selection_enabled_ = enabled;
    if (enabled) picking_enabled_ = false;
    box_selection_rect_ = {};
    setCursor(enabled ? Qt::CrossCursor : Qt::OpenHandCursor);
    update();
}

void PointCloudWidget::setHighlightedIndices(const QVector<int>& indices)
{
    highlighted_indices_ = indices;
    update();
}

void PointCloudWidget::setSelectionPreviewIndices(const QVector<int>& indices)
{
    selection_preview_indices_ = indices;
    update();
}

void PointCloudWidget::resetView()
{
    yaw_degrees_ = -38.0;
    pitch_degrees_ = 26.0;
    view_scale_ = 1.0;
    pan_ = {};
    update();
}

PointCloudWidget::ProjectedPoint PointCloudWidget::projectPoint(int index) const
{
    if (index < 0 || index >= static_cast<int>(cloud_.points.size()) ||
        !cloud_.bounds.valid) return {};
    return projectPointValue(cloud_.points[static_cast<std::size_t>(index)], index);
}

PointCloudWidget::ProjectedPoint PointCloudWidget::projectPointValue(
    const PointCloudPoint& point,
    int index) const
{
    if (!cloud_.bounds.valid) return {};
    const PointCloudCentroid center{
        (cloud_.bounds.min_x + cloud_.bounds.max_x) * 0.5,
        (cloud_.bounds.min_y + cloud_.bounds.max_y) * 0.5,
        (cloud_.bounds.min_z + cloud_.bounds.max_z) * 0.5};
    const double extent = std::max({
        cloud_.bounds.Width(), cloud_.bounds.Depth(), cloud_.bounds.Height(), 1e-12});
    const double x = (point.x - center.x) / extent * 2.0;
    const double y = (point.y - center.y) / extent * 2.0;
    const double z = (point.z - center.z) / extent * 2.0;
    const double yaw = yaw_degrees_ * kPi / 180.0;
    const double pitch = pitch_degrees_ * kPi / 180.0;
    const double cos_yaw = std::cos(yaw);
    const double sin_yaw = std::sin(yaw);
    const double cos_pitch = std::cos(pitch);
    const double sin_pitch = std::sin(pitch);
    const double rotated_x = cos_yaw * x - sin_yaw * y;
    const double rotated_y = sin_yaw * x + cos_yaw * y;
    const double screen_y = cos_pitch * z - sin_pitch * rotated_y;
    const double depth = sin_pitch * z + cos_pitch * rotated_y;
    const double camera_depth = std::max(1.0, 5.0 + depth);
    const double scale = std::min(width(), height()) * 1.85 * view_scale_;
    const QPointF viewport_center = rect().center() + pan_;
    return {
        viewport_center + QPointF(
            rotated_x * scale / camera_depth,
            -screen_y * scale / camera_depth),
        camera_depth,
        index};
}

QColor PointCloudWidget::pointColor(const PointCloudPoint& point) const
{
    if (color_mode_ == PointCloudColorMode::Original && point.has_color) {
        return QColor(point.r, point.g, point.b);
    }
    if (color_mode_ == PointCloudColorMode::Solid) return QColor(98, 178, 255);
    const double range = cloud_.bounds.Height();
    const double normalized = range > 1e-15
        ? std::clamp((point.z - cloud_.bounds.min_z) / range, 0.0, 1.0)
        : 0.5;
    return QColor::fromHsvF((1.0 - normalized) * 0.68, 0.86, 0.98);
}

void PointCloudWidget::paintGL()
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, drag_button_ == Qt::NoButton);
    QLinearGradient background(rect().topLeft(), rect().bottomRight());
    background.setColorAt(0.0, QColor(8, 13, 19));
    background.setColorAt(1.0, QColor(19, 29, 40));
    painter.fillRect(rect(), background);
    projected_points_.clear();
    rendered_point_count_ = 0;
    if (!hasCloud()) {
        painter.setPen(QColor(139, 154, 174));
        painter.drawText(rect(), Qt::AlignCenter,
            tr("打开 PLY、PCD、XYZ、TXT 或 CSV 点云"));
        return;
    }

    const int stride = std::max(1, static_cast<int>(std::ceil(
        cloud_.points.size() / static_cast<double>(kRenderPointBudget))));
    projected_points_.reserve(
        static_cast<int>((cloud_.points.size() + stride - 1) / stride));
    for (int index = 0; index < static_cast<int>(cloud_.points.size()); index += stride) {
        projected_points_.push_back(projectPoint(index));
    }
    std::sort(projected_points_.begin(), projected_points_.end(),
        [](const ProjectedPoint& left, const ProjectedPoint& right) {
            return left.depth > right.depth;
        });
    painter.setPen(Qt::NoPen);
    for (const ProjectedPoint& projected : projected_points_) {
        painter.setBrush(pointColor(cloud_.points[static_cast<std::size_t>(projected.index)]));
        painter.drawEllipse(projected.position, point_size_, point_size_);
    }
    rendered_point_count_ = projected_points_.size();

    if (axes_visible_) {
        const PointCloudCentroid center{
            (cloud_.bounds.min_x + cloud_.bounds.max_x) * 0.5,
            (cloud_.bounds.min_y + cloud_.bounds.max_y) * 0.5,
            (cloud_.bounds.min_z + cloud_.bounds.max_z) * 0.5};
        PointCloudPoint origin{center.x, center.y, center.z};
        const double extent = std::max({cloud_.bounds.Width(), cloud_.bounds.Depth(),
            cloud_.bounds.Height(), 1e-12});
        const std::array<PointCloudPoint, 3> ends{{
            {center.x + extent * 0.3, center.y, center.z},
            {center.x, center.y + extent * 0.3, center.z},
            {center.x, center.y, center.z + extent * 0.3}}};
        const QPointF start = projectPointValue(origin).position;
        const QColor colors[]{QColor(255, 92, 92), QColor(80, 225, 135), QColor(80, 150, 255)};
        const QString labels[]{QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")};
        for (int axis = 0; axis < 3; ++axis) {
            const QPointF end = projectPointValue(ends[axis]).position;
            painter.setPen(QPen(colors[axis], 2.0));
            painter.drawLine(start, end);
            painter.drawText(end + QPointF(4.0, -4.0), labels[axis]);
        }
    }

    if (!highlighted_indices_.isEmpty()) {
        QPainterPath path;
        bool first = true;
        for (int index : highlighted_indices_) {
            const QPointF position = projectPoint(index).position;
            if (first) {
                path.moveTo(position);
                first = false;
            } else {
                path.lineTo(position);
            }
        }
        painter.setPen(QPen(QColor(255, 223, 92), 2.0));
        painter.setBrush(QColor(255, 223, 92));
        painter.drawPath(path);
        for (int index : highlighted_indices_) {
            painter.drawEllipse(projectPoint(index).position, 5.0, 5.0);
        }
    }


    if (!selection_preview_indices_.isEmpty()) {
        const int stride = std::max(1, static_cast<int>(std::ceil(
            selection_preview_indices_.size() / static_cast<double>(kRenderPointBudget))));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 211, 74, 220));
        for (int offset = 0; offset < selection_preview_indices_.size(); offset += stride) {
            const int index = selection_preview_indices_[offset];
            if (index >= 0 && index < static_cast<int>(cloud_.points.size())) {
                painter.drawEllipse(projectPoint(index).position,
                    point_size_ + 1.5, point_size_ + 1.5);
            }
        }
    }

    if (box_selection_enabled_ && !box_selection_rect_.isEmpty()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 211, 74, 190));
        for (const ProjectedPoint& projected : projected_points_) {
            if (box_selection_rect_.contains(projected.position)) {
                painter.drawEllipse(projected.position, point_size_ + 1.5, point_size_ + 1.5);
            }
        }
        painter.setPen(QPen(QColor(255, 211, 74), 2.0, Qt::DashLine));
        painter.setBrush(QColor(255, 211, 74, 28));
        painter.drawRect(box_selection_rect_);
    }

    painter.setPen(QColor(140, 155, 175));
    painter.drawText(QRect(14, height() - 30, width() - 28, 20),
        Qt::AlignLeft | Qt::AlignVCenter,
        box_selection_enabled_
            ? tr("拖出矩形选择点 · 右键拖动平移 · 滚轮缩放")
            : picking_enabled_
            ? tr("单击拾取点 · 滚轮缩放 · 右键拖动平移")
            : tr("左键拖动旋转 · 右键拖动平移 · 滚轮缩放 · 双击复位"));
}

QVector<int> PointCloudWidget::indicesInScreenRect(const QRectF& rectangle) const
{
    QVector<int> indices;
    if (!hasCloud() || rectangle.isEmpty()) return indices;
    const QRectF normalized = rectangle.normalized();
    indices.reserve(static_cast<int>(cloud_.points.size() / 4));
    for (int index = 0; index < static_cast<int>(cloud_.points.size()); ++index) {
        if (normalized.contains(projectPoint(index).position)) indices.push_back(index);
    }
    return indices;
}

int PointCloudWidget::pickNearest(const QPointF& position, double radius) const
{
    int selected = -1;
    double best_distance = radius * radius;
    // Rendering is sampled for very large clouds, but measurement must remain exact:
    // project every source point only when the user explicitly performs a pick.
    for (int index = 0; index < static_cast<int>(cloud_.points.size()); ++index) {
        const ProjectedPoint point = projectPoint(index);
        const QPointF delta = point.position - position;
        const double distance = delta.x() * delta.x() + delta.y() * delta.y();
        if (distance <= best_distance) {
            best_distance = distance;
            selected = index;
        }
    }
    return selected;
}

QPointF PointCloudWidget::screenPosition(int point_index) const
{
    return projectPoint(point_index).position;
}

void PointCloudWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton &&
        event->button() != Qt::MiddleButton) return;
    drag_button_ = event->button();
    press_position_ = last_mouse_ = event->pos();
    moved_since_press_ = false;
    if (box_selection_enabled_ && event->button() == Qt::LeftButton) {
        box_selection_start_ = event->position();
        box_selection_rect_ = QRectF(box_selection_start_, box_selection_start_);
        update();
        return;
    }
    if (!picking_enabled_ || event->button() != Qt::LeftButton) {
        setCursor(event->button() == Qt::LeftButton ? Qt::ClosedHandCursor : Qt::SizeAllCursor);
    }
}

void PointCloudWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (drag_button_ == Qt::NoButton) return;
    const QPoint delta = event->pos() - last_mouse_;
    last_mouse_ = event->pos();
    moved_since_press_ = moved_since_press_ ||
        (event->pos() - press_position_).manhattanLength() > 4;
    if (box_selection_enabled_ && drag_button_ == Qt::LeftButton) {
        box_selection_rect_ = QRectF(box_selection_start_, event->position()).normalized();
        update();
        return;
    }
    if (picking_enabled_ && drag_button_ == Qt::LeftButton) return;
    if (drag_button_ == Qt::LeftButton) {
        yaw_degrees_ += delta.x() * 0.55;
        pitch_degrees_ = std::clamp(pitch_degrees_ - delta.y() * 0.4, -85.0, 85.0);
    } else {
        pan_ += QPointF(delta);
    }
    update();
}

void PointCloudWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != drag_button_) return;
    if (box_selection_enabled_ && event->button() == Qt::LeftButton) {
        box_selection_rect_ = QRectF(box_selection_start_, event->position()).normalized();
        const QVector<int> selected = indicesInScreenRect(box_selection_rect_);
        box_selection_rect_ = {};
        drag_button_ = Qt::NoButton;
        emit boxSelectionFinished(selected);
        setCursor(box_selection_enabled_ ? Qt::CrossCursor : Qt::OpenHandCursor);
        update();
        return;
    }
    if (picking_enabled_ && event->button() == Qt::LeftButton && !moved_since_press_) {
        const int index = pickNearest(event->position());
        if (index >= 0) emit pointPicked(index);
    }
    drag_button_ = Qt::NoButton;
    setCursor(picking_enabled_ || box_selection_enabled_ ? Qt::CrossCursor : Qt::OpenHandCursor);
    update();
}

void PointCloudWidget::mouseDoubleClickEvent(QMouseEvent*)
{
    resetView();
}

void PointCloudWidget::wheelEvent(QWheelEvent* event)
{
    const double factor = event->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12;
    view_scale_ = std::clamp(view_scale_ * factor, 0.25, 8.0);
    update();
    event->accept();
}

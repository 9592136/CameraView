#include "PointCloudWidget.h"

#include <QMouseEvent>
#include <QKeyEvent>
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

void PointCloudWidget::setCloud(const PointCloud& cloud, bool reset_view)
{
    cloud_ = cloud;
    if (!cloud_.Empty() && !cloud_.bounds.valid) cloud_.RecalculateBounds();
    highlighted_indices_.clear();
    selection_preview_indices_.clear();
    invalidateProjectionCache();
    if (reset_view) resetView(); else update();
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

void PointCloudWidget::setFittedPlane(const PointCloudPlane& plane)
{
    fitted_plane_ = plane;
    update();
}

void PointCloudWidget::setFittedPlaneVisible(bool visible)
{
    fitted_plane_visible_ = visible;
    update();
}

void PointCloudWidget::setGeometricModels(
    const std::vector<PointCloudGeometricModel>& models)
{
    geometric_models_ = models;
    if (active_model_id_ != 0 && std::none_of(geometric_models_.begin(), geometric_models_.end(),
            [this](const auto& model) { return model.id == active_model_id_; })) {
        active_model_id_ = 0;
    }
    active_residual_scale_ = 1.0;
    for (const auto& model : geometric_models_) {
        if (model.id != active_model_id_ || model.residuals.empty()) continue;
        std::vector<double> absolute;
        absolute.reserve(model.residuals.size());
        for (double value : model.residuals) absolute.push_back(std::abs(value));
        const std::size_t percentile = std::min(absolute.size() - 1,
            static_cast<std::size_t>(absolute.size() * 0.95));
        std::nth_element(absolute.begin(), absolute.begin() + percentile, absolute.end());
        active_residual_scale_ = std::max(absolute[percentile], 1e-12);
        break;
    }
    update();
}

void PointCloudWidget::setActiveGeometricModel(std::uint64_t id)
{
    active_model_id_ = id;
    setGeometricModels(geometric_models_);
}

void PointCloudWidget::setResidualColoringEnabled(bool enabled)
{
    residual_coloring_enabled_ = enabled;
    update();
}

void PointCloudWidget::setPickingEnabled(bool enabled)
{
    picking_enabled_ = enabled;
    if (enabled) {
        box_selection_enabled_ = false;
        section_selection_enabled_ = false;
    }
    setCursor(enabled || box_selection_enabled_ || section_selection_enabled_
        ? Qt::CrossCursor : Qt::OpenHandCursor);
}

void PointCloudWidget::setBoxSelectionEnabled(bool enabled)
{
    box_selection_enabled_ = enabled;
    if (enabled) {
        picking_enabled_ = false;
        section_selection_enabled_ = false;
    }
    box_selection_rect_ = {};
    setCursor(enabled ? Qt::CrossCursor : Qt::OpenHandCursor);
    update();
}

void PointCloudWidget::setSectionSelectionEnabled(bool enabled, double half_width_pixels)
{
    section_selection_enabled_ = enabled;
    section_half_width_pixels_ = std::clamp(half_width_pixels, 2.0, 80.0);
    if (enabled) {
        picking_enabled_ = false;
        box_selection_enabled_ = false;
    }
    section_start_ = {};
    section_end_ = {};
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
    invalidateProjectionCache();
    update();
}

qint64 PointCloudWidget::screenCellKey(int x, int y)
{
    return (static_cast<qint64>(x) << 32) ^ static_cast<quint32>(y);
}

void PointCloudWidget::invalidateProjectionCache()
{
    interaction_projection_valid_ = false;
    render_projection_valid_ = false;
    interaction_projected_points_.clear();
    screen_index_.clear();
}

void PointCloudWidget::ensureInteractionProjectionCache() const
{
    if (interaction_projection_valid_) return;
    constexpr int cell_size = 24;
    interaction_projected_points_.resize(static_cast<int>(cloud_.points.size()));
    screen_index_.clear();
    screen_index_.reserve(static_cast<int>(cloud_.points.size() / 8 + 1));
    for (int index = 0; index < static_cast<int>(cloud_.points.size()); ++index) {
        const ProjectedPoint projected = projectPoint(index);
        interaction_projected_points_[index] = projected;
        const int cell_x = static_cast<int>(std::floor(projected.position.x() / cell_size));
        const int cell_y = static_cast<int>(std::floor(projected.position.y() / cell_size));
        screen_index_[screenCellKey(cell_x, cell_y)].push_back(index);
    }
    interaction_projection_valid_ = true;
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

QColor PointCloudWidget::pointColor(const PointCloudPoint& point, int index) const
{
    if (residual_coloring_enabled_ && active_model_id_ != 0) {
        const auto model = std::find_if(geometric_models_.begin(), geometric_models_.end(),
            [this](const auto& value) { return value.id == active_model_id_; });
        if (model != geometric_models_.end()) {
            const double residual = PointCloudGeometricFitter::Residual(*model, point);
            if (std::isfinite(residual)) {
                const double normalized = std::clamp(residual / active_residual_scale_, -1.0, 1.0);
                const QColor neutral(225, 232, 240);
                const QColor endpoint = normalized < 0.0 ? QColor(55, 132, 255) : QColor(255, 84, 84);
                const double ratio = std::abs(normalized);
                return QColor::fromRgbF(
                    neutral.redF() * (1.0 - ratio) + endpoint.redF() * ratio,
                    neutral.greenF() * (1.0 - ratio) + endpoint.greenF() * ratio,
                    neutral.blueF() * (1.0 - ratio) + endpoint.blueF() * ratio);
            }
        }
    }
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
    rendered_point_count_ = 0;
    if (!hasCloud()) {
        painter.setPen(QColor(139, 154, 174));
        painter.drawText(rect(), Qt::AlignCenter,
            tr("打开 PLY、PCD、XYZ、TXT 或 CSV 点云"));
        return;
    }

    PointCloudPoint plane_center;
    std::array<PointCloudPoint, 4> plane_corners{};
    bool draw_plane = fitted_plane_visible_ && fitted_plane_.valid;
    if (draw_plane) {
        const PointCloudCentroid centroid = cloud_.Centroid();
        const PointCloudPoint cloud_center{centroid.x, centroid.y, centroid.z};
        const double offset = fitted_plane_.SignedDistance(cloud_center);
        plane_center = {
            cloud_center.x - offset * fitted_plane_.nx,
            cloud_center.y - offset * fitted_plane_.ny,
            cloud_center.z - offset * fitted_plane_.nz};
        std::array<double, 3> normal{
            fitted_plane_.nx, fitted_plane_.ny, fitted_plane_.nz};
        const std::array<double, 3> reference = std::abs(normal[2]) < 0.9
            ? std::array<double, 3>{0.0, 0.0, 1.0}
            : std::array<double, 3>{1.0, 0.0, 0.0};
        std::array<double, 3> first{
            normal[1] * reference[2] - normal[2] * reference[1],
            normal[2] * reference[0] - normal[0] * reference[2],
            normal[0] * reference[1] - normal[1] * reference[0]};
        const double first_length = std::sqrt(
            first[0] * first[0] + first[1] * first[1] + first[2] * first[2]);
        draw_plane = first_length > 1e-12;
        if (draw_plane) {
            for (double& value : first) value /= first_length;
            const std::array<double, 3> second{
                normal[1] * first[2] - normal[2] * first[1],
                normal[2] * first[0] - normal[0] * first[2],
                normal[0] * first[1] - normal[1] * first[0]};
            const double half_extent = std::max({cloud_.bounds.Width(),
                cloud_.bounds.Depth(), cloud_.bounds.Height(), 1e-9}) * 0.62;
            const std::array<std::array<double, 2>, 4> signs{{
                {-1.0, -1.0}, {1.0, -1.0}, {1.0, 1.0}, {-1.0, 1.0}}};
            QPolygonF polygon;
            for (int index = 0; index < 4; ++index) {
                const double first_scale = signs[index][0] * half_extent;
                const double second_scale = signs[index][1] * half_extent;
                plane_corners[index] = {
                    plane_center.x + first_scale * first[0] + second_scale * second[0],
                    plane_center.y + first_scale * first[1] + second_scale * second[1],
                    plane_center.z + first_scale * first[2] + second_scale * second[2]};
                polygon << projectPointValue(plane_corners[index]).position;
            }
            painter.setPen(QPen(QColor(75, 189, 255, 190), 1.5, Qt::DashLine));
            painter.setBrush(QColor(45, 145, 220, 42));
            painter.drawPolygon(polygon);
            painter.setPen(QPen(QColor(88, 194, 255, 85), 1.0));
            for (double ratio : {-0.5, 0.0, 0.5}) {
                const PointCloudPoint first_start{
                    plane_center.x - half_extent * first[0] + ratio * half_extent * second[0],
                    plane_center.y - half_extent * first[1] + ratio * half_extent * second[1],
                    plane_center.z - half_extent * first[2] + ratio * half_extent * second[2]};
                const PointCloudPoint first_end{
                    plane_center.x + half_extent * first[0] + ratio * half_extent * second[0],
                    plane_center.y + half_extent * first[1] + ratio * half_extent * second[1],
                    plane_center.z + half_extent * first[2] + ratio * half_extent * second[2]};
                const PointCloudPoint second_start{
                    plane_center.x + ratio * half_extent * first[0] - half_extent * second[0],
                    plane_center.y + ratio * half_extent * first[1] - half_extent * second[1],
                    plane_center.z + ratio * half_extent * first[2] - half_extent * second[2]};
                const PointCloudPoint second_end{
                    plane_center.x + ratio * half_extent * first[0] + half_extent * second[0],
                    plane_center.y + ratio * half_extent * first[1] + half_extent * second[1],
                    plane_center.z + ratio * half_extent * first[2] + half_extent * second[2]};
                painter.drawLine(projectPointValue(first_start).position,
                    projectPointValue(first_end).position);
                painter.drawLine(projectPointValue(second_start).position,
                    projectPointValue(second_end).position);
            }
        }
    }

    if (!render_projection_valid_) {
        projected_points_.clear();
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
        render_projection_valid_ = true;
    }
    painter.setPen(Qt::NoPen);
    for (const ProjectedPoint& projected : projected_points_) {
        painter.setBrush(pointColor(
            cloud_.points[static_cast<std::size_t>(projected.index)], projected.index));
        painter.drawEllipse(projected.position, point_size_, point_size_);
    }
    rendered_point_count_ = projected_points_.size();

    drawGeometricModels(painter);

    if (draw_plane) {
        const double normal_length = std::max({cloud_.bounds.Width(),
            cloud_.bounds.Depth(), cloud_.bounds.Height(), 1e-9}) * 0.32;
        const PointCloudPoint normal_end{
            plane_center.x + fitted_plane_.nx * normal_length,
            plane_center.y + fitted_plane_.ny * normal_length,
            plane_center.z + fitted_plane_.nz * normal_length};
        const QPointF start = projectPointValue(plane_center).position;
        const QPointF end = projectPointValue(normal_end).position;
        painter.setPen(QPen(QColor(92, 208, 255), 2.5));
        painter.drawLine(start, end);
        painter.setBrush(QColor(92, 208, 255));
        painter.drawEllipse(end, 4.0, 4.0);
        painter.drawText(end + QPointF(7.0, -7.0), tr("拟合平面法向"));
    }

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

    if (section_selection_enabled_ && section_start_ != section_end_) {
        const QLineF line(section_start_, section_end_);
        painter.setPen(QPen(QColor(255, 211, 74, 80), section_half_width_pixels_ * 2.0,
            Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(line);
        painter.setPen(QPen(QColor(255, 224, 100), 2.0, Qt::DashLine));
        painter.drawLine(line);
    }

    painter.setPen(QColor(140, 155, 175));
    painter.drawText(QRect(14, height() - 30, width() - 28, 20),
        Qt::AlignLeft | Qt::AlignVCenter,
        section_selection_enabled_
            ? tr("拖出截面线 · 线两侧带宽内的点将生成高度剖面")
            : box_selection_enabled_
            ? tr("拖出矩形选择点 · 右键拖动平移 · 滚轮缩放")
            : picking_enabled_
            ? tr("单击拾取点 · 滚轮缩放 · 右键拖动平移")
            : tr("左键拖动旋转 · 右键拖动平移 · 滚轮缩放 · 双击复位"));
}

void PointCloudWidget::drawGeometricModels(QPainter& painter) const
{
    constexpr int segments = 64;
    for (const PointCloudGeometricModel& model : geometric_models_) {
        if (!model.visible || model.type == PointCloudGeometricModelType::Plane) continue;
        const bool active = model.id == active_model_id_;
        const QColor color = active ? QColor(255, 196, 72) : QColor(88, 203, 255);
        painter.setPen(QPen(color, active ? 2.5 : 1.6));
        painter.setBrush(QColor(color.red(), color.green(), color.blue(), active ? 26 : 16));
        if (model.type == PointCloudGeometricModelType::Sphere && model.sphere.valid) {
            for (int plane = 0; plane < 3; ++plane) {
                QPainterPath path;
                for (int segment = 0; segment <= segments; ++segment) {
                    const double angle = 2.0 * kPi * segment / segments;
                    PointCloudPoint point = model.sphere.center;
                    const double first = model.sphere.radius * std::cos(angle);
                    const double second = model.sphere.radius * std::sin(angle);
                    if (plane == 0) { point.x += first; point.y += second; }
                    if (plane == 1) { point.x += first; point.z += second; }
                    if (plane == 2) { point.y += first; point.z += second; }
                    const QPointF projected = projectPointValue(point).position;
                    if (segment == 0) path.moveTo(projected); else path.lineTo(projected);
                }
                painter.drawPath(path);
            }
            painter.drawEllipse(projectPointValue(model.sphere.center).position, 4.0, 4.0);
        } else if (model.type == PointCloudGeometricModelType::Cylinder && model.cylinder.valid) {
            const auto axis = model.cylinder.axis_direction;
            std::array<double, 3> reference = std::abs(axis[2]) < 0.8
                ? std::array<double, 3>{0.0, 0.0, 1.0}
                : std::array<double, 3>{1.0, 0.0, 0.0};
            std::array<double, 3> first{axis[1] * reference[2] - axis[2] * reference[1],
                axis[2] * reference[0] - axis[0] * reference[2],
                axis[0] * reference[1] - axis[1] * reference[0]};
            const double first_length = std::sqrt(first[0] * first[0] + first[1] * first[1] + first[2] * first[2]);
            for (double& component : first) component /= first_length;
            const std::array<double, 3> second{axis[1] * first[2] - axis[2] * first[1],
                axis[2] * first[0] - axis[0] * first[2],
                axis[0] * first[1] - axis[1] * first[0]};
            std::array<QPointF, 4> end_points{};
            for (int end = 0; end < 2; ++end) {
                const double axial = end == 0 ? model.cylinder.axial_minimum : model.cylinder.axial_maximum;
                QPainterPath path;
                for (int segment = 0; segment <= segments; ++segment) {
                    const double angle = 2.0 * kPi * segment / segments;
                    PointCloudPoint point{
                        model.cylinder.axis_point.x + axial * axis[0] + model.cylinder.radius *
                            (std::cos(angle) * first[0] + std::sin(angle) * second[0]),
                        model.cylinder.axis_point.y + axial * axis[1] + model.cylinder.radius *
                            (std::cos(angle) * first[1] + std::sin(angle) * second[1]),
                        model.cylinder.axis_point.z + axial * axis[2] + model.cylinder.radius *
                            (std::cos(angle) * first[2] + std::sin(angle) * second[2])};
                    const QPointF projected = projectPointValue(point).position;
                    if (segment == 0) path.moveTo(projected); else path.lineTo(projected);
                    if (segment == 0) end_points[end * 2] = projected;
                    if (segment == segments / 2) end_points[end * 2 + 1] = projected;
                }
                painter.drawPath(path);
            }
            painter.drawLine(end_points[0], end_points[2]);
            painter.drawLine(end_points[1], end_points[3]);
            const PointCloudPoint start{model.cylinder.axis_point.x + model.cylinder.axial_minimum * axis[0],
                model.cylinder.axis_point.y + model.cylinder.axial_minimum * axis[1],
                model.cylinder.axis_point.z + model.cylinder.axial_minimum * axis[2]};
            const PointCloudPoint end{model.cylinder.axis_point.x + model.cylinder.axial_maximum * axis[0],
                model.cylinder.axis_point.y + model.cylinder.axial_maximum * axis[1],
                model.cylinder.axis_point.z + model.cylinder.axial_maximum * axis[2]};
            painter.setPen(QPen(color, active ? 2.2 : 1.2, Qt::DashLine));
            painter.drawLine(projectPointValue(start).position, projectPointValue(end).position);
        }
    }
}

QVector<int> PointCloudWidget::indicesInScreenRect(const QRectF& rectangle) const
{
    QVector<int> indices;
    if (!hasCloud() || rectangle.isEmpty()) return indices;
    const QRectF normalized = rectangle.normalized();
    ensureInteractionProjectionCache();
    indices.reserve(static_cast<int>(cloud_.points.size() / 4));
    constexpr int cell_size = 24;
    const int minimum_x = static_cast<int>(std::floor(normalized.left() / cell_size));
    const int maximum_x = static_cast<int>(std::floor(normalized.right() / cell_size));
    const int minimum_y = static_cast<int>(std::floor(normalized.top() / cell_size));
    const int maximum_y = static_cast<int>(std::floor(normalized.bottom() / cell_size));
    for (int cell_y = minimum_y; cell_y <= maximum_y; ++cell_y) {
        for (int cell_x = minimum_x; cell_x <= maximum_x; ++cell_x) {
            const auto entry = screen_index_.constFind(screenCellKey(cell_x, cell_y));
            if (entry == screen_index_.constEnd()) continue;
            for (int index : entry.value()) {
                if (normalized.contains(interaction_projected_points_[index].position)) indices.push_back(index);
            }
        }
    }
    return indices;
}

QVector<int> PointCloudWidget::indicesNearScreenLine(
    const QPointF& first,
    const QPointF& second,
    double half_width_pixels) const
{
    QVector<int> indices;
    const QPointF direction = second - first;
    const double length_squared = direction.x() * direction.x() + direction.y() * direction.y();
    if (!hasCloud() || length_squared < 16.0) return indices;
    const double limit_squared = half_width_pixels * half_width_pixels;
    ensureInteractionProjectionCache();
    const QRectF bounds(first, second);
    const QRectF expanded = bounds.normalized().adjusted(
        -half_width_pixels, -half_width_pixels, half_width_pixels, half_width_pixels);
    const QVector<int> candidates = indicesInScreenRect(expanded);
    for (int index : candidates) {
        const QPointF position = interaction_projected_points_[index].position;
        const QPointF offset = position - first;
        const double parameter = std::clamp(
            (offset.x() * direction.x() + offset.y() * direction.y()) / length_squared,
            0.0, 1.0);
        const QPointF closest = first + direction * parameter;
        const QPointF delta = position - closest;
        if (delta.x() * delta.x() + delta.y() * delta.y() <= limit_squared) {
            indices.push_back(index);
        }
    }
    return indices;
}

int PointCloudWidget::pickNearest(const QPointF& position, double radius) const
{
    int selected = -1;
    double best_distance = radius * radius;
    ensureInteractionProjectionCache();
    constexpr int cell_size = 24;
    const int minimum_x = static_cast<int>(std::floor((position.x() - radius) / cell_size));
    const int maximum_x = static_cast<int>(std::floor((position.x() + radius) / cell_size));
    const int minimum_y = static_cast<int>(std::floor((position.y() - radius) / cell_size));
    const int maximum_y = static_cast<int>(std::floor((position.y() + radius) / cell_size));
    for (int cell_y = minimum_y; cell_y <= maximum_y; ++cell_y) {
        for (int cell_x = minimum_x; cell_x <= maximum_x; ++cell_x) {
            const auto entry = screen_index_.constFind(screenCellKey(cell_x, cell_y));
            if (entry == screen_index_.constEnd()) continue;
            for (int index : entry.value()) {
                const ProjectedPoint& point = interaction_projected_points_[index];
                const QPointF delta = point.position - position;
                const double distance = delta.x() * delta.x() + delta.y() * delta.y();
                if (distance <= best_distance) {
                    best_distance = distance;
                    selected = index;
                }
            }
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
    if (section_selection_enabled_ && event->button() == Qt::LeftButton) {
        section_start_ = section_end_ = event->position();
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
    if (section_selection_enabled_ && drag_button_ == Qt::LeftButton) {
        section_end_ = event->position();
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
    invalidateProjectionCache();
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
    if (section_selection_enabled_ && event->button() == Qt::LeftButton) {
        section_end_ = event->position();
        const QPointF first = section_start_;
        const QPointF second = section_end_;
        const QVector<int> selected = indicesNearScreenLine(
            first, second, section_half_width_pixels_);
        section_selection_enabled_ = false;
        drag_button_ = Qt::NoButton;
        emit sectionSelectionFinished(selected, first, second);
        setCursor(Qt::OpenHandCursor);
        update();
        return;
    }
    if (picking_enabled_ && event->button() == Qt::LeftButton && !moved_since_press_) {
        const int index = pickNearest(event->position());
        if (index >= 0) emit pointPicked(index);
    }
    drag_button_ = Qt::NoButton;
    setCursor(picking_enabled_ || box_selection_enabled_ || section_selection_enabled_
        ? Qt::CrossCursor : Qt::OpenHandCursor);
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
    invalidateProjectionCache();
    update();
    event->accept();
}

void PointCloudWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        box_selection_enabled_ = false;
        section_selection_enabled_ = false;
        picking_enabled_ = false;
        box_selection_rect_ = {};
        section_start_ = section_end_ = {};
        drag_button_ = Qt::NoButton;
        setCursor(Qt::OpenHandCursor);
        update();
        emit interactionCancelled();
        event->accept();
        return;
    }
    QOpenGLWidget::keyPressEvent(event);
}

void PointCloudWidget::resizeEvent(QResizeEvent* event)
{
    invalidateProjectionCache();
    QOpenGLWidget::resizeEvent(event);
}

#include "PointCloudWidget.h"

#include <QMouseEvent>
#include <QKeyEvent>
#include <QImage>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <limits>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kRenderPointBudget = 120000;
constexpr int kInteractivePointBudget = 32000;

struct TextureVertex {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float u = 0.0F;
    float v = 0.0F;
};

double wrapDegrees(double degrees)
{
    return std::remainder(degrees, 360.0);
}

} // namespace

PointCloudWidget::PointCloudWidget(QWidget* parent) : QOpenGLWidget(parent)
{
    setObjectName(QStringLiteral("PointCloudView"));
    setMinimumSize(480, 360);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
    interaction_idle_timer_.setSingleShot(true);
    connect(&interaction_idle_timer_, &QTimer::timeout, this, [this] {
        if (!interactive_rendering_) return;
        interactive_rendering_ = false;
        invalidateRenderProjection();
        update();
    });
}

PointCloudWidget::~PointCloudWidget()
{
    if (context()) {
        makeCurrent();
        releaseTextureRenderer();
        doneCurrent();
    }
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
    texture_renderer_ready_ = initializeTextureRenderer();
    emit renderBackendChanged(render_backend_, hardware_accelerated_);
}

bool PointCloudWidget::initializeTextureRenderer()
{
    texture_program_ = std::make_unique<QOpenGLShaderProgram>();
    static constexpr char vertex_shader[] = R"(
        attribute highp vec3 aPosition;
        attribute highp vec2 aTexCoord;
        varying highp vec2 vTexCoord;
        void main()
        {
            gl_Position = vec4(aPosition, 1.0);
            vTexCoord = aTexCoord;
        }
    )";
    static constexpr char fragment_shader[] = R"(
        #ifdef GL_ES
        precision mediump float;
        #endif
        uniform sampler2D uTexture;
        uniform lowp float uTextureEnhancement;
        varying highp vec2 vTexCoord;
        void main()
        {
            lowp vec4 sampled = texture2D(uTexture, vTexCoord);
            if (uTextureEnhancement > 0.5) {
                sampled.rgb = pow(sampled.rgb, vec3(0.78));
                sampled.rgb = clamp((sampled.rgb - vec3(0.5)) * 1.08 + vec3(0.5), 0.0, 1.0);
            }
            gl_FragColor = sampled;
        }
    )";
    if (!texture_program_->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_shader) ||
        !texture_program_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment_shader) ||
        !texture_program_->link()) {
        texture_program_.reset();
        return false;
    }
    texture_vertex_buffer_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    texture_index_buffer_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
    if (!texture_vertex_buffer_->create() || !texture_index_buffer_->create()) {
        releaseTextureRenderer();
        return false;
    }
    texture_data_dirty_ = true;
    texture_mesh_dirty_ = true;
    return true;
}

void PointCloudWidget::releaseTextureRenderer()
{
    texture_image_.reset();
    if (texture_vertex_buffer_ && texture_vertex_buffer_->isCreated()) {
        texture_vertex_buffer_->destroy();
    }
    if (texture_index_buffer_ && texture_index_buffer_->isCreated()) {
        texture_index_buffer_->destroy();
    }
    texture_vertex_buffer_.reset();
    texture_index_buffer_.reset();
    texture_program_.reset();
    texture_renderer_ready_ = false;
    texture_vertex_count_ = 0;
    texture_index_count_ = 0;
}

void PointCloudWidget::setCloud(const PointCloud& cloud, bool reset_view)
{
    interaction_idle_timer_.stop();
    interactive_rendering_ = false;
    cloud_ = cloud;
    if (!cloud_.Empty() && !cloud_.bounds.valid) cloud_.RecalculateBounds();
    highlighted_indices_.clear();
    selection_preview_indices_.clear();
    texture_data_dirty_ = true;
    texture_mesh_dirty_ = true;
    discardProjectionCaches();
    if (reset_view) resetView(); else update();
}

void PointCloudWidget::setColorMode(PointCloudColorMode mode)
{
    color_mode_ = mode;
    invalidateRenderProjection();
    update();
}

void PointCloudWidget::setPointSize(double size)
{
    point_size_ = std::clamp(size, 1.0, 12.0);
    invalidateRenderProjection();
    update();
}

void PointCloudWidget::setTextureEnhancementEnabled(bool enabled)
{
    texture_enhancement_enabled_ = enabled;
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
    invalidateRenderProjection();
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
    invalidateRenderProjection();
    update();
}

void PointCloudWidget::setPickingEnabled(bool enabled)
{
    picking_enabled_ = enabled;
    if (enabled) {
        box_selection_enabled_ = false;
        free_selection_enabled_ = false;
        section_selection_enabled_ = false;
    }
    hovered_point_index_ = -1;
    updateInteractionCursor();
    update();
}

void PointCloudWidget::setBoxSelectionEnabled(bool enabled)
{
    box_selection_enabled_ = enabled;
    if (enabled) {
        picking_enabled_ = false;
        free_selection_enabled_ = false;
        section_selection_enabled_ = false;
    }
    box_selection_rect_ = {};
    hovered_point_index_ = -1;
    updateInteractionCursor();
    update();
}

void PointCloudWidget::setFreeSelectionEnabled(bool enabled)
{
    free_selection_enabled_ = enabled;
    if (enabled) {
        picking_enabled_ = false;
        box_selection_enabled_ = false;
        section_selection_enabled_ = false;
    }
    free_selection_path_.clear();
    hovered_point_index_ = -1;
    updateInteractionCursor();
    update();
}

void PointCloudWidget::setSectionSelectionEnabled(bool enabled, double half_width_pixels)
{
    section_selection_enabled_ = enabled;
    section_half_width_pixels_ = std::clamp(half_width_pixels, 2.0, 80.0);
    if (enabled) {
        picking_enabled_ = false;
        box_selection_enabled_ = false;
        free_selection_enabled_ = false;
    }
    section_start_ = {};
    section_end_ = {};
    hovered_point_index_ = -1;
    updateInteractionCursor();
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
    interaction_idle_timer_.stop();
    interactive_rendering_ = false;
    yaw_degrees_ = -38.0;
    pitch_degrees_ = 26.0;
    view_scale_ = 1.0;
    pan_ = {};
    invalidateProjectionCache();
    update();
}

void PointCloudWidget::setViewPreset(PointCloudViewPreset preset)
{
    interaction_idle_timer_.stop();
    interactive_rendering_ = false;
    switch (preset) {
    case PointCloudViewPreset::Top:
        yaw_degrees_ = 0.0;
        pitch_degrees_ = 90.0;
        break;
    case PointCloudViewPreset::Front:
        yaw_degrees_ = 0.0;
        pitch_degrees_ = 0.0;
        break;
    case PointCloudViewPreset::Right:
        yaw_degrees_ = 90.0;
        pitch_degrees_ = 0.0;
        break;
    case PointCloudViewPreset::Isometric:
        yaw_degrees_ = -38.0;
        pitch_degrees_ = 26.0;
        break;
    }
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
    texture_mesh_dirty_ = true;
    hovered_point_index_ = -1;
}

void PointCloudWidget::discardProjectionCaches()
{
    invalidateProjectionCache();
    projected_points_.clear();
    interaction_projected_points_.clear();
    screen_index_.clear();
}

void PointCloudWidget::invalidateRenderProjection()
{
    render_projection_valid_ = false;
    texture_mesh_dirty_ = true;
}

int PointCloudWidget::currentRenderBudget() const
{
    if (!interactive_rendering_) return kRenderPointBudget;
    const int base_budget = hardware_accelerated_ ? kInteractivePointBudget : 20000;
    const double size_cost = std::max(1.0, point_size_ / 2.5);
    const int adjusted = static_cast<int>(base_budget / size_cost);
    return std::clamp(adjusted, 10000, base_budget);
}

void PointCloudWidget::beginInteractiveRendering()
{
    interaction_idle_timer_.stop();
    if (interactive_rendering_) return;
    interactive_rendering_ = true;
    invalidateRenderProjection();
}

void PointCloudWidget::finishInteractiveRendering(int delay_ms)
{
    if (!interactive_rendering_) return;
    interaction_idle_timer_.start(std::max(0, delay_ms));
}

void PointCloudWidget::updateInteractionCursor()
{
    if (drag_button_ != Qt::NoButton) {
        if ((box_selection_enabled_ || free_selection_enabled_ ||
                section_selection_enabled_ || picking_enabled_) &&
            drag_button_ == Qt::LeftButton) {
            setCursor(Qt::CrossCursor);
        } else {
            setCursor(drag_button_ == Qt::LeftButton ? Qt::ClosedHandCursor : Qt::SizeAllCursor);
        }
        return;
    }
    setCursor(picking_enabled_ || box_selection_enabled_ || free_selection_enabled_ ||
        section_selection_enabled_
        ? Qt::CrossCursor : Qt::OpenHandCursor);
}

void PointCloudWidget::ensureInteractionProjectionCache() const
{
    if (interaction_projection_valid_) return;
    constexpr int cell_size = 24;
    interaction_projected_points_.resize(static_cast<int>(cloud_.points.size()));
    screen_index_.clear();
    const int visible_cell_count = std::max(1,
        (width() / cell_size + 3) * (height() / cell_size + 3));
    screen_index_.reserve(std::min(visible_cell_count,
        static_cast<int>(cloud_.points.size() / 8 + 1)));
    for (int index = 0; index < static_cast<int>(cloud_.points.size()); ++index) {
        const ProjectedPoint projected = projectPoint(index);
        interaction_projected_points_[index] = projected;
        if (projected.position.x() < -cell_size || projected.position.x() > width() + cell_size ||
            projected.position.y() < -cell_size || projected.position.y() > height() + cell_size) {
            continue;
        }
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
    if ((color_mode_ == PointCloudColorMode::Texture ||
            color_mode_ == PointCloudColorMode::Original) && point.has_color) {
        return QColor(point.r, point.g, point.b);
    }
    if (color_mode_ == PointCloudColorMode::Solid) return QColor(98, 178, 255);
    const double range = cloud_.bounds.Height();
    const double normalized = range > 1e-15
        ? std::clamp((point.z - cloud_.bounds.min_z) / range, 0.0, 1.0)
        : 0.5;
    return QColor::fromHsvF((1.0 - normalized) * 0.68, 0.86, 0.98);
}

bool PointCloudWidget::rebuildTextureSurface()
{
    if (!texture_renderer_ready_ || !cloud_.HasTextureSurface() ||
        !texture_program_ || !texture_vertex_buffer_ || !texture_index_buffer_) {
        return false;
    }
    const std::size_t grid_width = cloud_.organized_width;
    const std::size_t grid_height = cloud_.organized_height;
    if (texture_data_dirty_) {
        QImage image(static_cast<int>(grid_width), static_cast<int>(grid_height), QImage::Format_RGB888);
        if (image.isNull()) return false;
        for (std::size_t row = 0; row < grid_height; ++row) {
            unsigned char* destination = image.scanLine(static_cast<int>(row));
            for (std::size_t column = 0; column < grid_width; ++column) {
                const PointCloudPoint& point = cloud_.points[row * grid_width + column];
                destination[column * 3U] = point.r;
                destination[column * 3U + 1U] = point.g;
                destination[column * 3U + 2U] = point.b;
            }
        }
        texture_image_.reset();
        texture_image_ = std::make_unique<QOpenGLTexture>(
            image, QOpenGLTexture::GenerateMipMaps);
        if (!texture_image_->isCreated()) {
            texture_image_.reset();
            return false;
        }
        texture_image_->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
        texture_image_->setMagnificationFilter(QOpenGLTexture::Linear);
        texture_image_->setMaximumAnisotropy(8.0F);
        texture_image_->setWrapMode(QOpenGLTexture::ClampToEdge);
        texture_data_dirty_ = false;
    }
    if (!texture_mesh_dirty_) return texture_index_count_ > 0;

    const std::size_t vertex_budget = interactive_rendering_ ? 90000U : 720000U;
    const double ratio = cloud_.points.size() / static_cast<double>(vertex_budget);
    const std::size_t grid_stride = std::max<std::size_t>(1,
        static_cast<std::size_t>(std::ceil(std::sqrt(ratio))));
    std::vector<std::size_t> columns;
    std::vector<std::size_t> rows;
    for (std::size_t column = 0; column < grid_width; column += grid_stride) {
        columns.push_back(column);
    }
    if (columns.empty() || columns.back() != grid_width - 1) columns.push_back(grid_width - 1);
    for (std::size_t row = 0; row < grid_height; row += grid_stride) rows.push_back(row);
    if (rows.empty() || rows.back() != grid_height - 1) rows.push_back(grid_height - 1);
    if (columns.size() < 2 || rows.size() < 2) return false;

    std::vector<TextureVertex> vertices;
    vertices.reserve(columns.size() * rows.size());
    double minimum_depth = std::numeric_limits<double>::max();
    double maximum_depth = std::numeric_limits<double>::lowest();
    for (std::size_t row : rows) {
        for (std::size_t column : columns) {
            const ProjectedPoint projected = projectPoint(
                static_cast<int>(row * grid_width + column));
            TextureVertex vertex;
            vertex.x = static_cast<float>(projected.position.x() * 2.0 / width() - 1.0);
            vertex.y = static_cast<float>(1.0 - projected.position.y() * 2.0 / height());
            vertex.z = static_cast<float>(projected.depth);
            vertex.u = static_cast<float>(column / static_cast<double>(grid_width - 1));
            vertex.v = static_cast<float>(row / static_cast<double>(grid_height - 1));
            vertices.push_back(vertex);
            minimum_depth = std::min(minimum_depth, projected.depth);
            maximum_depth = std::max(maximum_depth, projected.depth);
        }
    }
    const double depth_range = std::max(maximum_depth - minimum_depth, 1e-9);
    for (TextureVertex& vertex : vertices) {
        vertex.z = static_cast<float>(-0.95 +
            (static_cast<double>(vertex.z) - minimum_depth) / depth_range * 1.9);
    }
    std::vector<std::uint32_t> indices;
    const std::size_t column_count = columns.size();
    indices.reserve((rows.size() - 1) * (column_count - 1) * 6U);
    for (std::size_t row = 0; row + 1 < rows.size(); ++row) {
        for (std::size_t column = 0; column + 1 < column_count; ++column) {
            const std::uint32_t top_left = static_cast<std::uint32_t>(
                row * column_count + column);
            const std::uint32_t top_right = top_left + 1U;
            const std::uint32_t bottom_left = static_cast<std::uint32_t>(
                (row + 1U) * column_count + column);
            const std::uint32_t bottom_right = bottom_left + 1U;
            indices.insert(indices.end(), {
                top_left, bottom_left, top_right,
                top_right, bottom_left, bottom_right});
        }
    }
    if (vertices.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        indices.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    texture_vertex_buffer_->bind();
    texture_vertex_buffer_->allocate(vertices.data(),
        static_cast<int>(vertices.size() * sizeof(TextureVertex)));
    texture_vertex_buffer_->release();
    texture_index_buffer_->bind();
    texture_index_buffer_->allocate(indices.data(),
        static_cast<int>(indices.size() * sizeof(std::uint32_t)));
    texture_index_buffer_->release();
    texture_vertex_count_ = static_cast<int>(vertices.size());
    texture_index_count_ = static_cast<int>(indices.size());
    texture_mesh_dirty_ = false;
    ++texture_mesh_revision_;
    return texture_index_count_ > 0;
}

bool PointCloudWidget::drawTextureSurface(QPainter& painter)
{
    if (color_mode_ != PointCloudColorMode::Texture || residual_coloring_enabled_ ||
        !cloud_.HasTextureSurface() || !texture_renderer_ready_) {
        return false;
    }
    painter.beginNativePainting();
    if (!rebuildTextureSurface()) {
        painter.endNativePainting();
        return false;
    }
    QOpenGLFunctions* functions = context() ? context()->functions() : nullptr;
    if (!functions || !texture_program_->bind() || !texture_vertex_buffer_->bind() ||
        !texture_index_buffer_->bind()) {
        painter.endNativePainting();
        return false;
    }
    functions->glEnable(GL_DEPTH_TEST);
    functions->glDepthMask(GL_TRUE);
    functions->glDisable(GL_CULL_FACE);
    functions->glClear(GL_DEPTH_BUFFER_BIT);
    texture_image_->bind(0);
    texture_program_->setUniformValue("uTexture", 0);
    texture_program_->setUniformValue("uTextureEnhancement",
        texture_enhancement_enabled_ ? 1.0F : 0.0F);
    const int position = texture_program_->attributeLocation("aPosition");
    const int texture_coordinate = texture_program_->attributeLocation("aTexCoord");
    texture_program_->enableAttributeArray(position);
    texture_program_->setAttributeBuffer(position, GL_FLOAT,
        static_cast<int>(offsetof(TextureVertex, x)), 3, sizeof(TextureVertex));
    texture_program_->enableAttributeArray(texture_coordinate);
    texture_program_->setAttributeBuffer(texture_coordinate, GL_FLOAT,
        static_cast<int>(offsetof(TextureVertex, u)), 2, sizeof(TextureVertex));
    functions->glDrawElements(
        GL_TRIANGLES, texture_index_count_, GL_UNSIGNED_INT, nullptr);
    texture_program_->disableAttributeArray(position);
    texture_program_->disableAttributeArray(texture_coordinate);
    texture_image_->release();
    texture_index_buffer_->release();
    texture_vertex_buffer_->release();
    texture_program_->release();
    functions->glDisable(GL_DEPTH_TEST);
    painter.endNativePainting();
    return true;
}

void PointCloudWidget::rebuildRenderCache()
{
    projected_points_.clear();
    surface_patches_.clear();
    const int render_budget = currentRenderBudget();
    const int stride = std::max(1, static_cast<int>(std::ceil(
        cloud_.points.size() / static_cast<double>(render_budget))));
    projected_points_.reserve(
        static_cast<int>((cloud_.points.size() + stride - 1) / stride));
    for (int index = 0; index < static_cast<int>(cloud_.points.size()); index += stride) {
        projected_points_.push_back(projectPoint(index));
    }

    if (color_mode_ == PointCloudColorMode::Texture && cloud_.HasTextureSurface() &&
        (!texture_renderer_ready_ || residual_coloring_enabled_)) {
        const std::size_t width = cloud_.organized_width;
        const std::size_t height = cloud_.organized_height;
        const int patch_budget = interactive_rendering_ ? 14000 : 56000;
        const double cell_count = static_cast<double>((width - 1) * (height - 1));
        const std::size_t grid_stride = std::max<std::size_t>(1,
            static_cast<std::size_t>(std::ceil(std::sqrt(cell_count / patch_budget))));
        surface_patches_.reserve(static_cast<int>(std::min<double>(
            patch_budget * 1.15, std::numeric_limits<int>::max())));
        for (std::size_t row = 0; row + 1 < height; row += grid_stride) {
            const std::size_t next_row = std::min(height - 1, row + grid_stride);
            for (std::size_t column = 0; column + 1 < width; column += grid_stride) {
                const std::size_t next_column = std::min(width - 1, column + grid_stride);
                const std::array<std::size_t, 4> indices{{
                    row * width + column,
                    row * width + next_column,
                    next_row * width + next_column,
                    next_row * width + column}};
                SurfacePatch patch;
                int red = 0;
                int green = 0;
                int blue = 0;
                bool valid = true;
                for (std::size_t index : indices) {
                    if (index >= cloud_.points.size()) {
                        valid = false;
                        break;
                    }
                    const ProjectedPoint projected = projectPoint(static_cast<int>(index));
                    patch.polygon << projected.position;
                    patch.depth += projected.depth;
                    const QColor color = pointColor(cloud_.points[index], static_cast<int>(index));
                    red += color.red();
                    green += color.green();
                    blue += color.blue();
                }
                if (!valid) continue;
                patch.depth *= 0.25;
                patch.color = QColor(red / 4, green / 4, blue / 4);
                surface_patches_.push_back(std::move(patch));
            }
        }
        if (!interactive_rendering_) {
            std::sort(surface_patches_.begin(), surface_patches_.end(),
                [](const SurfacePatch& left, const SurfacePatch& right) {
                    return left.depth > right.depth;
                });
        }
    }
    if (!interactive_rendering_) {
        std::sort(projected_points_.begin(), projected_points_.end(),
            [](const ProjectedPoint& left, const ProjectedPoint& right) {
                return left.depth > right.depth;
            });
    }
    render_projection_valid_ = true;
}

void PointCloudWidget::paintGL()
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, !interactive_rendering_);
    QLinearGradient background(rect().topLeft(), rect().bottomRight());
    background.setColorAt(0.0, QColor(8, 13, 19));
    background.setColorAt(1.0, QColor(19, 29, 40));
    painter.fillRect(rect(), background);
    rendered_point_count_ = 0;
    if (!hasCloud()) {
        painter.setPen(QColor(139, 154, 174));
        painter.drawText(rect(), Qt::AlignCenter,
            tr("打开 H3D、PLY、PCD、XYZ、TXT 或 CSV 点云"));
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

    if (!render_projection_valid_) rebuildRenderCache();
    const bool gpu_texture_surface = drawTextureSurface(painter);
    painter.setPen(Qt::NoPen);
    if (!gpu_texture_surface && !surface_patches_.isEmpty()) {
        for (const SurfacePatch& patch : surface_patches_) {
            painter.setBrush(patch.color);
            painter.drawPolygon(patch.polygon);
        }
    } else if (!gpu_texture_surface) {
        for (const ProjectedPoint& projected : projected_points_) {
            painter.setBrush(pointColor(
                cloud_.points[static_cast<std::size_t>(projected.index)], projected.index));
            painter.drawEllipse(projected.position, point_size_, point_size_);
        }
    }
    rendered_point_count_ = gpu_texture_surface
        ? texture_vertex_count_ : projected_points_.size();
    if (reported_rendered_point_count_ != rendered_point_count_ ||
        reported_interactive_rendering_ != interactive_rendering_) {
        reported_rendered_point_count_ = rendered_point_count_;
        reported_interactive_rendering_ = interactive_rendering_;
        emit renderStatisticsChanged(rendered_point_count_, interactive_rendering_);
    }

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

    if (color_mode_ == PointCloudColorMode::Texture && cloud_.HasTextureSurface()) {
        const QString badge = interactive_rendering_
            ? tr("纹理表面 · 交互预览") : tr("纹理表面");
        const QRectF badge_rect(width() - 152.0, 14.0, 138.0, 28.0);
        painter.setPen(QPen(QColor(68, 145, 225, 150), 1.0));
        painter.setBrush(QColor(18, 48, 77, 218));
        painter.drawRoundedRect(badge_rect, 7.0, 7.0);
        painter.setPen(QColor(188, 222, 255));
        painter.drawText(badge_rect, Qt::AlignCenter, badge);
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

    if (hovered_point_index_ >= 0 &&
        hovered_point_index_ < static_cast<int>(cloud_.points.size())) {
        const QPointF position = projectPoint(hovered_point_index_).position;
        painter.setPen(QPen(QColor(255, 255, 255, 230), 1.5));
        painter.setBrush(QColor(62, 159, 255, 130));
        painter.drawEllipse(position, point_size_ + 4.0, point_size_ + 4.0);
    }


    if (!selection_preview_indices_.isEmpty()) {
        const int stride = std::max(1, static_cast<int>(std::ceil(
            selection_preview_indices_.size() /
            static_cast<double>(currentRenderBudget()))));
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

    if (free_selection_enabled_ && free_selection_path_.size() > 1) {
        const QPolygonF closed_path = free_selection_path_ + QPolygonF{free_selection_path_.front()};
        if (free_selection_path_.size() > 2) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 211, 74, 190));
            for (const ProjectedPoint& projected : projected_points_) {
                if (free_selection_path_.containsPoint(
                        projected.position, Qt::OddEvenFill)) {
                    painter.drawEllipse(projected.position,
                        point_size_ + 1.5, point_size_ + 1.5);
                }
            }
        }
        painter.setPen(QPen(QColor(255, 211, 74), 2.0, Qt::SolidLine,
            Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(QColor(255, 211, 74, 24));
        painter.drawPolygon(closed_path, Qt::OddEvenFill);
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
            : free_selection_enabled_
            ? tr("左键绘制自由选区 · Shift 添加 · Ctrl 移除 · 右键平移")
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
    const double viewport_area = std::max(1.0, static_cast<double>(width()) * height());
    const double selection_ratio = std::clamp(
        normalized.width() * normalized.height() / viewport_area, 0.0, 1.0);
    indices.reserve(std::min(static_cast<int>(cloud_.points.size()),
        std::max(32, static_cast<int>(cloud_.points.size() * selection_ratio))));
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
    std::sort(indices.begin(), indices.end());
    return indices;
}

QVector<int> PointCloudWidget::indicesInScreenPolygon(const QPolygonF& polygon) const
{
    QVector<int> indices;
    if (!hasCloud() || polygon.size() < 3) return indices;
    const QRectF bounds = polygon.boundingRect();
    const QVector<int> candidates = indicesInScreenRect(bounds);
    indices.reserve(candidates.size());
    for (int index : candidates) {
        if (index >= 0 && index < interaction_projected_points_.size() &&
            polygon.containsPoint(interaction_projected_points_[index].position,
                Qt::OddEvenFill)) {
            indices.push_back(index);
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
    double best_depth = std::numeric_limits<double>::max();
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
                if (distance < best_distance - 0.01 ||
                    (std::abs(distance - best_distance) <= 0.01 && point.depth < best_depth)) {
                    best_distance = distance;
                    best_depth = point.depth;
                    selected = index;
                }
            }
        }
    }
    return selected;
}

int PointCloudWidget::pickNearestRendered(const QPointF& position, double radius) const
{
    if (!render_projection_valid_) return -1;
    int selected = -1;
    double best_distance = radius * radius;
    double best_depth = std::numeric_limits<double>::max();
    for (const ProjectedPoint& point : projected_points_) {
        const QPointF delta = point.position - position;
        const double distance = delta.x() * delta.x() + delta.y() * delta.y();
        if (distance < best_distance - 0.01 ||
            (std::abs(distance - best_distance) <= 0.01 && point.depth < best_depth)) {
            best_distance = distance;
            best_depth = point.depth;
            selected = point.index;
        }
    }
    return selected;
}

void PointCloudWidget::updateHoveredPoint(const QPointF& position)
{
    const int hovered = picking_enabled_ ? pickNearestRendered(position, 10.0) : -1;
    if (hovered == hovered_point_index_) return;
    hovered_point_index_ = hovered;
    update();
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
    if (event->button() != Qt::LeftButton ||
        (!box_selection_enabled_ && !free_selection_enabled_ &&
            !section_selection_enabled_ && !picking_enabled_)) {
        beginInteractiveRendering();
    } else if (box_selection_enabled_ || free_selection_enabled_ ||
        section_selection_enabled_) {
        beginInteractiveRendering();
    }
    hovered_point_index_ = -1;
    if (box_selection_enabled_ && event->button() == Qt::LeftButton) {
        selection_modifiers_ = event->modifiers();
        box_selection_start_ = event->position();
        box_selection_rect_ = QRectF(box_selection_start_, box_selection_start_);
        updateInteractionCursor();
        update();
        return;
    }
    if (free_selection_enabled_ && event->button() == Qt::LeftButton) {
        selection_modifiers_ = event->modifiers();
        free_selection_path_.clear();
        free_selection_path_ << event->position();
        updateInteractionCursor();
        update();
        return;
    }
    if (section_selection_enabled_ && event->button() == Qt::LeftButton) {
        section_start_ = section_end_ = event->position();
        updateInteractionCursor();
        update();
        return;
    }
    if (!picking_enabled_ || event->button() != Qt::LeftButton) {
        updateInteractionCursor();
    }
}

void PointCloudWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (drag_button_ == Qt::NoButton) {
        updateHoveredPoint(event->position());
        return;
    }
    const QPoint delta = event->pos() - last_mouse_;
    last_mouse_ = event->pos();
    moved_since_press_ = moved_since_press_ ||
        (event->pos() - press_position_).manhattanLength() > 4;
    if (box_selection_enabled_ && drag_button_ == Qt::LeftButton) {
        box_selection_rect_ = QRectF(box_selection_start_, event->position()).normalized();
        update();
        return;
    }
    if (free_selection_enabled_ && drag_button_ == Qt::LeftButton) {
        if (free_selection_path_.isEmpty() ||
            QLineF(free_selection_path_.back(), event->position()).length() >= 2.0) {
            free_selection_path_ << event->position();
        }
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
        yaw_degrees_ = wrapDegrees(yaw_degrees_ + delta.x() * 0.55);
        pitch_degrees_ = wrapDegrees(pitch_degrees_ - delta.y() * 0.4);
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
        updateInteractionCursor();
        finishInteractiveRendering();
        update();
        return;
    }
    if (free_selection_enabled_ && event->button() == Qt::LeftButton) {
        if (free_selection_path_.isEmpty() ||
            QLineF(free_selection_path_.back(), event->position()).length() >= 1.0) {
            free_selection_path_ << event->position();
        }
        const QVector<int> selected = indicesInScreenPolygon(free_selection_path_);
        free_selection_path_.clear();
        drag_button_ = Qt::NoButton;
        emit boxSelectionFinished(selected);
        updateInteractionCursor();
        finishInteractiveRendering();
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
        updateInteractionCursor();
        finishInteractiveRendering();
        update();
        return;
    }
    if (picking_enabled_ && event->button() == Qt::LeftButton && !moved_since_press_) {
        const int index = pickNearest(event->position());
        if (index >= 0) emit pointPicked(index);
    }
    drag_button_ = Qt::NoButton;
    updateInteractionCursor();
    finishInteractiveRendering();
    update();
}

void PointCloudWidget::mouseDoubleClickEvent(QMouseEvent*)
{
    resetView();
}

void PointCloudWidget::wheelEvent(QWheelEvent* event)
{
    const QPoint angle_delta = event->angleDelta();
    const QPoint pixel_delta = event->pixelDelta();
    const double steps = !pixel_delta.isNull()
        ? pixel_delta.y() / 120.0
        : angle_delta.y() / 120.0;
    if (std::abs(steps) < 1e-9) {
        event->ignore();
        return;
    }
    beginInteractiveRendering();
    const double old_scale = view_scale_;
    view_scale_ = std::clamp(view_scale_ * std::pow(1.12, steps), 0.25, 8.0);
    const double applied_factor = view_scale_ / old_scale;
    const QPointF cursor_offset = event->position() - QPointF(rect().center());
    pan_ = cursor_offset - (cursor_offset - pan_) * applied_factor;
    invalidateProjectionCache();
    update();
    finishInteractiveRendering(120);
    event->accept();
}

void PointCloudWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        box_selection_enabled_ = false;
        free_selection_enabled_ = false;
        section_selection_enabled_ = false;
        picking_enabled_ = false;
        box_selection_rect_ = {};
        free_selection_path_.clear();
        section_start_ = section_end_ = {};
        drag_button_ = Qt::NoButton;
        interactive_rendering_ = false;
        interaction_idle_timer_.stop();
        invalidateRenderProjection();
        updateInteractionCursor();
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

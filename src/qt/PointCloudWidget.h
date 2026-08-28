#pragma once

#include "pointcloud/PointCloud.h"
#include "pointcloud/PointCloudGeometricModel.h"
#include "pointcloud/PointCloudProcessor.h"

#include <QColor>
#include <QHash>
#include <QOpenGLWidget>
#include <QPoint>
#include <QPolygonF>
#include <QRectF>
#include <QTimer>
#include <QVector>

enum class PointCloudColorMode {
    Original,
    Height,
    Solid
};

class PointCloudWidget final : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit PointCloudWidget(QWidget* parent = nullptr);

    void setCloud(const PointCloud& cloud, bool resetView = true);
    const PointCloud& cloud() const { return cloud_; }
    bool hasCloud() const { return !cloud_.Empty(); }
    void setColorMode(PointCloudColorMode mode);
    void setPointSize(double size);
    void setAxesVisible(bool visible);
    void setFittedPlane(const PointCloudPlane& plane);
    void setFittedPlaneVisible(bool visible);
    void setGeometricModels(const std::vector<PointCloudGeometricModel>& models);
    void setActiveGeometricModel(std::uint64_t id);
    void setResidualColoringEnabled(bool enabled);
    void setPickingEnabled(bool enabled);
    void setBoxSelectionEnabled(bool enabled);
    void setFreeSelectionEnabled(bool enabled);
    void setSectionSelectionEnabled(bool enabled, double halfWidthPixels = 10.0);
    void setSelectionPreviewIndices(const QVector<int>& indices);
    void setHighlightedIndices(const QVector<int>& indices);
    void resetView();

    PointCloudColorMode colorMode() const { return color_mode_; }
    double pointSize() const { return point_size_; }
    bool axesVisible() const { return axes_visible_; }
    const PointCloudPlane& fittedPlane() const { return fitted_plane_; }
    bool fittedPlaneVisible() const { return fitted_plane_visible_; }
    const std::vector<PointCloudGeometricModel>& geometricModels() const { return geometric_models_; }
    std::uint64_t activeGeometricModel() const { return active_model_id_; }
    bool residualColoringEnabled() const { return residual_coloring_enabled_; }
    bool pickingEnabled() const { return picking_enabled_; }
    bool boxSelectionEnabled() const { return box_selection_enabled_; }
    bool freeSelectionEnabled() const { return free_selection_enabled_; }
    Qt::KeyboardModifiers selectionModifiers() const { return selection_modifiers_; }
    bool sectionSelectionEnabled() const { return section_selection_enabled_; }
    int renderedPointCount() const { return rendered_point_count_; }
    bool interactiveRendering() const { return interactive_rendering_; }
    int hoveredPointIndex() const { return hovered_point_index_; }
    double yawDegrees() const { return yaw_degrees_; }
    double pitchDegrees() const { return pitch_degrees_; }
    QString renderBackend() const { return render_backend_; }
    bool hardwareAccelerated() const { return hardware_accelerated_; }
    int pickNearest(const QPointF& position, double radius = 12.0) const;
    QVector<int> indicesInScreenRect(const QRectF& rectangle) const;
    QVector<int> indicesInScreenPolygon(const QPolygonF& polygon) const;
    QVector<int> indicesNearScreenLine(
        const QPointF& first,
        const QPointF& second,
        double halfWidthPixels) const;
    QPointF screenPosition(int point_index) const;

signals:
    void pointPicked(int pointIndex);
    void boxSelectionFinished(const QVector<int>& pointIndices);
    void sectionSelectionFinished(
        const QVector<int>& pointIndices,
        const QPointF& first,
        const QPointF& second);
    void renderBackendChanged(const QString& description, bool hardwareAccelerated);
    void renderStatisticsChanged(int renderedPointCount, bool interactive);
    void interactionCancelled();

protected:
    void initializeGL() override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct ProjectedPoint {
        QPointF position;
        double depth = 0.0;
        int index = -1;
    };

    ProjectedPoint projectPoint(int index) const;
    ProjectedPoint projectPointValue(const PointCloudPoint& point, int index = -1) const;
    QColor pointColor(const PointCloudPoint& point, int index) const;
    void drawGeometricModels(QPainter& painter) const;
    void invalidateProjectionCache();
    void discardProjectionCaches();
    void invalidateRenderProjection();
    void ensureInteractionProjectionCache() const;
    void beginInteractiveRendering();
    void finishInteractiveRendering(int delayMs = 90);
    void updateInteractionCursor();
    void updateHoveredPoint(const QPointF& position);
    int pickNearestRendered(const QPointF& position, double radius) const;
    int currentRenderBudget() const;
    static qint64 screenCellKey(int x, int y);

    PointCloud cloud_;
    mutable QVector<ProjectedPoint> projected_points_;
    mutable QVector<ProjectedPoint> interaction_projected_points_;
    mutable QHash<qint64, QVector<int>> screen_index_;
    mutable bool interaction_projection_valid_ = false;
    mutable bool render_projection_valid_ = false;
    QVector<int> highlighted_indices_;
    QVector<int> selection_preview_indices_;
    PointCloudColorMode color_mode_ = PointCloudColorMode::Height;
    double point_size_ = 2.5;
    double yaw_degrees_ = -38.0;
    double pitch_degrees_ = 26.0;
    double view_scale_ = 1.0;
    QPointF pan_;
    QPoint press_position_;
    QPoint last_mouse_;
    Qt::MouseButton drag_button_ = Qt::NoButton;
    bool moved_since_press_ = false;
    bool axes_visible_ = true;
    PointCloudPlane fitted_plane_;
    bool fitted_plane_visible_ = true;
    std::vector<PointCloudGeometricModel> geometric_models_;
    std::uint64_t active_model_id_ = 0;
    bool residual_coloring_enabled_ = false;
    double active_residual_scale_ = 1.0;
    bool picking_enabled_ = false;
    bool box_selection_enabled_ = false;
    bool free_selection_enabled_ = false;
    bool section_selection_enabled_ = false;
    bool interactive_rendering_ = false;
    QTimer interaction_idle_timer_;
    int hovered_point_index_ = -1;
    QPointF box_selection_start_;
    QRectF box_selection_rect_;
    QPolygonF free_selection_path_;
    Qt::KeyboardModifiers selection_modifiers_ = Qt::NoModifier;
    QPointF section_start_;
    QPointF section_end_;
    double section_half_width_pixels_ = 10.0;
    int rendered_point_count_ = 0;
    int reported_rendered_point_count_ = -1;
    bool reported_interactive_rendering_ = false;
    QString render_backend_ = QStringLiteral("OpenGL 正在初始化…");
    bool hardware_accelerated_ = false;
};

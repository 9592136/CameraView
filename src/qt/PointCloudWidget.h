#pragma once

#include "pointcloud/PointCloud.h"

#include <QColor>
#include <QOpenGLWidget>
#include <QPoint>
#include <QRectF>
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

    void setCloud(const PointCloud& cloud);
    const PointCloud& cloud() const { return cloud_; }
    bool hasCloud() const { return !cloud_.Empty(); }
    void setColorMode(PointCloudColorMode mode);
    void setPointSize(double size);
    void setAxesVisible(bool visible);
    void setPickingEnabled(bool enabled);
    void setBoxSelectionEnabled(bool enabled);
    void setSelectionPreviewIndices(const QVector<int>& indices);
    void setHighlightedIndices(const QVector<int>& indices);
    void resetView();

    PointCloudColorMode colorMode() const { return color_mode_; }
    double pointSize() const { return point_size_; }
    bool axesVisible() const { return axes_visible_; }
    bool pickingEnabled() const { return picking_enabled_; }
    bool boxSelectionEnabled() const { return box_selection_enabled_; }
    int renderedPointCount() const { return rendered_point_count_; }
    QString renderBackend() const { return render_backend_; }
    bool hardwareAccelerated() const { return hardware_accelerated_; }
    int pickNearest(const QPointF& position, double radius = 12.0) const;
    QVector<int> indicesInScreenRect(const QRectF& rectangle) const;
    QPointF screenPosition(int point_index) const;

signals:
    void pointPicked(int pointIndex);
    void boxSelectionFinished(const QVector<int>& pointIndices);
    void renderBackendChanged(const QString& description, bool hardwareAccelerated);

protected:
    void initializeGL() override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct ProjectedPoint {
        QPointF position;
        double depth = 0.0;
        int index = -1;
    };

    ProjectedPoint projectPoint(int index) const;
    ProjectedPoint projectPointValue(const PointCloudPoint& point, int index = -1) const;
    QColor pointColor(const PointCloudPoint& point) const;

    PointCloud cloud_;
    mutable QVector<ProjectedPoint> projected_points_;
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
    bool picking_enabled_ = false;
    bool box_selection_enabled_ = false;
    QPointF box_selection_start_;
    QRectF box_selection_rect_;
    int rendered_point_count_ = 0;
    QString render_backend_ = QStringLiteral("OpenGL 正在初始化…");
    bool hardware_accelerated_ = false;
};

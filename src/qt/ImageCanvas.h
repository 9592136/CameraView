#pragma once

#include <QImage>
#include <QPointF>
#include <QVector>
#include <QWidget>

enum class CanvasTool {
    None,
    Calibration,
    Length,
    ProfileLine,
    Angle,
    Rectangle,
    Polygon,
    Point,
    Polyline,
    Circle,
    Ellipse,
    SmartCountSample,
    SmartCountResult,
    CameraRoi
};

struct CanvasOverlay {
    CanvasTool kind = CanvasTool::None;
    QVector<QPointF> points;
    QString label;
    QColor color = QColor(46, 204, 113);
    bool highlighted = false;
    bool editable = false;
    int source_index = -1;
};

class ImageCanvas final : public QWidget {
    Q_OBJECT

public:
    explicit ImageCanvas(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void setLivePreviewOverlay(const QImage& image);
    void setOverlays(QVector<CanvasOverlay> overlays);
    void setTool(CanvasTool tool);
    CanvasTool tool() const { return tool_; }
    void setEdgeSnappingEnabled(bool enabled);
    bool edgeSnappingEnabled() const { return edge_snapping_enabled_; }
    void setEdgeSnapRadius(int radius);
    int edgeSnapRadius() const { return edge_snap_radius_; }
    void fitToView();
    bool focusOnImageRect(const QRectF& imageRegion);
    double zoom() const { return zoom_; }
    QPointF viewportCenterInImage() const;
    qint64 imageCacheKey() const { return image_.cacheKey(); }
    QSize imageSize() const { return image_.size(); }
    bool hasGrayscaleCache() const { return !grayscale_image_.isNull(); }
    bool hasLivePreviewOverlay() const { return !live_preview_overlay_.isNull(); }
    const QVector<CanvasOverlay>& overlays() const { return overlays_; }

signals:
    void pointsCommitted(CanvasTool tool, QVector<QPointF> points);
    void imagePositionChanged(QPointF point);
    void zoomChanged(double zoom);
    void toolChanged(CanvasTool tool);
    void toolCancelled(CanvasTool tool);
    void edgeSnapEvaluated(bool snapped, QPointF original, QPointF result, double strength);
    void overlaySelected(int sourceIndex);
    void overlayPointMoved(int sourceIndex, int pointIndex, QPointF point, bool finished);
    void overlayMoved(int sourceIndex, QPointF delta, bool finished);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QRectF imageRect() const;
    QPointF widgetToImage(const QPointF& point) const;
    QPointF imageToWidget(const QPointF& point) const;
    bool containsImagePoint(const QPointF& point) const;
    bool findEditableHandle(
        const QPointF& widgetPoint,
        int& overlayIndex,
        int& pointIndex) const;
    int findEditableOverlayBody(const QPointF& widgetPoint) const;
    bool overlayBodyContains(const CanvasOverlay& overlay, const QPointF& widgetPoint) const;
    QPointF snapToNearestEdge(const QPointF& point, bool* snapped, double* strength) const;
    void commitIfComplete();
    void drawOverlay(QPainter& painter, const CanvasOverlay& overlay) const;

    QImage image_;
    QImage grayscale_image_;
    QImage live_preview_overlay_;
    QVector<CanvasOverlay> overlays_;
    QVector<QPointF> pending_points_;
    CanvasTool tool_ = CanvasTool::None;
    double zoom_ = 1.0;
    QPointF pan_;
    QPointF last_mouse_;
    QPointF hover_image_point_;
    bool hover_image_point_valid_ = false;
    bool panning_ = false;
    bool camera_roi_dragging_ = false;
    int dragged_overlay_index_ = -1;
    int dragged_point_index_ = -1;
    bool dragging_overlay_body_ = false;
    QPointF drag_last_image_point_;
    bool edge_snapping_enabled_ = false;
    int edge_snap_radius_ = 12;
};

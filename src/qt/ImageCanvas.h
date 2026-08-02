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
    Polygon
};

struct CanvasOverlay {
    CanvasTool kind = CanvasTool::None;
    QVector<QPointF> points;
    QString label;
    QColor color = QColor(46, 204, 113);
};

class ImageCanvas final : public QWidget {
    Q_OBJECT

public:
    explicit ImageCanvas(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void setOverlays(QVector<CanvasOverlay> overlays);
    void setTool(CanvasTool tool);
    CanvasTool tool() const { return tool_; }
    void fitToView();
    bool focusOnImageRect(const QRectF& imageRegion);
    double zoom() const { return zoom_; }
    QPointF viewportCenterInImage() const;

signals:
    void pointsCommitted(CanvasTool tool, QVector<QPointF> points);
    void imagePositionChanged(QPointF point);
    void zoomChanged(double zoom);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QRectF imageRect() const;
    QPointF widgetToImage(const QPointF& point) const;
    QPointF imageToWidget(const QPointF& point) const;
    bool containsImagePoint(const QPointF& point) const;
    void commitIfComplete();
    void drawOverlay(QPainter& painter, const CanvasOverlay& overlay) const;

    QImage image_;
    QVector<CanvasOverlay> overlays_;
    QVector<QPointF> pending_points_;
    CanvasTool tool_ = CanvasTool::None;
    double zoom_ = 1.0;
    QPointF pan_;
    QPointF last_mouse_;
    bool panning_ = false;
};

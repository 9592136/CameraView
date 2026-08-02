#pragma once

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QVector>
#include <QWidget>

enum class SurfaceColorMode {
    Original,
    HeightMap,
    Grayscale
};

class ImageSurface3DWidget final : public QWidget {
    Q_OBJECT

public:
    explicit ImageSurface3DWidget(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void setVerticalScale(double scale);
    void setResolution(int resolution);
    void setColorMode(SurfaceColorMode mode);
    void setMeshVisible(bool visible);
    void resetView();

    bool hasSurface() const { return !heights_.isEmpty(); }
    QSize gridSize() const { return QSize(columns_, rows_); }
    double verticalScale() const { return vertical_scale_; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void rebuildSurface();
    QColor surfaceColor(int index) const;

    QImage source_;
    QVector<float> heights_;
    QVector<QColor> original_colors_;
    int columns_ = 0;
    int rows_ = 0;
    int resolution_ = 80;
    double vertical_scale_ = 1.25;
    double yaw_degrees_ = -35.0;
    double pitch_degrees_ = 52.0;
    double view_scale_ = 1.0;
    QPointF pan_;
    QPoint last_mouse_;
    Qt::MouseButton drag_button_ = Qt::NoButton;
    SurfaceColorMode color_mode_ = SurfaceColorMode::HeightMap;
    bool mesh_visible_ = true;
};

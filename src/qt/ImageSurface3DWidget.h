#pragma once

#include <QColor>
#include <QImage>
#include <QOpenGLWidget>
#include <QPoint>
#include <QString>
#include <QVector>

enum class SurfaceColorMode {
    Original,
    HeightMap,
    Grayscale
};

enum class SurfaceHeightChannel {
    Luminance,
    Red,
    Green,
    Blue
};

class ImageSurface3DWidget final : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit ImageSurface3DWidget(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void setVerticalScale(double scale);
    void setResolution(int resolution);
    void setHeightChannel(SurfaceHeightChannel channel);
    void setColorMode(SurfaceColorMode mode);
    void setMeshVisible(bool visible);
    void resetView();

    bool hasSurface() const { return !heights_.isEmpty(); }
    QSize gridSize() const { return QSize(columns_, rows_); }
    double verticalScale() const { return vertical_scale_; }
    SurfaceHeightChannel heightChannel() const { return height_channel_; }
    float heightAt(int column, int row) const;
    int lastRenderStride() const { return last_render_stride_; }
    int lastRenderedFaceCount() const { return last_rendered_face_count_; }
    QString renderBackend() const { return render_backend_; }
    bool hardwareAccelerated() const { return hardware_accelerated_; }

signals:
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
    void rebuildSurface();
    void updateHeights();
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
    SurfaceHeightChannel height_channel_ = SurfaceHeightChannel::Luminance;
    SurfaceColorMode color_mode_ = SurfaceColorMode::HeightMap;
    bool mesh_visible_ = true;
    int last_render_stride_ = 1;
    int last_rendered_face_count_ = 0;
    QString render_backend_ = QStringLiteral("OpenGL 初始化中…");
    bool hardware_accelerated_ = false;
};

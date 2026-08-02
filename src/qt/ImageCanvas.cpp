#include "ImageCanvas.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QLineF>
#include <QKeyEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

int requiredPointCount(CanvasTool tool)
{
    switch (tool) {
    case CanvasTool::Calibration:
    case CanvasTool::Length:
    case CanvasTool::ProfileLine:
    case CanvasTool::Rectangle:
    case CanvasTool::Circle:
    case CanvasTool::Ellipse:
    case CanvasTool::SmartCountSample:
        return 2;
    case CanvasTool::Angle:
        return 3;
    case CanvasTool::Point:
        return 1;
    default:
        return 0;
    }
}

} // namespace

ImageCanvas::ImageCanvas(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(480, 320);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
}

void ImageCanvas::setImage(const QImage& image)
{
    const bool size_changed = image_.size() != image.size();
    image_ = image;
    grayscale_image_ = image_.convertToFormat(QImage::Format_Grayscale8);
    if (size_changed) {
        fitToView();
    }
    update();
}

void ImageCanvas::setEdgeSnappingEnabled(bool enabled)
{
    edge_snapping_enabled_ = enabled;
}

void ImageCanvas::setEdgeSnapRadius(int radius)
{
    edge_snap_radius_ = std::clamp(radius, 2, 40);
}

void ImageCanvas::setOverlays(QVector<CanvasOverlay> overlays)
{
    overlays_ = std::move(overlays);
    update();
}

void ImageCanvas::setTool(CanvasTool tool)
{
    tool_ = tool;
    pending_points_.clear();
    hover_image_point_valid_ = false;
    setCursor(tool == CanvasTool::None ? Qt::ArrowCursor : Qt::CrossCursor);
    emit toolChanged(tool_);
    update();
}

void ImageCanvas::fitToView()
{
    zoom_ = 1.0;
    pan_ = {};
    emit zoomChanged(zoom_);
    update();
}

bool ImageCanvas::focusOnImageRect(const QRectF& imageRegion)
{
    if (image_.isNull() || width() <= 0 || height() <= 0) {
        return false;
    }

    QRectF bounds = imageRegion.normalized().intersected(
        QRectF(QPointF(0.0, 0.0), QSizeF(image_.size())));
    if (bounds.isEmpty()) {
        return false;
    }

    constexpr double focus_padding = 2.0;
    const double fit = std::min(
        width() / static_cast<double>(image_.width()),
        height() / static_cast<double>(image_.height()));
    const double padded_width = std::max(1.0, bounds.width() * focus_padding);
    const double padded_height = std::max(1.0, bounds.height() * focus_padding);
    zoom_ = std::clamp(std::min(
        width() / (padded_width * fit),
        height() / (padded_height * fit)), 1.0, 20.0);

    const double scale = fit * zoom_;
    const QSizeF draw_size(image_.width() * scale, image_.height() * scale);
    const QPointF unpanned_top_left(
        (width() - draw_size.width()) / 2.0,
        (height() - draw_size.height()) / 2.0);
    const QPointF target_widget_position(
        unpanned_top_left.x() + bounds.center().x() * scale,
        unpanned_top_left.y() + bounds.center().y() * scale);
    pan_ = rect().center() - target_widget_position;

    emit zoomChanged(zoom_);
    update();
    return true;
}

QPointF ImageCanvas::viewportCenterInImage() const
{
    return image_.isNull() ? QPointF() : widgetToImage(rect().center());
}

QRectF ImageCanvas::imageRect() const
{
    if (image_.isNull()) {
        return {};
    }
    const double fit = std::min(
        width() / static_cast<double>(image_.width()),
        height() / static_cast<double>(image_.height()));
    const QSizeF size(image_.width() * fit * zoom_, image_.height() * fit * zoom_);
    const QPointF top_left((width() - size.width()) / 2.0, (height() - size.height()) / 2.0);
    return QRectF(top_left + pan_, size);
}

QPointF ImageCanvas::widgetToImage(const QPointF& point) const
{
    const QRectF rect = imageRect();
    if (rect.isEmpty()) {
        return {};
    }
    return {
        (point.x() - rect.left()) * image_.width() / rect.width(),
        (point.y() - rect.top()) * image_.height() / rect.height()};
}

QPointF ImageCanvas::imageToWidget(const QPointF& point) const
{
    const QRectF rect = imageRect();
    return {
        rect.left() + point.x() * rect.width() / image_.width(),
        rect.top() + point.y() * rect.height() / image_.height()};
}

bool ImageCanvas::containsImagePoint(const QPointF& point) const
{
    return !image_.isNull() && point.x() >= 0.0 && point.y() >= 0.0 &&
        point.x() < image_.width() && point.y() < image_.height();
}

void ImageCanvas::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    QLinearGradient background(rect().topLeft(), rect().bottomRight());
    background.setColorAt(0.0, QColor(14, 19, 26));
    background.setColorAt(1.0, QColor(20, 27, 36));
    painter.fillRect(rect(), background);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, zoom_ < 4.0);

    painter.setPen(QPen(QColor(255, 255, 255, 6), 1.0));
    constexpr int grid_size = 32;
    for (int x = 0; x < width(); x += grid_size) painter.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += grid_size) painter.drawLine(0, y, width(), y);

    if (image_.isNull()) {
        const QPointF center = rect().center();
        const QRectF camera_body(center.x() - 31.0, center.y() - 58.0, 62.0, 42.0);
        painter.setPen(QPen(QColor(91, 112, 139), 2.0));
        painter.setBrush(QColor(28, 39, 52));
        painter.drawRoundedRect(camera_body, 9.0, 9.0);
        painter.drawRoundedRect(QRectF(center.x() - 15.0, center.y() - 66.0, 30.0, 10.0), 4.0, 4.0);
        painter.setBrush(QColor(18, 26, 36));
        painter.drawEllipse(center + QPointF(0.0, -37.0), 12.0, 12.0);
        painter.setPen(QColor(218, 226, 237));
        QFont title_font = painter.font();
        title_font.setPointSize(title_font.pointSize() + 2);
        title_font.setWeight(QFont::DemiBold);
        painter.setFont(title_font);
        painter.drawText(
            QRectF(0.0, center.y() + 4.0, width(), 28.0),
            Qt::AlignHCenter | Qt::AlignVCenter,
            tr("尚未载入图像"));
        QFont detail_font = painter.font();
        detail_font.setPointSize(std::max(8, detail_font.pointSize() - 2));
        detail_font.setWeight(QFont::Normal);
        painter.setFont(detail_font);
        painter.setPen(QColor(126, 141, 160));
        painter.drawText(
            QRectF(20.0, center.y() + 35.0, width() - 40.0, 24.0),
            Qt::AlignHCenter | Qt::AlignVCenter,
            tr("拖放图像到此处，或按 Ctrl+O 打开图像"));
        return;
    }

    const QRectF target = imageRect();
    painter.fillRect(target.translated(0.0, 7.0), QColor(0, 0, 0, 80));
    painter.drawImage(target, image_);
    painter.setPen(QPen(QColor(105, 122, 145, 110), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(target);
    painter.setClipRect(target);
    for (const CanvasOverlay& overlay : overlays_) {
        drawOverlay(painter, overlay);
    }
    if (!pending_points_.isEmpty()) {
        QVector<QPointF> preview = pending_points_;
        if (hover_image_point_valid_ &&
            (preview.isEmpty() || QLineF(preview.last(), hover_image_point_).length() >= 0.01)) {
            preview.push_back(hover_image_point_);
        }
        CanvasOverlay pending{tool_, preview, tr("进行中"), QColor(255, 193, 7)};
        drawOverlay(painter, pending);
    }
}

void ImageCanvas::drawOverlay(QPainter& painter, const CanvasOverlay& overlay) const
{
    if (overlay.points.isEmpty()) {
        return;
    }
    QVector<QPointF> points;
    points.reserve(overlay.points.size());
    for (const QPointF& point : overlay.points) {
        points.push_back(imageToWidget(point));
    }

    QPen pen(overlay.highlighted ? overlay.color.lighter(145) : overlay.color,
        overlay.highlighted ? 3.5 : 2.0);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if ((overlay.kind == CanvasTool::Rectangle || overlay.kind == CanvasTool::Ellipse ||
        overlay.kind == CanvasTool::SmartCountSample || overlay.kind == CanvasTool::SmartCountResult) &&
        points.size() >= 2) {
        if (overlay.kind == CanvasTool::Ellipse) {
            painter.drawEllipse(QRectF(points[0], points[1]).normalized());
        } else {
            painter.drawRect(QRectF(points[0], points[1]).normalized());
        }
    } else if (overlay.kind == CanvasTool::Circle && points.size() >= 2) {
        const double radius = QLineF(points[0], points[1]).length();
        painter.drawEllipse(points[0], radius, radius);
        painter.drawLine(points[0], points[1]);
    } else if (overlay.kind == CanvasTool::Point) {
        constexpr double arm = 8.0;
        painter.drawLine(points[0] + QPointF(-arm, 0.0), points[0] + QPointF(arm, 0.0));
        painter.drawLine(points[0] + QPointF(0.0, -arm), points[0] + QPointF(0.0, arm));
    } else if (overlay.kind == CanvasTool::Polygon && points.size() >= 2) {
        painter.drawPolyline(points.constData(), points.size());
        if (points.size() >= 3) {
            painter.drawLine(points.last(), points.first());
        }
    } else if (points.size() >= 2) {
        for (int index = 1; index < points.size(); ++index) {
            painter.drawLine(points[index - 1], points[index]);
        }
    }

    painter.setBrush(overlay.color);
    for (const QPointF& point : points) {
        painter.drawEllipse(point, 4.0, 4.0);
    }
    if (!overlay.label.isEmpty()) {
        const QPointF anchor = points.last() + QPointF(8.0, -8.0);
        painter.setPen(QPen(QColor(10, 10, 10, 210), 4.0));
        painter.drawText(anchor, overlay.label);
        painter.setPen(QPen(overlay.color, 1.0));
        painter.drawText(anchor, overlay.label);
    }
}

void ImageCanvas::wheelEvent(QWheelEvent* event)
{
    if (image_.isNull()) {
        return;
    }
    const QPointF before = widgetToImage(event->position());
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    zoom_ = std::clamp(zoom_ * factor, 0.2, 20.0);
    const QPointF after_widget = imageToWidget(before);
    pan_ += event->position() - after_widget;
    emit zoomChanged(zoom_);
    update();
}

void ImageCanvas::mousePressEvent(QMouseEvent* event)
{
    last_mouse_ = event->position();
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        panning_ = true;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (event->button() != Qt::LeftButton || tool_ == CanvasTool::None) {
        if (event->button() == Qt::LeftButton && tool_ == CanvasTool::None) {
            double bestDistance = std::numeric_limits<double>::max();
            for (int overlayIndex = 0; overlayIndex < overlays_.size(); ++overlayIndex) {
                const CanvasOverlay& overlay = overlays_[overlayIndex];
                if (!overlay.editable || overlay.source_index < 0) continue;
                for (int pointIndex = 0; pointIndex < overlay.points.size(); ++pointIndex) {
                    const double distance = QLineF(event->position(), imageToWidget(overlay.points[pointIndex])).length();
                    if (distance <= 10.0 && distance < bestDistance) {
                        bestDistance = distance;
                        dragged_overlay_index_ = overlayIndex;
                        dragged_point_index_ = pointIndex;
                    }
                }
            }
            if (dragged_overlay_index_ >= 0) {
                setCursor(Qt::SizeAllCursor);
                event->accept();
            }
        }
        return;
    }
    const QPointF original = widgetToImage(event->position());
    QPointF point = original;
    if (!containsImagePoint(point)) {
        return;
    }
    if (edge_snapping_enabled_) {
        bool snapped = false;
        double strength = 0.0;
        point = snapToNearestEdge(original, &snapped, &strength);
        emit edgeSnapEvaluated(snapped, original, point, strength);
    }
    pending_points_.push_back(point);
    commitIfComplete();
    update();
}

void ImageCanvas::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF image_point = widgetToImage(event->position());
    if (containsImagePoint(image_point)) {
        emit imagePositionChanged(image_point);
        bool ignored = false;
        double ignoredStrength = 0.0;
        hover_image_point_ = edge_snapping_enabled_
            ? snapToNearestEdge(image_point, &ignored, &ignoredStrength) : image_point;
        hover_image_point_valid_ = true;
        if (!pending_points_.isEmpty()) update();
    } else if (hover_image_point_valid_) {
        hover_image_point_valid_ = false;
        if (!pending_points_.isEmpty()) update();
    }
    if (panning_) {
        pan_ += event->position() - last_mouse_;
        last_mouse_ = event->position();
        update();
    } else if (dragged_overlay_index_ >= 0 && dragged_point_index_ >= 0 &&
        dragged_overlay_index_ < overlays_.size()) {
        QPointF target = image_point;
        if (containsImagePoint(target)) {
            bool ignored = false;
            double ignoredStrength = 0.0;
            if (edge_snapping_enabled_) target = snapToNearestEdge(target, &ignored, &ignoredStrength);
            CanvasOverlay& overlay = overlays_[dragged_overlay_index_];
            if (dragged_point_index_ < overlay.points.size()) {
                overlay.points[dragged_point_index_] = target;
                emit overlayPointMoved(overlay.source_index, dragged_point_index_, target, false);
                update();
            }
        }
    }
}

void ImageCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && tool_ != CanvasTool::None) {
        setTool(CanvasTool::None);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ImageCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && dragged_overlay_index_ >= 0 &&
        dragged_overlay_index_ < overlays_.size()) {
        const CanvasOverlay& overlay = overlays_[dragged_overlay_index_];
        if (dragged_point_index_ >= 0 && dragged_point_index_ < overlay.points.size()) {
            emit overlayPointMoved(overlay.source_index, dragged_point_index_,
                overlay.points[dragged_point_index_], true);
        }
        dragged_overlay_index_ = -1;
        dragged_point_index_ = -1;
        setCursor(Qt::ArrowCursor);
        return;
    }
    if (panning_ && (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton)) {
        panning_ = false;
        setCursor(tool_ == CanvasTool::None ? Qt::ArrowCursor : Qt::CrossCursor);
    }
}

void ImageCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
    const bool variableTool = tool_ == CanvasTool::Polygon || tool_ == CanvasTool::Polyline;
    const int minimumPoints = tool_ == CanvasTool::Polygon ? 3 : 2;
    if (variableTool && event->button() == Qt::LeftButton && pending_points_.size() >= minimumPoints) {
        QVector<QPointF> points = pending_points_;
        while (points.size() >= 2 && QLineF(points[points.size() - 2], points.last()).length() < 0.01) {
            points.removeLast();
        }
        if (points.size() < minimumPoints) return;
        pending_points_.clear();
        emit pointsCommitted(tool_, points);
        update();
    }
}

QPointF ImageCanvas::snapToNearestEdge(const QPointF& point, bool* snapped, double* strength) const
{
    if (snapped) *snapped = false;
    if (strength) *strength = 0.0;
    if (grayscale_image_.isNull() || grayscale_image_.width() < 3 || grayscale_image_.height() < 3) {
        return point;
    }

    const int centerX = qRound(point.x());
    const int centerY = qRound(point.y());
    const int left = std::max(1, centerX - edge_snap_radius_);
    const int right = std::min(grayscale_image_.width() - 2, centerX + edge_snap_radius_);
    const int top = std::max(1, centerY - edge_snap_radius_);
    const int bottom = std::min(grayscale_image_.height() - 2, centerY + edge_snap_radius_);
    if (left > right || top > bottom) return point;

    auto valueAt = [this](int x, int y) {
        return static_cast<int>(grayscale_image_.constScanLine(y)[x]);
    };
    double bestScore = 0.0;
    double bestMagnitude = 0.0;
    QPointF best = point;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            const int gx = -valueAt(x - 1, y - 1) + valueAt(x + 1, y - 1)
                - 2 * valueAt(x - 1, y) + 2 * valueAt(x + 1, y)
                - valueAt(x - 1, y + 1) + valueAt(x + 1, y + 1);
            const int gy = -valueAt(x - 1, y - 1) - 2 * valueAt(x, y - 1) - valueAt(x + 1, y - 1)
                + valueAt(x - 1, y + 1) + 2 * valueAt(x, y + 1) + valueAt(x + 1, y + 1);
            const double magnitude = std::hypot(static_cast<double>(gx), static_cast<double>(gy));
            const double distance = std::hypot(x - point.x(), y - point.y());
            const double score = magnitude - distance * 3.0;
            if (score > bestScore) {
                bestScore = score;
                bestMagnitude = magnitude;
                best = QPointF(x, y);
            }
        }
    }

    constexpr double minimumSobelMagnitude = 80.0;
    if (bestMagnitude >= minimumSobelMagnitude) {
        if (snapped) *snapped = true;
        if (strength) *strength = bestMagnitude;
        return best;
    }
    return point;
}

void ImageCanvas::commitIfComplete()
{
    const int required = requiredPointCount(tool_);
    if (required > 0 && pending_points_.size() >= required) {
        const QVector<QPointF> points = pending_points_;
        pending_points_.clear();
        emit pointsCommitted(tool_, points);
    }
}

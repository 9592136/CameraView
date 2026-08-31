#include "ImageCanvas.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
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

double distanceToSegment(const QPointF& point, const QPointF& first, const QPointF& second)
{
    const QPointF segment = second - first;
    const double length_squared = QPointF::dotProduct(segment, segment);
    if (length_squared <= 0.000001) return QLineF(point, first).length();
    const double projection = std::clamp(
        QPointF::dotProduct(point - first, segment) / length_squared, 0.0, 1.0);
    return QLineF(point, first + segment * projection).length();
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
    // Grayscale conversion touches every pixel and is only needed by edge
    // snapping. Keep the normal live-preview path as an implicit QImage share.
    grayscale_image_ = edge_snapping_enabled_
        ? image_.convertToFormat(QImage::Format_Grayscale8)
        : QImage{};
    if (size_changed) {
        fitToView();
    }
    update();
}

void ImageCanvas::setLivePreviewOverlay(const QImage& image)
{
    live_preview_overlay_ = image;
    update();
}

void ImageCanvas::setEdgeSnappingEnabled(bool enabled)
{
    edge_snapping_enabled_ = enabled;
    grayscale_image_ = enabled && !image_.isNull()
        ? image_.convertToFormat(QImage::Format_Grayscale8)
        : QImage{};
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
    camera_roi_dragging_ = false;
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

bool ImageCanvas::findEditableHandle(
    const QPointF& widgetPoint,
    int& overlayIndex,
    int& pointIndex) const
{
    double best_distance = std::numeric_limits<double>::max();
    overlayIndex = -1;
    pointIndex = -1;
    for (int candidate_overlay = overlays_.size() - 1; candidate_overlay >= 0; --candidate_overlay) {
        const CanvasOverlay& overlay = overlays_[candidate_overlay];
        if (!overlay.editable || overlay.source_index < 0) continue;
        for (int candidate_point = 0; candidate_point < overlay.points.size(); ++candidate_point) {
            const double distance = QLineF(
                widgetPoint, imageToWidget(overlay.points[candidate_point])).length();
            if (distance <= 10.0 && distance < best_distance) {
                best_distance = distance;
                overlayIndex = candidate_overlay;
                pointIndex = candidate_point;
            }
        }
    }
    return overlayIndex >= 0;
}

int ImageCanvas::findEditableOverlayBody(const QPointF& widgetPoint) const
{
    for (int index = overlays_.size() - 1; index >= 0; --index) {
        if (overlays_[index].editable && overlays_[index].source_index >= 0 &&
            overlayBodyContains(overlays_[index], widgetPoint)) {
            return index;
        }
    }
    return -1;
}

bool ImageCanvas::overlayBodyContains(
    const CanvasOverlay& overlay,
    const QPointF& widgetPoint) const
{
    if (overlay.points.isEmpty()) return false;
    QVector<QPointF> points;
    points.reserve(overlay.points.size());
    for (const QPointF& point : overlay.points) points.push_back(imageToWidget(point));
    constexpr double tolerance = 8.0;

    if ((overlay.kind == CanvasTool::Rectangle || overlay.kind == CanvasTool::Ellipse) &&
        points.size() >= 2) {
        const QRectF bounds = QRectF(points[0], points[1]).normalized();
        if (overlay.kind == CanvasTool::Rectangle) {
            return bounds.adjusted(-tolerance, -tolerance, tolerance, tolerance)
                .contains(widgetPoint);
        }
        const QPointF center = bounds.center();
        const double radius_x = std::max(1.0, bounds.width() / 2.0 + tolerance);
        const double radius_y = std::max(1.0, bounds.height() / 2.0 + tolerance);
        const double dx = (widgetPoint.x() - center.x()) / radius_x;
        const double dy = (widgetPoint.y() - center.y()) / radius_y;
        return dx * dx + dy * dy <= 1.0;
    }
    if (overlay.kind == CanvasTool::Circle && points.size() >= 2) {
        const double radius = QLineF(points[0], points[1]).length();
        return QLineF(points[0], widgetPoint).length() <= radius + tolerance;
    }
    if (overlay.kind == CanvasTool::Polygon && points.size() >= 3) {
        QPainterPath path;
        path.addPolygon(QPolygonF(points));
        if (path.contains(widgetPoint)) return true;
        if (distanceToSegment(widgetPoint, points.last(), points.first()) <= tolerance) return true;
    }
    for (int index = 1; index < points.size(); ++index) {
        if (distanceToSegment(widgetPoint, points[index - 1], points[index]) <= tolerance) {
            return true;
        }
    }
    return false;
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
    painter.setClipping(false);
    if (!live_preview_overlay_.isNull()) {
        const QSize maximum(std::min(420, std::max(120, width() / 3)),
            std::min(300, std::max(90, height() / 3)));
        const QSize preview_size = live_preview_overlay_.size().scaled(
            maximum, Qt::KeepAspectRatio);
        const QRectF preview_rect(
            width() - preview_size.width() - 18.0,
            height() - preview_size.height() - 18.0,
            preview_size.width(), preview_size.height());
        painter.fillRect(preview_rect.adjusted(-7.0, -28.0, 7.0, 7.0), QColor(8, 12, 18, 225));
        painter.drawImage(preview_rect, live_preview_overlay_);
        painter.setPen(QPen(QColor(96, 165, 250), 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(preview_rect);
        painter.setPen(QColor(219, 234, 254));
        painter.drawText(
            preview_rect.adjusted(0.0, -25.0, 0.0, 0.0),
            Qt::AlignLeft | Qt::AlignTop,
            tr("实时相机"));
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
        overlay.kind == CanvasTool::CameraRoi ||
        overlay.kind == CanvasTool::SmartCountSample || overlay.kind == CanvasTool::SmartCountResult) &&
        points.size() >= 2) {
        if (overlay.kind == CanvasTool::Ellipse) {
            painter.drawEllipse(QRectF(points[0], points[1]).normalized());
        } else {
            if (overlay.kind == CanvasTool::CameraRoi) {
                pen.setStyle(Qt::DashLine);
                pen.setWidthF(2.5);
                painter.setPen(pen);
                painter.setBrush(QColor(255, 193, 7, 48));
            }
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

    const double handle_radius = overlay.highlighted && overlay.editable ? 5.5 : 4.0;
    for (const QPointF& point : points) {
        if (overlay.highlighted && overlay.editable) {
            painter.setPen(QPen(QColor(245, 249, 255), 2.0));
            painter.setBrush(QColor(13, 19, 27));
            painter.drawEllipse(point, handle_radius + 2.0, handle_radius + 2.0);
        }
        painter.setPen(QPen(overlay.color, 1.5));
        painter.setBrush(overlay.color);
        painter.drawEllipse(point, handle_radius, handle_radius);
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
            dragged_overlay_index_ = -1;
            dragged_point_index_ = -1;
            dragging_overlay_body_ = false;
            if (findEditableHandle(event->position(),
                    dragged_overlay_index_, dragged_point_index_)) {
                emit overlaySelected(overlays_[dragged_overlay_index_].source_index);
            } else {
                dragged_overlay_index_ = findEditableOverlayBody(event->position());
                if (dragged_overlay_index_ >= 0) {
                    dragging_overlay_body_ = true;
                    drag_last_image_point_ = widgetToImage(event->position());
                    emit overlaySelected(overlays_[dragged_overlay_index_].source_index);
                } else {
                    emit overlaySelected(-1);
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
    if (tool_ == CanvasTool::CameraRoi) {
        camera_roi_dragging_ = true;
        pending_points_ = {point, point};
        hover_image_point_ = point;
        hover_image_point_valid_ = true;
        event->accept();
        update();
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
    if (camera_roi_dragging_ && tool_ == CanvasTool::CameraRoi && !image_.isNull()) {
        const QPointF clamped(
            std::clamp(image_point.x(), 0.0, static_cast<double>(image_.width() - 1)),
            std::clamp(image_point.y(), 0.0, static_cast<double>(image_.height() - 1)));
        if (pending_points_.size() < 2) pending_points_ = {clamped, clamped};
        pending_points_[1] = clamped;
        hover_image_point_ = clamped;
        hover_image_point_valid_ = true;
        emit imagePositionChanged(clamped);
        update();
        event->accept();
        return;
    }
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
    } else if (dragged_overlay_index_ >= 0 && dragging_overlay_body_ &&
        dragged_overlay_index_ < overlays_.size()) {
        if (!containsImagePoint(image_point)) return;
        CanvasOverlay& overlay = overlays_[dragged_overlay_index_];
        QPointF delta = image_point - drag_last_image_point_;
        if (!overlay.points.isEmpty()) {
            double minimum_x = overlay.points.first().x();
            double maximum_x = minimum_x;
            double minimum_y = overlay.points.first().y();
            double maximum_y = minimum_y;
            for (const QPointF& point : overlay.points) {
                minimum_x = std::min(minimum_x, point.x());
                maximum_x = std::max(maximum_x, point.x());
                minimum_y = std::min(minimum_y, point.y());
                maximum_y = std::max(maximum_y, point.y());
            }
            const auto clamp_axis = [](double value, double lower, double upper) {
                // A malformed/imported overlay may already span beyond both image
                // edges. In that case there is no translation that can place every
                // point inside the image, so keep that axis stable instead of
                // passing an invalid range to std::clamp.
                return lower <= upper ? std::clamp(value, lower, upper) : 0.0;
            };
            delta.setX(clamp_axis(delta.x(), -minimum_x,
                static_cast<double>(image_.width() - 1) - maximum_x));
            delta.setY(clamp_axis(delta.y(), -minimum_y,
                static_cast<double>(image_.height() - 1) - maximum_y));
        }
        if (!qFuzzyIsNull(delta.x()) || !qFuzzyIsNull(delta.y())) {
            for (QPointF& point : overlay.points) point += delta;
            drag_last_image_point_ += delta;
            emit overlayMoved(overlay.source_index, delta, false);
            update();
        }
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
    } else if (tool_ == CanvasTool::None) {
        int overlay_index = -1;
        int point_index = -1;
        const bool over_handle = findEditableHandle(
            event->position(), overlay_index, point_index);
        const bool over_body = over_handle || findEditableOverlayBody(event->position()) >= 0;
        setCursor(over_body ? Qt::SizeAllCursor : Qt::ArrowCursor);
    }
}

void ImageCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && tool_ != CanvasTool::None) {
        const CanvasTool cancelled = tool_;
        setTool(CanvasTool::None);
        emit toolCancelled(cancelled);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ImageCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && camera_roi_dragging_ &&
        tool_ == CanvasTool::CameraRoi) {
        camera_roi_dragging_ = false;
        if (pending_points_.size() >= 2) {
            const QRectF roi(pending_points_[0], pending_points_[1]);
            const QVector<QPointF> points = pending_points_;
            pending_points_.clear();
            hover_image_point_valid_ = false;
            if (roi.normalized().width() >= 2.0 && roi.normalized().height() >= 2.0) {
                emit pointsCommitted(CanvasTool::CameraRoi, points);
            }
        }
        update();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && dragged_overlay_index_ >= 0 &&
        dragged_overlay_index_ < overlays_.size()) {
        const CanvasOverlay& overlay = overlays_[dragged_overlay_index_];
        if (dragging_overlay_body_) {
            emit overlayMoved(overlay.source_index, {}, true);
        } else if (dragged_point_index_ >= 0 && dragged_point_index_ < overlay.points.size()) {
            emit overlayPointMoved(overlay.source_index, dragged_point_index_,
                overlay.points[dragged_point_index_], true);
        }
        dragged_overlay_index_ = -1;
        dragged_point_index_ = -1;
        dragging_overlay_body_ = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
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

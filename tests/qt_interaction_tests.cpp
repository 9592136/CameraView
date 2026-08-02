#include "qt/ImageCanvas.h"
#include "qt/ai/YoloWorkspaceWidget.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QListWidget>

#include <cmath>
#include <iostream>

namespace {

int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
}

bool near(double lhs, double rhs, double tolerance = 0.01)
{
    return std::abs(lhs - rhs) <= tolerance;
}

QJsonArray point(double x, double y)
{
    return {x, y};
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    ImageCanvas canvas;
    canvas.resize(800, 600);
    if (canvas.focusOnImageRect(QRectF(10.0, 10.0, 20.0, 20.0))) {
        return fail("ImageCanvas accepted a focus request without an image.");
    }
    canvas.setImage(QImage(1000, 500, QImage::Format_RGB32));
    if (!canvas.focusOnImageRect(QRectF(100.0, 100.0, 200.0, 100.0))) {
        return fail("ImageCanvas rejected a valid image target.");
    }
    const QPointF focused_center = canvas.viewportCenterInImage();
    if (canvas.zoom() <= 1.0 || !near(focused_center.x(), 200.0) || !near(focused_center.y(), 150.0)) {
        return fail("ImageCanvas did not zoom and center the requested target.");
    }
    if (canvas.focusOnImageRect(QRectF(1200.0, 600.0, 20.0, 20.0))) {
        return fail("ImageCanvas accepted a target outside the image.");
    }

    YoloWorkspaceWidget workspace;
    workspace.setCurrentImage(QImage(1000, 500, QImage::Format_RGB32), QStringLiteral("test"));
    QRectF requested_bounds;
    int request_count = 0;
    QObject::connect(&workspace, &YoloWorkspaceWidget::focusRequested,
        [&requested_bounds, &request_count](const QRectF& bounds) {
            requested_bounds = bounds;
            ++request_count;
        });

    QJsonArray predictions;
    predictions.append(QJsonObject{
        {QStringLiteral("class_id"), 0},
        {QStringLiteral("label"), QStringLiteral("cell-box")},
        {QStringLiteral("confidence"), 0.9},
        {QStringLiteral("box"), QJsonArray{0.1, 0.2, 0.3, 0.4}},
    });
    predictions.append(QJsonObject{
        {QStringLiteral("class_id"), 1},
        {QStringLiteral("label"), QStringLiteral("cell-mask")},
        {QStringLiteral("confidence"), 0.8},
        {QStringLiteral("polygon"), QJsonArray{point(0.4, 0.2), point(0.6, 0.2), point(0.5, 0.5)}},
    });
    predictions.append(QJsonObject{
        {QStringLiteral("class_id"), 2},
        {QStringLiteral("label"), QStringLiteral("classification-only")},
        {QStringLiteral("confidence"), 0.7},
    });
    const QJsonObject inference{
        {QStringLiteral("predictions"), predictions},
        {QStringLiteral("elapsed_ms"), 12.5},
    };
    if (!QMetaObject::invokeMethod(&workspace, "renderInference", Qt::DirectConnection,
            Q_ARG(QJsonObject, inference))) {
        return fail("Could not render the synthetic inference result.");
    }

    auto* list = workspace.findChild<QListWidget*>(QStringLiteral("YoloResultList"));
    if (!list || list->count() != 4) {
        return fail("YOLO result rows were not created as expected.");
    }
    emit list->itemDoubleClicked(list->item(0));
    if (request_count != 1 || !near(requested_bounds.left(), 100.0) ||
        !near(requested_bounds.top(), 100.0) || !near(requested_bounds.width(), 200.0) ||
        !near(requested_bounds.height(), 100.0)) {
        return fail("Detection result double-click did not request the correct image target.");
    }
    emit list->itemDoubleClicked(list->item(1));
    if (request_count != 2 || !near(requested_bounds.left(), 400.0) ||
        !near(requested_bounds.top(), 100.0) || !near(requested_bounds.width(), 200.0) ||
        !near(requested_bounds.height(), 150.0)) {
        return fail("Segmentation result double-click did not request the polygon bounds.");
    }
    emit list->itemDoubleClicked(list->item(2));
    emit list->itemDoubleClicked(list->item(3));
    if (request_count != 2) {
        return fail("Rows without geometry unexpectedly requested image focus.");
    }

    return 0;
}

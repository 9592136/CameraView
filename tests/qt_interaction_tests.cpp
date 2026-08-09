#include "qt/CalibrationDialog.h"
#include "qt/ImageCanvas.h"
#include "qt/ObjectiveCalibrationSettings.h"
#include "qt/ai/YoloWorkspaceWidget.h"

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>

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

    CalibrationDialog calibration_dialog(
        200.0, 100.0, MeasurementUnit::Micrometers);
    auto* calibration_length = calibration_dialog.findChild<QDoubleSpinBox*>(
        QStringLiteral("CalibrationDialogLength"));
    auto* calibration_unit = calibration_dialog.findChild<QComboBox*>(
        QStringLiteral("CalibrationDialogUnit"));
    auto* calibration_preview = calibration_dialog.findChild<QLabel*>(
        QStringLiteral("CalibrationScalePreview"));
    if (!calibration_length || !calibration_unit || !calibration_preview ||
        !near(calibration_dialog.profile().MicronsPerPixel(), 0.5) ||
        !calibration_preview->text().contains(QStringLiteral("0.5"))) {
        return fail("Calibration confirmation dialog did not preview a micrometer scale.");
    }
    calibration_length->setValue(0.1);
    calibration_unit->setCurrentIndex(1);
    if (calibration_dialog.unit() != MeasurementUnit::Millimeters ||
        !near(calibration_dialog.profile().MicronsPerPixel(), 0.5) ||
        !calibration_preview->text().contains(QStringLiteral("0.5"))) {
        return fail("Calibration confirmation dialog did not convert millimeters correctly.");
    }

    QTemporaryDir calibration_settings_directory;
    if (!calibration_settings_directory.isValid()) {
        return fail("Could not create a temporary directory for objective calibration settings.");
    }
    const QString calibration_settings_path =
        calibration_settings_directory.filePath(QStringLiteral("calibration.ini"));
    {
        QSettings settings(calibration_settings_path, QSettings::IniFormat);
        ObjectiveCalibrationState saved = ObjectiveCalibrationSettings::Defaults();
        saved.selected_index = 2;
        saved.calibrations[0] = CalibrationProfile::FromMicronsPerPixel(1.25);
        saved.calibrations[2] = CalibrationProfile::FromMicronsPerPixel(0.25);
        ObjectiveCalibrationSettings::Save(settings, saved);
    }
    {
        QSettings settings(calibration_settings_path, QSettings::IniFormat);
        const ObjectiveCalibrationState restored = ObjectiveCalibrationSettings::Load(settings);
        if (restored.labels.size() < 6 || restored.calibrations.size() != restored.labels.size() ||
            restored.selected_index != 2 || restored.labels[2] != L"20x" ||
            !near(restored.calibrations[0].MicronsPerPixel(), 1.25) ||
            !near(restored.calibrations[2].MicronsPerPixel(), 0.25)) {
            return fail("Objective-specific calibration settings were not remembered correctly.");
        }
    }

    ImageCanvas canvas;
    canvas.resize(800, 600);
    if (canvas.focusOnImageRect(QRectF(10.0, 10.0, 20.0, 20.0))) {
        return fail("ImageCanvas accepted a focus request without an image.");
    }
    const QImage shared_canvas_image(1000, 500, QImage::Format_RGB32);
    canvas.setImage(shared_canvas_image);
    if (canvas.imageCacheKey() != shared_canvas_image.cacheKey() || canvas.hasGrayscaleCache()) {
        return fail("ImageCanvas copied or grayscale-converted an image on the normal preview path.");
    }
    canvas.setLivePreviewOverlay(QImage(320, 240, QImage::Format_RGB32));
    if (!canvas.hasLivePreviewOverlay()) {
        return fail("ImageCanvas did not retain the live-camera inset used by live stitching.");
    }
    canvas.setLivePreviewOverlay({});
    if (canvas.hasLivePreviewOverlay()) {
        return fail("ImageCanvas did not clear the live-camera inset.");
    }
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
    canvas.fitToView();
    CanvasTool announced_tool = CanvasTool::None;
    int tool_change_notifications = 0;
    QObject::connect(&canvas, &ImageCanvas::toolChanged,
        [&announced_tool, &tool_change_notifications](CanvasTool tool) {
            announced_tool = tool;
            ++tool_change_notifications;
        });
    canvas.setTool(CanvasTool::ProfileLine);
    if (announced_tool != CanvasTool::ProfileLine || tool_change_notifications != 1) {
        return fail("ImageCanvas did not publish its active measurement tool for button-state synchronization.");
    }
    CanvasTool committed_tool = CanvasTool::None;
    QVector<QPointF> committed_points;
    QObject::connect(&canvas, &ImageCanvas::pointsCommitted,
        [&committed_tool, &committed_points](CanvasTool tool, const QVector<QPointF>& points) {
            committed_tool = tool;
            committed_points = points;
        });
    QMouseEvent first_profile_point(
        QEvent::MouseButtonPress, QPointF(80.0, 180.0), QPointF(80.0, 180.0),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent second_profile_point(
        QEvent::MouseButtonPress, QPointF(720.0, 420.0), QPointF(720.0, 420.0),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &first_profile_point);
    QApplication::sendEvent(&canvas, &second_profile_point);
    if (committed_tool != CanvasTool::ProfileLine || committed_points.size() != 2 ||
        !near(committed_points.at(0).x(), 100.0) || !near(committed_points.at(0).y(), 100.0) ||
        !near(committed_points.at(1).x(), 900.0) || !near(committed_points.at(1).y(), 400.0)) {
        return fail("Profile-line canvas interaction did not commit two image-space endpoints.");
    }

    QImage edge_image(100, 100, QImage::Format_RGB32);
    edge_image.fill(Qt::black);
    for (int y = 0; y < edge_image.height(); ++y) {
        for (int x = 50; x < edge_image.width(); ++x) edge_image.setPixelColor(x, y, Qt::white);
    }
    canvas.resize(500, 500);
    canvas.setImage(edge_image);
    canvas.fitToView();
    canvas.setEdgeSnappingEnabled(true);
    if (!canvas.hasGrayscaleCache()) {
        return fail("ImageCanvas did not build the grayscale cache when edge snapping was enabled.");
    }
    canvas.setEdgeSnapRadius(10);
    bool snap_reported = false;
    bool snap_succeeded = false;
    QObject::connect(&canvas, &ImageCanvas::edgeSnapEvaluated,
        [&snap_reported, &snap_succeeded](bool snapped, const QPointF&, const QPointF&, double) {
            snap_reported = true;
            snap_succeeded = snapped;
        });
    committed_tool = CanvasTool::None;
    committed_points.clear();
    canvas.setTool(CanvasTool::Point);
    QMouseEvent edge_point(
        QEvent::MouseButtonPress, QPointF(225.0, 250.0), QPointF(225.0, 250.0),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &edge_point);
    if (!snap_reported || !snap_succeeded || committed_tool != CanvasTool::Point ||
        committed_points.size() != 1 || committed_points[0].x() < 48.0 || committed_points[0].x() > 51.0) {
        return fail("Automatic edge snapping did not move a point measurement to the image edge.");
    }

    canvas.setEdgeSnappingEnabled(false);
    if (canvas.hasGrayscaleCache()) {
        return fail("ImageCanvas retained the expensive grayscale cache after edge snapping was disabled.");
    }
    committed_tool = CanvasTool::None;
    committed_points.clear();
    canvas.setTool(CanvasTool::Circle);
    QMouseEvent circle_center(QEvent::MouseButtonPress, QPointF(100.0, 100.0), QPointF(100.0, 100.0),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent circle_edge(QEvent::MouseButtonPress, QPointF(200.0, 100.0), QPointF(200.0, 100.0),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &circle_center);
    QApplication::sendEvent(&canvas, &circle_edge);
    if (committed_tool != CanvasTool::Circle || committed_points.size() != 2) {
        return fail("Circle canvas interaction did not commit center and edge points.");
    }

    committed_tool = CanvasTool::None;
    committed_points.clear();
    canvas.setTool(CanvasTool::SmartCountSample);
    QMouseEvent sample_first(QEvent::MouseButtonPress, QPointF(125.0, 125.0), QPointF(125.0, 125.0),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent sample_second(QEvent::MouseButtonPress, QPointF(225.0, 225.0), QPointF(225.0, 225.0),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &sample_first);
    QApplication::sendEvent(&canvas, &sample_second);
    if (committed_tool != CanvasTool::SmartCountSample || committed_points.size() != 2 ||
        !near(committed_points[0].x(), 25.0) || !near(committed_points[1].x(), 45.0)) {
        return fail("Smart-count sample canvas interaction did not commit its rectangular bounds.");
    }

    committed_tool = CanvasTool::None;
    committed_points.clear();
    canvas.setTool(CanvasTool::Polyline);
    QMouseEvent line_first(QEvent::MouseButtonPress, QPointF(100.0, 100.0), QPointF(100.0, 100.0),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent line_second(QEvent::MouseButtonPress, QPointF(200.0, 150.0), QPointF(200.0, 150.0),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent line_third(QEvent::MouseButtonPress, QPointF(300.0, 200.0), QPointF(300.0, 200.0),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent line_finish(QEvent::MouseButtonDblClick, QPointF(300.0, 200.0), QPointF(300.0, 200.0),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &line_first);
    QApplication::sendEvent(&canvas, &line_second);
    QApplication::sendEvent(&canvas, &line_third);
    QApplication::sendEvent(&canvas, &line_finish);
    if (committed_tool != CanvasTool::Polyline || committed_points.size() != 3) {
        return fail("Polyline canvas interaction did not commit its variable point list.");
    }

    int moved_source = -1;
    int moved_point = -1;
    QPointF moved_position;
    bool move_finished = false;
    QObject::connect(&canvas, &ImageCanvas::overlayPointMoved,
        [&moved_source, &moved_point, &moved_position, &move_finished](
            int source, int pointIndex, const QPointF& position, bool finished) {
            moved_source = source;
            moved_point = pointIndex;
            moved_position = position;
            move_finished = finished;
        });
    canvas.setTool(CanvasTool::None);
    canvas.setOverlays({CanvasOverlay{CanvasTool::Length, {{20.0, 20.0}, {40.0, 20.0}},
        QStringLiteral("editable"), Qt::cyan, true, true, 3}});
    QMouseEvent drag_press(QEvent::MouseButtonPress, QPointF(100.0, 100.0), QPointF(100.0, 100.0),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent drag_move(QEvent::MouseMove, QPointF(150.0, 125.0), QPointF(150.0, 125.0),
        Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent drag_release(QEvent::MouseButtonRelease, QPointF(150.0, 125.0), QPointF(150.0, 125.0),
        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &drag_press);
    QApplication::sendEvent(&canvas, &drag_move);
    QApplication::sendEvent(&canvas, &drag_release);
    if (moved_source != 3 || moved_point != 0 || !near(moved_position.x(), 30.0) ||
        !near(moved_position.y(), 25.0) || !move_finished) {
        return fail("Editable measurement overlay did not report a completed handle drag.");
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

    QTemporaryDir dataset_directory;
    YoloDatasetProject dataset;
    QString dataset_error;
    if (!dataset_directory.isValid() || !YoloDatasetProject::create(
            dataset_directory.filePath(QStringLiteral("annotated-cells")),
            QStringLiteral("Annotated cells"), YoloTask::Detection,
            {QStringLiteral("nucleus")}, &dataset, &dataset_error) ||
        !workspace.loadDatasetProject(dataset.rootDirectory(), &dataset_error)) {
        std::cerr << "Dataset UI setup failed: "
                  << dataset_error.toStdString()
                  << " (temporary directory: "
                  << dataset_directory.path().toStdString() << ")\n";
        return fail("Could not create and load a dataset project for the UI test.");
    }
    auto* tabs = workspace.findChild<QTabWidget*>(QStringLiteral("YoloWorkspaceTabs"));
    auto* annotate_button = workspace.findChild<QPushButton*>(QStringLiteral("DatasetAnnotateButton"));
    auto* save_button = workspace.findChild<QPushButton*>(QStringLiteral("DatasetSaveSampleButton"));
    auto* use_training_button = workspace.findChild<QPushButton*>(QStringLiteral("DatasetUseTrainingButton"));
    auto* annotation_list = workspace.findChild<QListWidget*>(QStringLiteral("DatasetAnnotationList"));
    auto* sample_list = workspace.findChild<QListWidget*>(QStringLiteral("DatasetSampleList"));
    auto* training_dataset_edit = workspace.findChild<QLineEdit*>(QStringLiteral("TrainingDatasetEdit"));
    auto* training_task_combo = workspace.findChild<QComboBox*>(QStringLiteral("TrainingTaskCombo"));
    if (!tabs || !annotate_button || !save_button || !use_training_button ||
        !annotation_list || !sample_list || !training_dataset_edit || !training_task_combo) {
        return fail("Dataset editor controls are missing.");
    }
    tabs->setCurrentIndex(1);
    CanvasTool requested_tool = CanvasTool::None;
    QObject::connect(&workspace, &YoloWorkspaceWidget::annotationToolRequested,
        [&requested_tool](CanvasTool tool, const QString&) { requested_tool = tool; });
    annotate_button->click();
    if (requested_tool != CanvasTool::Rectangle) {
        return fail("Detection dataset did not request the rectangle canvas tool.");
    }
    workspace.acceptCanvasAnnotation(
        CanvasTool::Rectangle, {QPointF(25.0, 30.0), QPointF(125.0, 90.0)});
    if (annotation_list->count() != 1) {
        return fail("Canvas annotation was not added to the dataset editor.");
    }
    save_button->click();
    if (sample_list->count() != 1) {
        return fail("The annotated current image was not saved as a dataset sample.");
    }
    YoloDatasetProject saved_dataset;
    if (!saved_dataset.load(dataset.rootDirectory(), &dataset_error) ||
        saved_dataset.samples().size() != 1 ||
        saved_dataset.samples().first().annotations.size() != 1) {
        return fail("The dataset editor did not persist a reloadable sample.");
    }
    use_training_button->click();
    if (tabs->currentIndex() != 2 ||
        training_dataset_edit->text() != saved_dataset.trainingDataPath() ||
        training_task_combo->currentData().toString() != QStringLiteral("detect")) {
        return fail("Use-for-training did not configure the training page.");
    }
    tabs->setCurrentIndex(1);
    QString requested_image_path;
    QObject::connect(&workspace, &YoloWorkspaceWidget::imageOpenRequested,
        [&requested_image_path](const QString& path) { requested_image_path = path; });
    sample_list->setCurrentRow(0);
    emit sample_list->itemDoubleClicked(sample_list->item(0));
    if (requested_image_path != saved_dataset.absoluteImagePath(saved_dataset.samples().first())) {
        return fail("Dataset sample double-click did not request the saved image.");
    }
    workspace.cancelPendingDatasetImageOpen();
    workspace.setCurrentImage(
        QImage(1000, 500, QImage::Format_RGB32), QStringLiteral("test"), QStringLiteral("different-image"));
    if (annotation_list->count() != 0) {
        return fail("Annotations leaked to a different image with the same name and size.");
    }

    return 0;
}

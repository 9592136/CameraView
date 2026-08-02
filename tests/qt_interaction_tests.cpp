#include "qt/ImageCanvas.h"
#include "qt/ai/YoloWorkspaceWidget.h"

#include <QApplication>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
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

    QTemporaryDir dataset_directory;
    YoloDatasetProject dataset;
    QString dataset_error;
    if (!dataset_directory.isValid() || !YoloDatasetProject::create(
            dataset_directory.filePath(QStringLiteral("annotated-cells")),
            QStringLiteral("Annotated cells"), YoloTask::Detection,
            {QStringLiteral("nucleus")}, &dataset, &dataset_error) ||
        !workspace.loadDatasetProject(dataset.rootDirectory(), &dataset_error)) {
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

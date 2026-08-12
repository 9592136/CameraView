#include "CameraMainWindow.h"
#include "CameraViewTheme.h"
#include "NumericSlider.h"
#include "PointCloudDialog.h"
#include "ai/YoloModelRegistry.h"
#include "imaging/ProcessingParameterRules.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QComboBox>
#include <QColorDialog>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDebug>
#include <QElapsedTimer>
#include <QDoubleSpinBox>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QToolButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QVariant>

namespace {

int importYoloManifest(const QString& manifestPath)
{
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical().noquote() << QStringLiteral("Cannot open YOLO manifest: %1").arg(file.errorString());
        return 2;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        qCritical().noquote() << QStringLiteral("Invalid YOLO manifest: %1").arg(parseError.errorString());
        return 2;
    }

    YoloModelRegistry registry;
    QString error;
    if (!registry.load(&error)) {
        qCritical().noquote() << QStringLiteral("Cannot load YOLO registry: %1").arg(error);
        return 2;
    }

    const QJsonArray entries = document.object().value(QStringLiteral("models")).toArray();
    if (entries.isEmpty()) {
        qCritical() << "YOLO manifest contains no models.";
        return 2;
    }
    for (const QJsonValue& value : entries) {
        const QJsonObject entry = value.toObject();
        const QString path = QFileInfo(entry.value(QStringLiteral("path")).toString()).absoluteFilePath();
        const QString name = entry.value(QStringLiteral("name")).toString();
        const YoloTask task = yoloTaskFromKey(entry.value(QStringLiteral("task")).toString());
        const QJsonObject metrics = entry.value(QStringLiteral("metrics")).toObject();
        QStringList classes;
        for (const QJsonValue& classValue : entry.value(QStringLiteral("classes")).toArray()) {
            classes.push_back(classValue.toString());
        }

        const YoloModelRecord* existing = nullptr;
        for (const YoloModelRecord& model : registry.models()) {
            if (model.name == name && model.task == task) {
                existing = &model;
                break;
            }
        }

        QString id;
        if (existing) {
            id = existing->id;
            if (!registry.updateMetadata(id, task, classes, metrics, &error)) {
                qCritical().noquote() << QStringLiteral("Cannot update %1: %2").arg(name, error);
                return 2;
            }
            qInfo().noquote() << QStringLiteral("Updated YOLO model: %1").arg(name);
        } else {
            YoloModelRecord imported;
            if (!registry.registerTrainingArtifact(path, task, name, metrics, &imported, &error)
                || !registry.updateMetadata(imported.id, task, classes, metrics, &error)) {
                qCritical().noquote() << QStringLiteral("Cannot import %1: %2").arg(name, error);
                return 2;
            }
            id = imported.id;
            qInfo().noquote() << QStringLiteral("Imported YOLO model: %1").arg(name);
        }
        if (entry.value(QStringLiteral("active")).toBool() && !registry.setActive(id, &error)) {
            qCritical().noquote() << QStringLiteral("Cannot activate %1: %2").arg(name, error);
            return 2;
        }
    }
    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("CameraView"));
    application.setApplicationDisplayName(QStringLiteral("CameraView · Qt"));
    application.setOrganizationName(QStringLiteral("CameraView"));
    const QIcon application_icon(QStringLiteral(":/icons/CameraView.png"));
    if (application_icon.isNull()) {
        qCritical() << "CameraView application icon resource is missing.";
        return 4;
    }
    application.setWindowIcon(application_icon);
    applyCameraViewTheme(application);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("CameraView Qt industrial camera application"));
    parser.addHelpOption();
    const QCommandLineOption smoke_test(
        QStringLiteral("smoke-test"),
        QStringLiteral("Initialize the Qt application and exit automatically."));
    const QCommandLineOption import_yolo_manifest(
        QStringLiteral("import-yolo-manifest"),
        QStringLiteral("Import trained YOLO models from a JSON manifest and exit."),
        QStringLiteral("path"));
    const QCommandLineOption ui_snapshot(
        QStringLiteral("ui-snapshot"),
        QStringLiteral("Render the main window to an image and exit."),
        QStringLiteral("path"));
    const QCommandLineOption workspace_tab(
        QStringLiteral("workspace-tab"),
        QStringLiteral("Select the workspace tab by zero-based index."),
        QStringLiteral("index"));
    const QCommandLineOption focus_widget(
        QStringLiteral("focus-widget"),
        QStringLiteral("Scroll a named widget into view before rendering a UI snapshot."),
        QStringLiteral("object-name"));
    const QCommandLineOption verify_measurement_toolbar(
        QStringLiteral("verify-measurement-toolbar"),
        QStringLiteral("Verify that every measurement function is represented in the toolbar."));
    const QCommandLineOption verify_measurement_overlay_editor(
        QStringLiteral("verify-measurement-overlay-editor"),
        QStringLiteral("Verify measurement overlay editing, dragging, and color controls."));
    const QCommandLineOption verify_preview_pipeline(
        QStringLiteral("verify-preview-pipeline"),
        QStringLiteral("Verify the zero-copy neutral camera preview path."));
    const QCommandLineOption verify_fluorescence_workflow(
        QStringLiteral("verify-fluorescence-workflow"),
        QStringLiteral("Verify microscopy fluorescence display and exposure controls."));
    const QCommandLineOption verify_point_cloud_workflow(
        QStringLiteral("verify-point-cloud-workflow"),
        QStringLiteral("Verify the 3D point-cloud workbench and controls."));
    const QCommandLineOption verify_numeric_sliders(
        QStringLiteral("verify-numeric-sliders"),
        QStringLiteral("Verify drag-based numeric setting controls and value readouts."));
    const QCommandLineOption verify_stitch_workflow(
        QStringLiteral("verify-stitch-workflow"),
        QStringLiteral("Verify complete Qt stitching workflow controls and options."));
    const QCommandLineOption verify_stitch_execution(
        QStringLiteral("verify-stitch-execution"),
        QStringLiteral("Run an end-to-end Qt stitching workflow with synthetic overlapping images."));
    const QCommandLineOption live_camera_report(
        QStringLiteral("live-camera-report"),
        QStringLiteral("Open the first camera, exercise the live Qt preview, and write a JSON report."),
        QStringLiteral("path"));
    parser.addOption(smoke_test);
    parser.addOption(import_yolo_manifest);
    parser.addOption(ui_snapshot);
    parser.addOption(workspace_tab);
    parser.addOption(focus_widget);
    parser.addOption(verify_measurement_toolbar);
    parser.addOption(verify_measurement_overlay_editor);
    parser.addOption(verify_preview_pipeline);
    parser.addOption(verify_fluorescence_workflow);
    parser.addOption(verify_point_cloud_workflow);
    parser.addOption(verify_numeric_sliders);
    parser.addOption(verify_stitch_workflow);
    parser.addOption(verify_stitch_execution);
    parser.addOption(live_camera_report);
    parser.process(application);

    if (parser.isSet(import_yolo_manifest)) {
        return importYoloManifest(parser.value(import_yolo_manifest));
    }

    CameraMainWindow window;
    if (parser.isSet(verify_point_cloud_workflow)) {
        QAction* action = window.findChild<QAction*>(QStringLiteral("PointCloudWorkspaceAction"));
        if (!action) {
            qCritical() << "3D point-cloud workbench action is missing.";
            return 12;
        }
        action->trigger();
        application.processEvents();
        auto* dialog = window.findChild<PointCloudDialog*>(QStringLiteral("PointCloudDialog"));
        const QStringList required_controls{
            QStringLiteral("PointCloudView"),
            QStringLiteral("PointCloudToolTabs"),
            QStringLiteral("PointCloudOpenButton"),
            QStringLiteral("PointCloudExportButton"),
            QStringLiteral("PointCloudUnitCombo"),
            QStringLiteral("PointCloudColorCombo"),
            QStringLiteral("PointCloudPointSize"),
            QStringLiteral("PointCloudAxesCheck"),
            QStringLiteral("PointCloudVoxelApplyButton"),
            QStringLiteral("PointCloudOutlierApplyButton"),
            QStringLiteral("PointCloudCropApplyButton"),
            QStringLiteral("PointCloudBeginInteractiveCropButton"),
            QStringLiteral("PointCloudKeepSelectionButton"),
            QStringLiteral("PointCloudRemoveSelectionButton"),
            QStringLiteral("PointCloudFitPlaneButton"),
            QStringLiteral("PointCloudLevelButton"),
            QStringLiteral("PointCloudUndoButton"),
            QStringLiteral("PointCloudRestoreButton"),
            QStringLiteral("PointCloudPointMeasureButton"),
            QStringLiteral("PointCloudDistanceMeasureButton"),
            QStringLiteral("PointCloudHeightMeasureButton"),
            QStringLiteral("PointCloudAngleMeasureButton"),
            QStringLiteral("PointCloudPlaneMeasureButton"),
            QStringLiteral("PointCloudMeasurementList"),
            QStringLiteral("PointCloudExportMeasurementsButton")};
        if (!dialog) {
            qCritical() << "3D point-cloud workbench did not open.";
            return 12;
        }
        for (const QString& name : required_controls) {
            if (!dialog->findChild<QWidget*>(name)) {
                qCritical().noquote() << QStringLiteral(
                    "3D point-cloud control is missing: %1").arg(name);
                return 12;
            }
        }
        return 0;
    }
    if (parser.isSet(verify_preview_pipeline)) {
        QImage frame(3072, 2048, QImage::Format_BGR888);
        frame.fill(Qt::black);
        QElapsedTimer timer;
        timer.start();
        for (quint64 sequence = 1; sequence <= 120; ++sequence) {
            if (!QMetaObject::invokeMethod(&window, "onCameraFrame", Qt::DirectConnection,
                    Q_ARG(QImage, frame), Q_ARG(quint64, sequence), Q_ARG(quint32, 0))) {
                qCritical() << "Could not inject a synthetic camera frame.";
                return 6;
            }
        }
        ImageCanvas* canvas = window.findChild<ImageCanvas*>(QStringLiteral("ImageCanvas"));
        if (!canvas || !canvas->property("directCameraPreview").toBool() ||
            canvas->property("cameraPreviewSequence").toULongLong() != 120 ||
            timer.elapsed() > 2000) {
            qCritical() << "Neutral camera preview did not use the direct high-throughput path."
                        << "elapsed_ms=" << timer.elapsed();
            return 6;
        }
        return 0;
    }
    if (parser.isSet(verify_measurement_overlay_editor)) {
        const QStringList required_controls{
            QStringLiteral("MeasurementResultList"),
            QStringLiteral("CalibrationObjective"),
            QStringLiteral("CalibrationObjectiveAddButton"),
            QStringLiteral("CalibrationObjectiveEditButton"),
            QStringLiteral("CalibrationObjectiveDeleteButton"),
            QStringLiteral("MeasurementSelectionButton"),
            QStringLiteral("MeasurementRenameToolButton"),
            QStringLiteral("MeasurementColorToolButton"),
            QStringLiteral("MeasurementResetColorToolButton"),
            QStringLiteral("MeasurementDeleteToolButton"),
            QStringLiteral("MeasurementClearToolButton"),
            QStringLiteral("MeasurementExportToolButton")};
        for (const QString& object_name : required_controls) {
            if (!window.findChild<QWidget*>(object_name)) {
                qCritical().noquote() << QStringLiteral(
                    "Measurement overlay editor control is missing: %1").arg(object_name);
                return 11;
            }
        }
        auto* canvas = window.findChild<ImageCanvas*>(QStringLiteral("ImageCanvas"));
        auto* list = window.findChild<QListWidget*>(QStringLiteral("MeasurementResultList"));
        auto* toolbar = window.findChild<QToolBar*>(QStringLiteral("MeasurementToolbar"));
        auto* color = window.findChild<QToolButton*>(
            QStringLiteral("MeasurementColorToolButton"));
        auto* reset_color = window.findChild<QToolButton*>(
            QStringLiteral("MeasurementResetColorToolButton"));
        auto* selection = window.findChild<QToolButton*>(
            QStringLiteral("MeasurementSelectionButton"));
        QImage frame(100, 100, QImage::Format_RGB32);
        frame.fill(Qt::black);
        const QVector<QPointF> measurement_points{{20.0, 20.0}, {40.0, 20.0}};
        QAction* length_action = nullptr;
        if (toolbar) {
            for (QAction* action : toolbar->actions()) {
                if (action->data().toInt() == static_cast<int>(CanvasTool::Length)) {
                    length_action = action;
                    break;
                }
            }
        }
        if (!canvas || !list || !toolbar || !length_action || !selection ||
            !color || !reset_color ||
            !color->isEnabled() ||
            !reset_color->isEnabled() ||
            !QMetaObject::invokeMethod(&window, "onCameraFrame", Qt::DirectConnection,
                Q_ARG(QImage, frame), Q_ARG(quint64, 1), Q_ARG(quint32, 0)) ||
            (length_action->trigger(), canvas->tool() != CanvasTool::Length) ||
            !QMetaObject::invokeMethod(&window, "onCanvasPoints", Qt::DirectConnection,
                Q_ARG(CanvasTool, CanvasTool::Length),
                Q_ARG(QVector<QPointF>, measurement_points)) ||
            list->count() != 1 || list->currentRow() != 0 ||
            canvas->tool() != CanvasTool::Length || !length_action->isChecked() ||
            !color->isEnabled() ||
            !color->toolTip().contains(QLatin1Char('#'))) {
            qCritical() << "Measurement overlay editor did not bind to the selected measurement.";
            return 11;
        }
        const QColor custom_color(12, 34, 56);
        QTimer::singleShot(0, &window, [custom_color] {
            auto* dialog = qobject_cast<QColorDialog*>(QApplication::activeModalWidget());
            if (!dialog) return;
            dialog->setCurrentColor(custom_color);
            dialog->accept();
        });
        color->click();
        if (canvas->overlays().isEmpty() || canvas->overlays().first().color != custom_color ||
            !color->toolTip().contains(QStringLiteral("#0C2238"))) {
            qCritical() << "Custom measurement overlay color did not reach the canvas.";
            return 11;
        }
        const QVector<QPointF> second_measurement_points{{30.0, 30.0}, {60.0, 30.0}};
        if (!QMetaObject::invokeMethod(&window, "onCanvasPoints", Qt::DirectConnection,
                Q_ARG(CanvasTool, CanvasTool::Length),
                Q_ARG(QVector<QPointF>, second_measurement_points)) ||
            list->count() != 2 || list->currentRow() != 1 ||
            canvas->tool() != CanvasTool::Length || !length_action->isChecked() ||
            canvas->overlays().size() != 2 ||
            canvas->overlays().at(0).color != custom_color ||
            canvas->overlays().at(1).color != custom_color) {
            qCritical() << "Global color or continuous measurement behavior was not preserved.";
            return 11;
        }
        reset_color->click();
        if (canvas->overlays().size() != 2 ||
            canvas->overlays().at(0).color != QColor(76, 201, 240) ||
            canvas->overlays().at(1).color != QColor(76, 201, 240)) {
            qCritical() << "Global measurement color was not restored for all overlays.";
            return 11;
        }
        selection->click();
        if (canvas->tool() != CanvasTool::None || !selection->isChecked() ||
            length_action->isChecked()) {
            qCritical() << "Measurement selection mode did not replace the active drawing tool.";
            return 11;
        }
        return 0;
    }
    if (parser.isSet(verify_fluorescence_workflow)) {
        const QStringList required_controls{
            QStringLiteral("FluorescencePresetGroup"),
            QStringLiteral("FluorescencePresetList"),
            QStringLiteral("FluorescencePresetExposure"),
            QStringLiteral("FluorescencePresetColorButton"),
            QStringLiteral("FluorescencePresetAddButton"),
            QStringLiteral("FluorescencePresetSaveButton"),
            QStringLiteral("FluorescencePresetDeleteButton"),
            QStringLiteral("FluorescenceCaptureGroup"),
            QStringLiteral("FluorescenceCaptureStatus"),
            QStringLiteral("FluorescenceCaptureStartButton"),
            QStringLiteral("FluorescenceCaptureCurrentButton"),
            QStringLiteral("FluorescenceCaptureCancelButton"),
            QStringLiteral("FluorescenceDyeCombo"),
            QStringLiteral("FluorescenceAddChannelButton"),
            QStringLiteral("FluorescencePreviewCheck"),
            QStringLiteral("FluorescenceBlendCombo"),
            QStringLiteral("FluorescenceChannelList"),
            QStringLiteral("FluorescenceRemoveChannelButton"),
            QStringLiteral("FluorescenceIsolateChannelButton"),
            QStringLiteral("FluorescenceShowAllButton"),
            QStringLiteral("FluorescenceClearButton"),
            QStringLiteral("FluorescenceChannelVisible"),
            QStringLiteral("FluorescenceBlackLevel"),
            QStringLiteral("FluorescenceWhiteLevel"),
            QStringLiteral("FluorescenceApplyLevelsButton"),
            QStringLiteral("FluorescenceAutoLevelsButton"),
            QStringLiteral("FluorescenceStatisticsLabel")};
        for (const QString& object_name : required_controls) {
            if (!window.findChild<QWidget*>(object_name)) {
                qCritical().noquote() << QStringLiteral(
                    "Fluorescence workflow control is missing: %1").arg(object_name);
                return 9;
            }
        }
        const auto* blend = window.findChild<QComboBox*>(
            QStringLiteral("FluorescenceBlendCombo"));
        const auto* black = window.findChild<NumericSlider*>(
            QStringLiteral("FluorescenceBlackLevelSlider"));
        const auto* white = window.findChild<NumericSlider*>(
            QStringLiteral("FluorescenceWhiteLevelSlider"));
        const auto* presets = window.findChild<QListWidget*>(
            QStringLiteral("FluorescencePresetList"));
        const auto* preset_exposure = window.findChild<QDoubleSpinBox*>(
            QStringLiteral("FluorescencePresetExposure"));
        const auto* capture_current = window.findChild<QPushButton*>(
            QStringLiteral("FluorescenceCaptureCurrentButton"));
        if (!blend || blend->count() != 3 || blend->currentData().toInt() !=
                static_cast<int>(FluorescenceBlendMode::Screen) ||
            !presets || !preset_exposure ||
            preset_exposure->value() <= 0.0 || !capture_current || capture_current->isEnabled() ||
            !black || black->minimum() != 0 || black->maximum() != 254 ||
            !white || white->minimum() != 1 || white->maximum() != 255) {
            qCritical() << "Fluorescence workflow defaults are incomplete.";
            return 9;
        }
        return 0;
    }
    if (parser.isSet(verify_numeric_sliders)) {
        const QStringList slider_names{
            QStringLiteral("ImageFilterParameterSlider"),
            QStringLiteral("FluorescenceBlackLevelSlider"),
            QStringLiteral("FluorescenceWhiteLevelSlider"),
            QStringLiteral("LiveStitchIntervalSlider"),
            QStringLiteral("StitchOverlapSlider"),
            QStringLiteral("EdgeSnapRadiusSlider"),
            QStringLiteral("SmartCountSimilaritySlider"),
            QStringLiteral("SmartCountScaleToleranceSlider"),
            QStringLiteral("YoloConfidenceSlider"),
            QStringLiteral("YoloIouSlider")};
        for (const QString& slider_name : slider_names) {
            auto* control = window.findChild<NumericSlider*>(slider_name);
            if (!control || !control->slider() || !control->valueLabel() ||
                control->valueLabel()->text().isEmpty()) {
                qCritical().noquote() << QStringLiteral(
                    "Numeric slider is missing or has no value readout: %1").arg(slider_name);
                return 10;
            }
        }
        auto* black = window.findChild<NumericSlider*>(
            QStringLiteral("FluorescenceBlackLevelSlider"));
        auto* similarity = window.findChild<NumericSlider*>(
            QStringLiteral("SmartCountSimilaritySlider"));
        auto* confidence = window.findChild<NumericSlider*>(
            QStringLiteral("YoloConfidenceSlider"));
        black->setValue(42);
        similarity->setValue(0.83);
        confidence->setValue(0.37);
        if (black->integerValue() != 42 || black->valueLabel()->text() != QStringLiteral("42") ||
            qAbs(similarity->value() - 0.83) > 0.0001 ||
            similarity->valueLabel()->text() != QStringLiteral("0.83") ||
            qAbs(confidence->value() - 0.37) > 0.0001 ||
            confidence->valueLabel()->text() != QStringLiteral("0.37")) {
            qCritical() << "Numeric slider mapping or readout formatting is incorrect.";
            return 10;
        }
        return 0;
    }
    if (parser.isSet(verify_stitch_workflow)) {
        const QStringList required_controls{
            QStringLiteral("LiveStitchStartButton"), QStringLiteral("LiveStitchStopButton"),
            QStringLiteral("LiveStitchIntervalSpin"),
            QStringLiteral("StitchAddCurrentButton"), QStringLiteral("StitchImportFilesButton"),
            QStringLiteral("StitchImportDirectoryButton"), QStringLiteral("StitchMoveUpButton"),
            QStringLiteral("StitchMoveDownButton"), QStringLiteral("StitchDeleteButton"),
            QStringLiteral("StitchClearButton"), QStringLiteral("StitchBuildButton"),
            QStringLiteral("StitchCancelButton"), QStringLiteral("StitchRetryButton"),
            QStringLiteral("StitchSaveButton"), QStringLiteral("StitchTileList"),
            QStringLiteral("StitchProgress")};
        for (const QString& object_name : required_controls) {
            if (!window.findChild<QWidget*>(object_name)) {
                qCritical().noquote() << QStringLiteral("Stitch workflow control is missing: %1").arg(object_name);
                return 7;
            }
        }
        const auto* layout = window.findChild<QComboBox*>(QStringLiteral("StitchLayoutCombo"));
        const auto* registration = window.findChild<QComboBox*>(QStringLiteral("StitchRegistrationCombo"));
        const auto* transform = window.findChild<QComboBox*>(QStringLiteral("StitchTransformCombo"));
        const auto* blend = window.findChild<QComboBox*>(QStringLiteral("StitchBlendCombo"));
        const auto* overlap = window.findChild<NumericSlider*>(QStringLiteral("StitchOverlapSlider"));
        const auto* interval = window.findChild<NumericSlider*>(QStringLiteral("LiveStitchIntervalSlider"));
        const auto* retry = window.findChild<QPushButton*>(QStringLiteral("StitchRetryButton"));
        const auto* save = window.findChild<QPushButton*>(QStringLiteral("StitchSaveButton"));
        if (!layout || layout->count() != 2 || !registration || registration->count() != 5 ||
            !transform || transform->count() != 3 || !blend || blend->count() != 2 ||
            !overlap || overlap->minimum() != ProcessingParameterRules::MinStitchOverlapPercent() ||
            overlap->maximum() != ProcessingParameterRules::MaxStitchOverlapPercent() ||
            !interval || interval->minimum() != 250 || interval->maximum() != 10000 ||
            interval->value() != 1200 ||
            !retry || retry->isEnabled() || !save || save->isEnabled()) {
            qCritical() << "Stitch workflow options or initial action state are incomplete.";
            return 7;
        }
        return 0;
    }
    if (parser.isSet(verify_stitch_execution)) {
        QTemporaryDir directory;
        if (!directory.isValid()) return 80;

        QImage source(960, 480, QImage::Format_BGR888);
        source.fill(QColor(24, 30, 38));
        {
            QPainter painter(&source);
            painter.setRenderHint(QPainter::Antialiasing);
            for (int y = 20; y < source.height(); y += 55) {
                for (int x = 20; x < source.width(); x += 65) {
                    const QColor color((x * 7 + y * 3) % 220 + 25,
                        (x * 5 + y * 11) % 220 + 25,
                        (x * 13 + y * 2) % 220 + 25);
                    painter.setBrush(color);
                    painter.setPen(Qt::white);
                    painter.drawEllipse(QPointF(x, y), 8 + (x % 17), 7 + (y % 13));
                }
            }
            painter.setPen(QPen(Qt::yellow, 4));
            painter.drawLine(15, 70, 940, 410);
            painter.drawText(QRect(40, 180, 880, 80), Qt::AlignCenter,
                QStringLiteral("CameraView stitching migration verification"));
        }
        const QString left_path = directory.filePath(QStringLiteral("tile-01.png"));
        const QString right_path = directory.filePath(QStringLiteral("tile-02.png"));
        if (!source.copy(0, 0, 640, 480).save(left_path) ||
            !source.copy(320, 0, 640, 480).save(right_path)) {
            return 81;
        }

        auto* layout = window.findChild<QComboBox*>(QStringLiteral("StitchLayoutCombo"));
        auto* registration = window.findChild<QComboBox*>(QStringLiteral("StitchRegistrationCombo"));
        auto* transform = window.findChild<QComboBox*>(QStringLiteral("StitchTransformCombo"));
        auto* overlap = window.findChild<NumericSlider*>(QStringLiteral("StitchOverlapSlider"));
        auto* tile_list = window.findChild<QListWidget*>(QStringLiteral("StitchTileList"));
        auto* build = window.findChild<QPushButton*>(QStringLiteral("StitchBuildButton"));
        auto* retry = window.findChild<QPushButton*>(QStringLiteral("StitchRetryButton"));
        auto* save = window.findChild<QPushButton*>(QStringLiteral("StitchSaveButton"));
        auto* progress = window.findChild<QProgressBar*>(QStringLiteral("StitchProgress"));
        if (!layout || !registration || !transform || !overlap || !tile_list ||
            !build || !retry || !save || !progress) {
            return 82;
        }
        layout->setCurrentIndex(layout->findData(static_cast<int>(StitchLayoutMode::Linear)));
        registration->setCurrentIndex(
            registration->findData(static_cast<int>(StitchRegistrationMethod::Phase)));
        transform->setCurrentIndex(
            transform->findData(static_cast<int>(StitchTransformModel::Translation)));
        overlap->setValue(50);

        const QStringList files{left_path, right_path};
        if (!QMetaObject::invokeMethod(&window, "importStitchFiles", Qt::DirectConnection,
                Q_ARG(QStringList, files))) {
            return 83;
        }
        if (tile_list->count() != 2) return 83;

        build->click();
        QElapsedTimer elapsed;
        elapsed.start();
        QTimer watchdog;
        watchdog.setInterval(50);
        QObject::connect(&watchdog, &QTimer::timeout, &application,
            [&application, &watchdog, &elapsed, retry, save, progress] {
                if (save->isEnabled() && retry->isEnabled() && progress->value() == 100) {
                    watchdog.stop();
                    application.exit(0);
                } else if (elapsed.elapsed() > 20000) {
                    watchdog.stop();
                    application.exit(84);
                }
            });
        watchdog.start();
        return application.exec();
    }
    if (parser.isSet(live_camera_report)) {
        const QString report_path = parser.value(live_camera_report);
        auto* device = window.findChild<QComboBox*>(QStringLiteral("CameraDeviceCombo"));
        auto* state = window.findChild<QLabel*>(QStringLiteral("CameraStateLabel"));
        auto* fps = window.findChild<QLabel*>(QStringLiteral("PreviewFpsStatus"));
        auto* canvas = window.findChild<ImageCanvas*>(QStringLiteral("ImageCanvas"));
        if (!device || !state || !fps || !canvas || report_path.isEmpty()) return 9;

        window.show();
        QTimer::singleShot(750, &window, [&window] {
            QMetaObject::invokeMethod(&window, "refreshDevices", Qt::QueuedConnection);
        });
        QElapsedTimer discovery_elapsed;
        discovery_elapsed.start();
        QTimer discovery_timer;
        discovery_timer.setInterval(250);
        QObject::connect(&discovery_timer, &QTimer::timeout, &window,
            [&application, &window, &discovery_timer, &discovery_elapsed,
             report_path, device, state] {
                if (device->count() > 0) {
                    discovery_timer.stop();
                    device->setCurrentIndex(0);
                    QMetaObject::invokeMethod(&window, "openSelectedCamera", Qt::QueuedConnection);
                    return;
                }
                if (discovery_elapsed.elapsed() > 8000) {
                    QJsonObject report;
                    report.insert(QStringLiteral("camera_state"), state->text());
                    report.insert(QStringLiteral("device_count"), device->count());
                    QFile output(report_path);
                    if (output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                        output.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
                    }
                    discovery_timer.stop();
                    application.exit(9);
                }
            });
        discovery_timer.start();
        QTimer::singleShot(20000, &application,
            [&application, &window, report_path, state, fps, canvas] {
                QJsonObject report;
                report.insert(QStringLiteral("camera_state"), state->text());
                report.insert(QStringLiteral("fps_label"), fps->text());
                report.insert(QStringLiteral("preview_sequence"),
                    static_cast<qint64>(canvas->property("cameraPreviewSequence").toULongLong()));
                report.insert(QStringLiteral("direct_camera_preview"),
                    canvas->property("directCameraPreview").toBool());
                report.insert(QStringLiteral("image_width"), canvas->imageSize().width());
                report.insert(QStringLiteral("image_height"), canvas->imageSize().height());
                QFile output(report_path);
                const bool saved = output.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
                    output.write(QJsonDocument(report).toJson(QJsonDocument::Indented)) >= 0;
                output.close();
                const bool valid = saved &&
                    report.value(QStringLiteral("preview_sequence")).toInteger() > 0 &&
                    report.value(QStringLiteral("direct_camera_preview")).toBool() &&
                    QRegularExpression(QStringLiteral("\\d")).match(fps->text()).hasMatch();
                QMetaObject::invokeMethod(&window, "stopCamera", Qt::QueuedConnection);
                application.exit(valid ? 0 : 9);
            });
        return application.exec();
    }
    if (parser.isSet(verify_measurement_toolbar)) {
        QToolBar* toolbar = window.findChild<QToolBar*>(QStringLiteral("MeasurementToolbar"));
        const QStringList required{
            QStringLiteral("标定"), QStringLiteral("点"), QStringLiteral("长度"), QStringLiteral("折线"),
            QStringLiteral("角度"), QStringLiteral("矩形"), QStringLiteral("多边形"), QStringLiteral("圆"),
            QStringLiteral("椭圆"), QStringLiteral("剖线测量"), QStringLiteral("智能框选"),
            QStringLiteral("开始计数"), QStringLiteral("自动寻边"), QStringLiteral("删除"),
            QStringLiteral("清空"), QStringLiteral("导出 CSV")};
        QStringList actual;
        bool icons_complete = toolbar != nullptr;
        if (toolbar) {
            for (const QAction* action : toolbar->actions()) {
                if (action->isSeparator()) continue;
                actual.push_back(action->text());
                icons_complete = icons_complete && !action->icon().isNull();
            }
        }
        for (const QString& name : required) {
            if (!actual.contains(name)) {
                qCritical().noquote() << QStringLiteral("Measurement toolbar is missing: %1").arg(name);
                return 5;
            }
        }
        if (!icons_complete || actual.size() != required.size()) {
            qCritical() << "Measurement toolbar action or icon coverage is incomplete.";
            return 5;
        }
        if (window.toolBarBreak(toolbar)) {
            qCritical() << "Measurement toolbar must share one row with the main toolbar.";
            return 5;
        }
        return 0;
    }
    if (parser.isSet(workspace_tab)) {
        bool valid = false;
        const int index = parser.value(workspace_tab).toInt(&valid);
        if (QTabWidget* tabs = window.findChild<QTabWidget*>(QStringLiteral("FunctionTabs"));
            valid && tabs && index >= 0 && index < tabs->count()) {
            tabs->setCurrentIndex(index);
        }
    }
    if (parser.isSet(ui_snapshot)) {
        window.show();
        if (parser.isSet(focus_widget)) {
            if (QWidget* target = window.findChild<QWidget*>(parser.value(focus_widget))) {
                QWidget* ancestor = target->parentWidget();
                while (ancestor) {
                    if (auto* scroll = qobject_cast<QScrollArea*>(ancestor)) {
                        scroll->ensureWidgetVisible(target, 12, 12);
                        break;
                    }
                    ancestor = ancestor->parentWidget();
                }
            }
        }
        const QString snapshot_path = parser.value(ui_snapshot);
        QTimer::singleShot(1000, &application, [&application, &window, snapshot_path] {
            const bool saved = window.grab().save(snapshot_path);
            if (!saved) qCritical().noquote() << QStringLiteral("Cannot save UI snapshot: %1").arg(snapshot_path);
            application.exit(saved ? 0 : 3);
        });
    } else if (parser.isSet(smoke_test)) {
        QTimer::singleShot(750, &application, &QCoreApplication::quit);
    } else {
        window.show();
    }
    return application.exec();
}

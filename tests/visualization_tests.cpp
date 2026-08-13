#include "qt/ImageSurface3DDialog.h"
#include "qt/ImageSurface3DWidget.h"
#include "qt/ProfileAnalysisDialog.h"
#include "qt/ProfilePlotWidget.h"
#include "qt/PointCloudDialog.h"
#include "qt/PointCloudDeviationDialog.h"
#include "qt/PointCloudSectionDialog.h"
#include "qt/PointCloudWidget.h"
#include "qt/CameraViewTheme.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QPixmap>
#include <QTabWidget>

#include <cmath>
#include <cstring>
#include <iostream>

namespace {

int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
}

bool near(double left, double right, double tolerance = 1e-5)
{
    return std::abs(left - right) <= tolerance;
}

ImageFrame frameFromImage(const QImage& image)
{
    const QImage bgr = image.convertToFormat(QImage::Format_BGR888);
    ImageFrame frame;
    frame.width = bgr.width();
    frame.height = bgr.height();
    frame.stride = bgr.bytesPerLine();
    frame.bgr.resize(static_cast<std::size_t>(frame.stride * frame.height));
    for (int row = 0; row < frame.height; ++row) {
        std::memcpy(frame.bgr.data() + row * frame.stride, bgr.constScanLine(row), frame.stride);
    }
    return frame;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    applyCameraViewTheme(application);
    QImage image(180, 120, QImage::Format_RGB32);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const double ridge = std::exp(-(
                std::pow((x - 95.0) / 34.0, 2.0) +
                std::pow((y - 58.0) / 25.0, 2.0)));
            const int value = qBound(0, qRound(30.0 + 205.0 * ridge + 20.0 * std::sin(x / 11.0)), 255);
            image.setPixelColor(x, y, QColor(value, qBound(0, value + y / 5, 255), qBound(0, 255 - value / 2, 255)));
        }
    }

    ImageSurface3DWidget surface;
    surface.resize(760, 500);
    surface.setImage(image);
    surface.setResolution(180);
    surface.setVerticalScale(1.6);
    surface.show();
    application.processEvents();
    if (!surface.hasSurface() || surface.gridSize() != image.size()) {
        return fail("3D surface grid was not generated.");
    }
    if (!surface.renderBackend().startsWith(QStringLiteral("OpenGL"))) {
        return fail("3D surface did not initialize an OpenGL render backend.");
    }
    const QColor reference = image.pixelColor(90, 60);
    surface.setHeightChannel(SurfaceHeightChannel::Red);
    if (surface.heightChannel() != SurfaceHeightChannel::Red ||
        !near(surface.heightAt(90, 60), reference.redF())) {
        return fail("Red height channel was not applied.");
    }
    surface.setHeightChannel(SurfaceHeightChannel::Green);
    if (!near(surface.heightAt(90, 60), reference.greenF())) {
        return fail("Green height channel was not applied.");
    }
    surface.setHeightChannel(SurfaceHeightChannel::Blue);
    if (!near(surface.heightAt(90, 60), reference.blueF())) {
        return fail("Blue height channel was not applied.");
    }
    surface.setHeightChannel(SurfaceHeightChannel::Luminance);

    QElapsedTimer render_timer;
    render_timer.start();
    const QImage surface_snapshot = surface.grab().toImage();
    const qint64 full_render_ms = render_timer.elapsed();
    const int full_face_count = surface.lastRenderedFaceCount();
    if (surface.lastRenderStride() != 1 ||
        full_face_count != (surface.gridSize().width() - 1) * (surface.gridSize().height() - 1)) {
        return fail("Full-quality 3D render did not use every surface cell.");
    }

    QMouseEvent drag_press(
        QEvent::MouseButtonPress, QPointF(300.0, 220.0), QPointF(300.0, 220.0),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&surface, &drag_press);
    render_timer.restart();
    surface.grab();
    const qint64 interactive_render_ms = render_timer.elapsed();
    const int interactive_face_count = surface.lastRenderedFaceCount();
    if (surface.lastRenderStride() <= 1 || interactive_face_count >= full_face_count) {
        return fail("High-resolution drag did not activate adaptive surface detail.");
    }
    QMouseEvent drag_release(
        QEvent::MouseButtonRelease, QPointF(300.0, 220.0), QPointF(300.0, 220.0),
        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&surface, &drag_release);
    surface.grab();
    if (surface.lastRenderStride() != 1 || surface.lastRenderedFaceCount() != full_face_count) {
        return fail("Full surface detail was not restored after dragging.");
    }
    std::cout << "3D render 180x120: " << full_render_ms << " ms, full faces "
              << full_face_count << ", interactive faces " << interactive_face_count
              << " in " << interactive_render_ms << " ms"
              << ", backend " << surface.renderBackend().toStdString()
              << (surface.hardwareAccelerated() ? " (hardware)" : " (software fallback)") << '\n';
    if (surface_snapshot.isNull() || !surface_snapshot.save(
            QDir::current().filePath(QStringLiteral("CameraView-3d-surface.png")))) {
        return fail("3D surface snapshot could not be rendered.");
    }

    const ImageFrame frame = frameFromImage(image);
    const ImageProfileResult profile = ImageProfileSampler::Sample(
        frame, {8.0, 60.0}, {170.0, 60.0}, ImageProfileChannel::Luminance);
    ProfilePlotWidget plot;
    plot.resize(760, 460);
    plot.setProfile(profile, 0.5, QStringLiteral("µm"), QStringLiteral("亮度"));
    plot.show();
    application.processEvents();
    if (!profile.IsValid() || plot.profileSampleCount() < 100 ||
        !plot.grab().save(QDir::current().filePath(QStringLiteral("CameraView-profile-plot.png")))) {
        return fail("Profile plot could not be rendered.");
    }

    ImageSurface3DDialog surface_dialog(image, QStringLiteral("synthetic"));
    auto* height_channel_combo = surface_dialog.findChild<QComboBox*>(
        QStringLiteral("SurfaceHeightChannelCombo"));
    auto* backend_label = surface_dialog.findChild<QLabel*>(QStringLiteral("SurfaceRenderBackend"));
    if (!surface_dialog.surfaceWidget()->hasSurface() ||
        !height_channel_combo || height_channel_combo->count() != 4 || !backend_label) {
        return fail("3D dialog did not receive the image surface.");
    }
    height_channel_combo->setCurrentIndex(1);
    if (surface_dialog.surfaceWidget()->heightChannel() != SurfaceHeightChannel::Red) {
        return fail("3D dialog did not apply its RGB height-channel selection.");
    }
    height_channel_combo->setCurrentIndex(0);
    surface_dialog.resize(1040, 720);
    surface_dialog.show();
    application.processEvents();
    if (!backend_label->text().contains(QStringLiteral("OpenGL"))) {
        return fail("3D dialog did not display its render backend.");
    }
    if (!surface_dialog.grab().save(
            QDir::current().filePath(QStringLiteral("CameraView-3d-dialog.png")))) {
        return fail("3D dialog snapshot could not be rendered.");
    }
    ProfileAnalysisDialog profile_dialog(
        frame, {8.0, 60.0}, {170.0, 60.0},
        CalibrationProfile::FromMicronsPerPixel(0.5),
        MeasurementUnit::Micrometers,
        QStringLiteral("synthetic"));
    if (!profile_dialog.profile().IsValid() || profile_dialog.plotWidget()->profileSampleCount() < 100) {
        return fail("Profile analysis dialog did not compute calibrated samples.");
    }
    profile_dialog.resize(980, 680);
    profile_dialog.show();
    application.processEvents();
    if (!profile_dialog.grab().save(
            QDir::current().filePath(QStringLiteral("CameraView-profile-dialog.png")))) {
        return fail("Profile analysis dialog snapshot could not be rendered.");
    }

    PointCloud point_cloud;
    point_cloud.name = L"synthetic point cloud";
    point_cloud.unit = PointCloudUnit::Millimeters;
    for (int y = -12; y <= 12; ++y) {
        for (int x = -16; x <= 16; ++x) {
            const double z = 2.5 * std::exp(-(
                std::pow(x / 8.0, 2.0) + std::pow(y / 7.0, 2.0)));
            PointCloudPoint point;
            point.x = x * 0.4;
            point.y = y * 0.4;
            point.z = z;
            point.r = static_cast<std::uint8_t>(qBound(0, 80 + x * 4, 255));
            point.g = static_cast<std::uint8_t>(qBound(0, 120 + y * 4, 255));
            point.b = 220;
            point.has_color = true;
            point_cloud.points.push_back(point);
        }
    }
    point_cloud.RecalculateBounds();
    PointCloudWidget point_cloud_view;
    point_cloud_view.resize(780, 520);
    point_cloud_view.setCloud(point_cloud);
    point_cloud_view.show();
    application.processEvents();
    const QImage point_cloud_snapshot = point_cloud_view.grab().toImage();
    const int center_index = static_cast<int>(point_cloud.points.size() / 2);
    const QPointF center_screen = point_cloud_view.screenPosition(center_index);
    if (!point_cloud_view.hasCloud() || point_cloud_view.renderedPointCount() < 700 ||
        point_cloud_view.pickNearest(center_screen, 8.0) < 0 ||
        !point_cloud_view.renderBackend().startsWith(QStringLiteral("OpenGL")) ||
        point_cloud_snapshot.isNull() || !point_cloud_snapshot.save(
            QDir::current().filePath(QStringLiteral("CameraView-point-cloud.png")))) {
        return fail("3D point-cloud rendering or screen-space picking failed.");
    }

    PointCloudDialog point_cloud_dialog(point_cloud);
    PointCloudDialog empty_point_cloud_dialog;
    auto* empty_export = empty_point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudExportButton"));
    auto* empty_measure = empty_point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudDistanceMeasureButton"));
    auto* empty_rating = empty_point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudEvaluateTolerancesButton"));
    auto* empty_section = empty_point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudBeginSectionButton"));
    if (!empty_export || empty_export->isEnabled() || !empty_measure || empty_measure->isEnabled() ||
        !empty_rating || empty_rating->isEnabled() || !empty_section || empty_section->isEnabled()) {
        return fail("Point-cloud actions were not disabled before loading data.");
    }
    const QStringList point_cloud_controls{
        QStringLiteral("PointCloudView"),
        QStringLiteral("PointCloudOpenButton"),
        QStringLiteral("PointCloudExportButton"),
        QStringLiteral("PointCloudUnitCombo"),
        QStringLiteral("PointCloudColorCombo"),
        QStringLiteral("PointCloudVoxelApplyButton"),
        QStringLiteral("PointCloudOutlierApplyButton"),
        QStringLiteral("PointCloudBeginInteractiveCropButton"),
        QStringLiteral("PointCloudKeepSelectionButton"),
        QStringLiteral("PointCloudRemoveSelectionButton"),
        QStringLiteral("PointCloudSmartFilterApplyButton"),
        QStringLiteral("PointCloudHoleRepairApplyButton"),
        QStringLiteral("PointCloudFitPlaneButton"),
        QStringLiteral("PointCloudFitPlaneModelButton"),
        QStringLiteral("PointCloudFitSphereButton"),
        QStringLiteral("PointCloudFitCylinderButton"),
        QStringLiteral("PointCloudModelList"),
        QStringLiteral("PointCloudShowFittedPlaneCheck"),
        QStringLiteral("PointCloudLevelButton"),
        QStringLiteral("PointCloudUndoButton"),
        QStringLiteral("PointCloudDistanceMeasureButton"),
        QStringLiteral("PointCloudHeightMeasureButton"),
        QStringLiteral("PointCloudAngleMeasureButton"),
        QStringLiteral("PointCloudPlaneMeasureButton"),
        QStringLiteral("PointCloudPlaneAngleButton"),
        QStringLiteral("PointCloudLineIntersectionButton"),
        QStringLiteral("PointCloudEvaluateTolerancesButton"),
        QStringLiteral("PointCloudDeviationDistributionButton"),
        QStringLiteral("PointCloudBeginSectionButton"),
        QStringLiteral("PointCloudMeasurementList")};
    for (const QString& name : point_cloud_controls) {
        if (!point_cloud_dialog.findChild<QWidget*>(name)) {
            return fail("3D point-cloud workbench is missing a required control.");
        }
    }
    auto* fit_plane_button = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudFitPlaneButton"));
    auto* show_plane_check = point_cloud_dialog.findChild<QCheckBox*>(
        QStringLiteral("PointCloudShowFittedPlaneCheck"));
    fit_plane_button->click();
    application.processEvents();
    if (!point_cloud_dialog.cloudWidget()->fittedPlane().valid ||
        !point_cloud_dialog.cloudWidget()->fittedPlaneVisible() ||
        !show_plane_check || !show_plane_check->isEnabled() || !show_plane_check->isChecked()) {
        return fail("Fitted plane was not displayed in the 3D point-cloud view.");
    }
    show_plane_check->setChecked(false);
    if (point_cloud_dialog.cloudWidget()->fittedPlaneVisible()) {
        return fail("Fitted-plane visibility control did not hide the overlay.");
    }
    show_plane_check->setChecked(true);
    point_cloud_dialog.show();
    application.processEvents();
    if (!point_cloud_dialog.grab().save(
            QDir::current().filePath(QStringLiteral("CameraView-point-cloud-fitted-plane.png")))) {
        return fail("Fitted-plane 3D visualization snapshot could not be rendered.");
    }
    auto* deviation_button = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudDeviationDistributionButton"));
    deviation_button->click();
    application.processEvents();
    auto* deviation_dialog = point_cloud_dialog.findChild<PointCloudDeviationDialog*>(
        QStringLiteral("PointCloudDeviationDialog"));
    if (!deviation_dialog || !deviation_dialog->distribution().valid ||
        deviation_dialog->distribution().deviations.size() != point_cloud.Size() ||
        deviation_dialog->plotWidget()->binCount() < 8 ||
        !deviation_dialog->grab().save(QDir::current().filePath(
            QStringLiteral("CameraView-point-cloud-deviation-distribution.png")))) {
        return fail("Point-cloud Gaussian deviation distribution could not be rendered.");
    }
    deviation_dialog->close();
    const QRectF point_cloud_selection_rect(
        point_cloud_dialog.cloudWidget()->rect().adjusted(120, 100, -120, -100));
    const QVector<int> box_selected =
        point_cloud_dialog.cloudWidget()->indicesInScreenRect(point_cloud_selection_rect);
    if (box_selected.isEmpty() || box_selected.size() >= point_cloud.Size()) {
        return fail("3D point-cloud screen rectangle selection failed.");
    }
    QVector<int> drag_selected;
    QObject::connect(point_cloud_dialog.cloudWidget(),
        &PointCloudWidget::boxSelectionFinished,
        [&drag_selected](const QVector<int>& indices) { drag_selected = indices; });
    auto* begin_selection = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudBeginInteractiveCropButton"));
    auto* keep_selection = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudKeepSelectionButton"));
    auto* remove_selection = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudRemoveSelectionButton"));
    point_cloud_dialog.setMeasurementMode(PointCloudMeasureMode::Distance);
    begin_selection->click();
    auto* navigate_button = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudNavigateButton"));
    if (!point_cloud_dialog.cloudWidget()->boxSelectionEnabled() ||
        point_cloud_dialog.measurementMode() != PointCloudMeasureMode::Navigate ||
        !navigate_button || !navigate_button->isChecked()) {
        return fail("Interactive crop button did not enter box-selection mode.");
    }
    const QPointF selection_start = point_cloud_selection_rect.topLeft();
    const QPointF selection_end = point_cloud_selection_rect.bottomRight();
    QMouseEvent selection_press(QEvent::MouseButtonPress,
        selection_start, selection_start, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent selection_move(QEvent::MouseMove,
        selection_end, selection_end, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent selection_release(QEvent::MouseButtonRelease,
        selection_end, selection_end, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(point_cloud_dialog.cloudWidget(), &selection_press);
    QApplication::sendEvent(point_cloud_dialog.cloudWidget(), &selection_move);
    QApplication::sendEvent(point_cloud_dialog.cloudWidget(), &selection_release);
    if (drag_selected.size() != box_selected.size() ||
        point_cloud_dialog.cloudWidget()->boxSelectionEnabled() ||
        !keep_selection->isEnabled() || !remove_selection->isEnabled()) {
        return fail("3D point-cloud drag selection did not emit its selected points.");
    }
    point_cloud_dialog.setMeasurementMode(PointCloudMeasureMode::Distance);
    if (point_cloud_dialog.measurementMode() != PointCloudMeasureMode::Distance ||
        !point_cloud_dialog.cloudWidget()->pickingEnabled()) {
        return fail("3D point-cloud measurement mode did not enable point picking.");
    }
    if (box_selected.size() != drag_selected.size()) {
        return fail("Universal point-cloud selection was not preserved when switching tools.");
    }
    if (!QMetaObject::invokeMethod(&point_cloud_dialog, "acceptPickedPoint",
            Qt::DirectConnection, Q_ARG(int, 0)) ||
        !QMetaObject::invokeMethod(&point_cloud_dialog, "acceptPickedPoint",
            Qt::DirectConnection, Q_ARG(int, center_index)) ||
        point_cloud_dialog.measurementCount() != 1 ||
        point_cloud_dialog.measurementMode() != PointCloudMeasureMode::Distance ||
        !point_cloud_dialog.cloudWidget()->pickingEnabled()) {
        return fail("3D point-cloud continuous distance measurement did not remain active.");
    }
    auto* section_button = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudBeginSectionButton"));
    section_button->click();
    if (!point_cloud_dialog.cloudWidget()->sectionSelectionEnabled() ||
        point_cloud_dialog.measurementMode() != PointCloudMeasureMode::Navigate ||
        !navigate_button || !navigate_button->isChecked()) {
        return fail("Point-cloud section button did not enter line-selection mode.");
    }
    const QPointF section_start(130.0, point_cloud_dialog.cloudWidget()->height() * 0.55);
    const QPointF section_end(
        point_cloud_dialog.cloudWidget()->width() - 130.0,
        point_cloud_dialog.cloudWidget()->height() * 0.55);
    QMouseEvent section_press(QEvent::MouseButtonPress,
        section_start, section_start, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent section_move(QEvent::MouseMove,
        section_end, section_end, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent section_release(QEvent::MouseButtonRelease,
        section_end, section_end, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(point_cloud_dialog.cloudWidget(), &section_press);
    QApplication::sendEvent(point_cloud_dialog.cloudWidget(), &section_move);
    QApplication::sendEvent(point_cloud_dialog.cloudWidget(), &section_release);
    application.processEvents();
    auto* section_dialog = point_cloud_dialog.findChild<PointCloudSectionDialog*>(
        QStringLiteral("PointCloudSectionDialog"));
    if (!section_dialog || !section_dialog->profile().valid ||
        section_dialog->plotWidget()->sampleCount() < 8 ||
        !section_dialog->grab().save(
            QDir::current().filePath(QStringLiteral("CameraView-point-cloud-section.png")))) {
        return fail("Arbitrary point-cloud section interaction or plot rendering failed.");
    }
    section_dialog->close();
    const int measurements_before_rating = point_cloud_dialog.measurementCount();
    auto* evaluate_tolerances = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudEvaluateTolerancesButton"));
    auto* tolerance_summary = point_cloud_dialog.findChild<QLabel*>(
        QStringLiteral("PointCloudToleranceSummary"));
    evaluate_tolerances->click();
    if (point_cloud_dialog.measurementCount() != measurements_before_rating + 5 ||
        !tolerance_summary || !tolerance_summary->text().contains(QStringLiteral("平面度"))) {
        return fail("One-click point-cloud tolerance rating did not publish five results.");
    }
    point_cloud_dialog.resize(1180, 760);
    point_cloud_dialog.show();
    application.processEvents();
    auto* point_cloud_tabs = point_cloud_dialog.findChild<QTabWidget*>(
        QStringLiteral("PointCloudToolTabs"));
    if (point_cloud_dialog.cloud().Size() != point_cloud.Size() ||
        !point_cloud_tabs || point_cloud_tabs->count() != 5) {
        return fail("3D point-cloud workbench tabs could not be rendered.");
    }
    const QStringList point_cloud_tab_snapshots{
        QStringLiteral("CameraView-point-cloud-dialog-data.png"),
        QStringLiteral("CameraView-point-cloud-dialog-processing.png"),
        QStringLiteral("CameraView-point-cloud-dialog-fit.png"),
        QStringLiteral("CameraView-point-cloud-dialog-measurement.png"),
        QStringLiteral("CameraView-point-cloud-dialog-inspection.png")};
    for (int tab_index = 0; tab_index < point_cloud_tabs->count(); ++tab_index) {
        point_cloud_tabs->setCurrentIndex(tab_index);
        application.processEvents();
        if (!point_cloud_dialog.grab().save(
                QDir::current().filePath(point_cloud_tab_snapshots[tab_index]))) {
            return fail("3D point-cloud workbench snapshot could not be rendered.");
        }
    }
    return 0;
}

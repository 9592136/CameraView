#include "qt/ImageSurface3DDialog.h"
#include "qt/ImageSurface3DWidget.h"
#include "qt/ProfileAnalysisDialog.h"
#include "qt/ProfilePlotWidget.h"
#include "qt/PointCloudDialog.h"
#include "qt/PointCloudDeviationDialog.h"
#include "qt/PointCloudSectionDialog.h"
#include "qt/PointCloudWidget.h"
#include "qt/CameraViewTheme.h"
#include "pointcloud/PointCloudIO.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QPixmap>
#include <QSplitter>
#include <QTabWidget>
#include <QThread>
#include <QToolButton>

#include <cmath>
#include <cstring>
#include <filesystem>
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
    if (argc == 3 && std::string(argv[1]) == "--snapshot-h3d") {
        PointCloud h3d;
        std::wstring error;
        if (!PointCloudIO::Load(std::filesystem::path(argv[2]), h3d, error)) {
            std::wcerr << L"H3D snapshot import failed: " << error << L'\n';
            return 1;
        }
        PointCloudDialog dialog(h3d);
        dialog.resize(1280, 800);
        dialog.show();
        application.processEvents();
        const int texture_index = dialog.findChild<QComboBox*>(
            QStringLiteral("PointCloudColorCombo"))->findData(
                static_cast<int>(PointCloudColorMode::Texture));
        dialog.findChild<QComboBox*>(QStringLiteral("PointCloudColorCombo"))
            ->setCurrentIndex(texture_index);
        application.processEvents();
        return dialog.grab().save(QDir::current().filePath(
            QStringLiteral("CameraView-real-h3d-texture.png"))) ? 0 : 1;
    }
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
    point_cloud.organized_width = 33;
    point_cloud.organized_height = 25;
    point_cloud.texture_available = true;
    point_cloud.format_name = L"Synthetic H3D";
    point_cloud.RecalculateBounds();
    PointCloudWidget point_cloud_view;
    point_cloud_view.resize(780, 520);
    point_cloud_view.setCloud(point_cloud);
    point_cloud_view.setColorMode(PointCloudColorMode::Texture);
    point_cloud_view.show();
    application.processEvents();
    const QImage point_cloud_snapshot = point_cloud_view.grab().toImage();
    const int center_index = static_cast<int>(point_cloud.points.size() / 2);
    const QPointF center_screen = point_cloud_view.screenPosition(center_index);
    if (!point_cloud_view.hasCloud() || !point_cloud_view.textureSurfaceAvailable() ||
        point_cloud_view.colorMode() != PointCloudColorMode::Texture ||
        point_cloud_view.renderedPointCount() < 700 ||
        point_cloud_view.pickNearest(center_screen, 8.0) < 0 ||
        !point_cloud_view.renderBackend().startsWith(QStringLiteral("OpenGL")) ||
        point_cloud_snapshot.isNull() || !point_cloud_snapshot.save(
            QDir::current().filePath(QStringLiteral("CameraView-point-cloud.png")))) {
        return fail("3D point-cloud rendering or screen-space picking failed.");
    }
    point_cloud_view.setViewPreset(PointCloudViewPreset::Top);
    if (!near(point_cloud_view.yawDegrees(), 0.0) ||
        !near(point_cloud_view.pitchDegrees(), 90.0)) {
        return fail("Point-cloud standard view presets were not applied.");
    }
    point_cloud_view.setViewPreset(PointCloudViewPreset::Isometric);

    PointCloud large_point_cloud;
    large_point_cloud.unit = PointCloudUnit::Millimeters;
    large_point_cloud.points.reserve(160000);
    for (int y = 0; y < 400; ++y) {
        for (int x = 0; x < 400; ++x) {
            large_point_cloud.points.push_back({
                x * 0.02, y * 0.02,
                0.3 * std::sin(x * 0.035) * std::cos(y * 0.035)});
        }
    }
    large_point_cloud.RecalculateBounds();
    point_cloud_view.setCloud(large_point_cloud);
    point_cloud_view.grab();
    const int full_point_count = point_cloud_view.renderedPointCount();
    QMouseEvent cloud_drag_press(
        QEvent::MouseButtonPress, QPointF(300.0, 220.0), QPointF(300.0, 220.0),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent cloud_drag_move(
        QEvent::MouseMove, QPointF(350.0, 245.0), QPointF(350.0, 245.0),
        Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&point_cloud_view, &cloud_drag_press);
    QApplication::sendEvent(&point_cloud_view, &cloud_drag_move);
    point_cloud_view.grab();
    const int interactive_point_count = point_cloud_view.renderedPointCount();
    if (!point_cloud_view.interactiveRendering() ||
        interactive_point_count >= full_point_count || interactive_point_count > 34000) {
        return fail("Large point-cloud navigation did not activate adaptive rendering.");
    }
    QMouseEvent cloud_drag_release(
        QEvent::MouseButtonRelease, QPointF(350.0, 245.0), QPointF(350.0, 245.0),
        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&point_cloud_view, &cloud_drag_release);
    QThread::msleep(130);
    application.processEvents();
    point_cloud_view.grab();
    if (point_cloud_view.interactiveRendering() ||
        point_cloud_view.renderedPointCount() != full_point_count) {
        return fail("Full point-cloud quality was not restored after navigation.");
    }

    point_cloud_view.resetView();
    QMouseEvent continuous_pitch_press(
        QEvent::MouseButtonPress, QPointF(300.0, 220.0), QPointF(300.0, 220.0),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent continuous_pitch_move(
        QEvent::MouseMove, QPointF(300.0, -180.0), QPointF(300.0, -180.0),
        Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent continuous_pitch_release(
        QEvent::MouseButtonRelease, QPointF(300.0, -180.0), QPointF(300.0, -180.0),
        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&point_cloud_view, &continuous_pitch_press);
    QApplication::sendEvent(&point_cloud_view, &continuous_pitch_move);
    QApplication::sendEvent(&point_cloud_view, &continuous_pitch_release);
    if (std::abs(point_cloud_view.pitchDegrees()) <= 85.0 ||
        std::abs(point_cloud_view.pitchDegrees()) > 180.0) {
        return fail("Point-cloud pitch rotation stopped at the former pole limit.");
    }
    std::cout << "Point-cloud adaptive render: full " << full_point_count
              << ", interactive " << interactive_point_count << " points\n";

    large_point_cloud.organized_width = 400;
    large_point_cloud.organized_height = 400;
    large_point_cloud.texture_available = true;
    for (int index = 0; index < static_cast<int>(large_point_cloud.points.size()); ++index) {
        PointCloudPoint& point = large_point_cloud.points[static_cast<std::size_t>(index)];
        point.r = static_cast<std::uint8_t>(40 + index % 180);
        point.g = static_cast<std::uint8_t>(70 + (index / 400) % 150);
        point.b = 205;
        point.has_color = true;
    }
    point_cloud_view.setCloud(large_point_cloud);
    point_cloud_view.setColorMode(PointCloudColorMode::Texture);
    point_cloud_view.grab();
    const int full_texture_vertices = point_cloud_view.renderedPointCount();
    QMouseEvent texture_drag_press(
        QEvent::MouseButtonPress, QPointF(280.0, 210.0), QPointF(280.0, 210.0),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent texture_drag_move(
        QEvent::MouseMove, QPointF(330.0, 235.0), QPointF(330.0, 235.0),
        Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&point_cloud_view, &texture_drag_press);
    QApplication::sendEvent(&point_cloud_view, &texture_drag_move);
    point_cloud_view.grab();
    const int interactive_texture_vertices = point_cloud_view.renderedPointCount();
    const std::uint64_t first_interactive_texture_revision =
        point_cloud_view.textureMeshRevision();
    QMouseEvent texture_drag_move_second(
        QEvent::MouseMove, QPointF(370.0, 260.0), QPointF(370.0, 260.0),
        Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&point_cloud_view, &texture_drag_move_second);
    point_cloud_view.grab();
    const std::uint64_t second_interactive_texture_revision =
        point_cloud_view.textureMeshRevision();
    QMouseEvent texture_drag_release(
        QEvent::MouseButtonRelease, QPointF(370.0, 260.0), QPointF(370.0, 260.0),
        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&point_cloud_view, &texture_drag_release);
    if (full_texture_vertices < 150000 || interactive_texture_vertices >= full_texture_vertices ||
        interactive_texture_vertices > 50000) {
        return fail("GPU texture mesh did not preserve static detail or reduce interaction cost.");
    }
    if (second_interactive_texture_revision <= first_interactive_texture_revision) {
        return fail("GPU texture mesh was not rebuilt for consecutive drag frames.");
    }

    point_cloud_view.setCloud(point_cloud);
    point_cloud_view.grab();

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
        QStringLiteral("PointCloudViewPresetCombo"),
        QStringLiteral("PointCloudTextureStatus"),
        QStringLiteral("PointCloudTextureEnhancementCheck"),
        QStringLiteral("PointCloudVoxelApplyButton"),
        QStringLiteral("PointCloudOutlierApplyButton"),
        QStringLiteral("PointCloudBeginInteractiveCropButton"),
        QStringLiteral("PointCloudClearSelectionButton"),
        QStringLiteral("PointCloudKeepSelectionButton"),
        QStringLiteral("PointCloudRemoveSelectionButton"),
        QStringLiteral("PointCloudSmartFilterApplyButton"),
        QStringLiteral("PointCloudHoleRepairApplyButton"),
        QStringLiteral("PointCloudFitPlaneButton"),
        QStringLiteral("PointCloudFitPlaneModelButton"),
        QStringLiteral("PointCloudFitSphereButton"),
        QStringLiteral("PointCloudFitCylinderButton"),
        QStringLiteral("PointCloudCancelFitButton"),
        QStringLiteral("PointCloudModelList"),
        QStringLiteral("PointCloudShowModelButton"),
        QStringLiteral("PointCloudHideModelButton"),
        QStringLiteral("PointCloudDeleteModelButton"),
        QStringLiteral("PointCloudClearModelsButton"),
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
        QStringLiteral("PointCloudMeasurementList"),
        QStringLiteral("PointCloudDeleteMeasurementButton"),
        QStringLiteral("PointCloudClearMeasurementsButton"),
        QStringLiteral("PointCloudExportMeasurementsButton")};
    for (const QString& name : point_cloud_controls) {
        if (!point_cloud_dialog.findChild<QWidget*>(name)) {
            return fail("3D point-cloud workbench is missing a required control.");
        }
    }
    auto* clear_selection_action = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudClearSelectionButton"));
    auto* fit_scope = point_cloud_dialog.findChild<QComboBox*>(
        QStringLiteral("PointCloudFitScopeCombo"));
    auto* fit_plane_model_action = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudFitPlaneModelButton"));
    auto* delete_measurement_action = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudDeleteMeasurementButton"));
    auto* clear_measurements_action = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudClearMeasurementsButton"));
    auto* export_measurements_action = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudExportMeasurementsButton"));
    if (!clear_selection_action || clear_selection_action->isEnabled() || !fit_scope ||
        !fit_plane_model_action || !delete_measurement_action ||
        delete_measurement_action->isEnabled() || !clear_measurements_action ||
        clear_measurements_action->isEnabled() || !export_measurements_action ||
        export_measurements_action->isEnabled()) {
        return fail("Point-cloud selection or measurement actions did not reflect the empty state.");
    }
    fit_scope->setCurrentIndex(fit_scope->findData(
        static_cast<int>(PointCloudFitScope::Selection)));
    if (fit_plane_model_action->isEnabled()) {
        return fail("Selection-scoped fitting stayed enabled without a selection.");
    }
    fit_scope->setCurrentIndex(fit_scope->findData(
        static_cast<int>(PointCloudFitScope::WholeCloud)));
    if (!fit_plane_model_action->isEnabled()) {
        return fail("Whole-cloud fitting was not enabled for valid point-cloud data.");
    }
    auto* texture_enhancement = point_cloud_dialog.findChild<QCheckBox*>(
        QStringLiteral("PointCloudTextureEnhancementCheck"));
    if (!texture_enhancement || !texture_enhancement->isEnabled()) {
        return fail("H3D texture enhancement control is not available for textured data.");
    }
    texture_enhancement->setChecked(false);
    if (point_cloud_dialog.cloudWidget()->textureEnhancementEnabled()) {
        return fail("H3D raw-texture mode did not disable texture enhancement.");
    }
    texture_enhancement->setChecked(true);
    auto* fit_plane_button = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudFitPlaneButton"));
    auto* show_plane_check = point_cloud_dialog.findChild<QCheckBox*>(
        QStringLiteral("PointCloudShowFittedPlaneCheck"));
    auto* level_group = point_cloud_dialog.findChild<QGroupBox*>(
        QStringLiteral("PointCloudLevelGroup"));
    if (!level_group) return fail("Point-cloud leveling card is missing.");
    level_group->setChecked(true);
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
    const QRect cloud_view_rect = point_cloud_dialog.cloudWidget()->rect();
    const QRectF point_cloud_selection_rect(cloud_view_rect.adjusted(
        cloud_view_rect.width() * 28 / 100, cloud_view_rect.height() * 24 / 100,
        -cloud_view_rect.width() * 28 / 100, -cloud_view_rect.height() * 24 / 100));
    const QPolygonF free_selection_polygon{
        point_cloud_selection_rect.topLeft(),
        point_cloud_selection_rect.topRight(),
        point_cloud_selection_rect.bottomRight(),
        point_cloud_selection_rect.bottomLeft()};
    const QVector<int> box_selected =
        point_cloud_dialog.cloudWidget()->indicesInScreenPolygon(free_selection_polygon);
    if (box_selected.isEmpty() || box_selected.size() >= point_cloud.Size()) {
        return fail("3D point-cloud free-form selection failed.");
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
    auto* selection_card = point_cloud_dialog.findChild<QGroupBox*>(
        QStringLiteral("PointCloudSelectionGroup"));
    if (!selection_card) return fail("Point-cloud selection card is missing.");
    selection_card->setChecked(true);
    begin_selection->click();
    auto* navigate_button = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudNavigateButton"));
    if (!point_cloud_dialog.cloudWidget()->freeSelectionEnabled() ||
        point_cloud_dialog.measurementMode() != PointCloudMeasureMode::Navigate ||
        !navigate_button || !navigate_button->isChecked()) {
        return fail("Interactive crop button did not enter free-selection mode.");
    }
    const QPointF selection_start = point_cloud_selection_rect.topLeft();
    const QPointF selection_top_right = point_cloud_selection_rect.topRight();
    const QPointF selection_bottom_right = point_cloud_selection_rect.bottomRight();
    const QPointF selection_bottom_left = point_cloud_selection_rect.bottomLeft();
    QMouseEvent selection_press(QEvent::MouseButtonPress,
        selection_start, selection_start, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent selection_move_top(QEvent::MouseMove,
        selection_top_right, selection_top_right, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent selection_move_right(QEvent::MouseMove,
        selection_bottom_right, selection_bottom_right, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent selection_move_bottom(QEvent::MouseMove,
        selection_bottom_left, selection_bottom_left, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent selection_release(QEvent::MouseButtonRelease,
        selection_start, selection_start, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(point_cloud_dialog.cloudWidget(), &selection_press);
    QApplication::sendEvent(point_cloud_dialog.cloudWidget(), &selection_move_top);
    QApplication::sendEvent(point_cloud_dialog.cloudWidget(), &selection_move_right);
    QApplication::sendEvent(point_cloud_dialog.cloudWidget(), &selection_move_bottom);
    QApplication::sendEvent(point_cloud_dialog.cloudWidget(), &selection_release);
    if (drag_selected.size() != box_selected.size() ||
        !point_cloud_dialog.cloudWidget()->freeSelectionEnabled() ||
        !keep_selection->isEnabled() || !remove_selection->isEnabled()) {
        return fail("3D point-cloud free selection did not stay available after selection.");
    }
    const QVector<int> preserved_selection =
        point_cloud_dialog.cloudWidget()->selectionPreviewIndices();
    begin_selection->click();
    if (point_cloud_dialog.cloudWidget()->selectionPreviewIndices() != preserved_selection ||
        !keep_selection->isEnabled() || !clear_selection_action->isEnabled()) {
        return fail("Re-entering free selection discarded the reusable point selection.");
    }
    fit_scope->setCurrentIndex(fit_scope->findData(
        static_cast<int>(PointCloudFitScope::Selection)));
    if (!fit_plane_model_action->isEnabled()) {
        return fail("Selection-scoped plane fitting did not activate for a valid selection.");
    }
    auto* point_cloud_tabs_for_fit = point_cloud_dialog.findChild<QTabWidget*>(
        QStringLiteral("PointCloudToolTabs"));
    fit_plane_model_action->click();
    if (!point_cloud_tabs_for_fit || !point_cloud_tabs_for_fit->isEnabled()) {
        return fail("Background fitting blocked the entire point-cloud tool panel.");
    }
    QElapsedTimer fit_wait_timer;
    fit_wait_timer.start();
    while (point_cloud_dialog.cloudWidget()->geometricModels().empty() &&
        fit_wait_timer.elapsed() < 5000) {
        application.processEvents();
        QThread::msleep(10);
    }
    if (point_cloud_dialog.cloudWidget()->geometricModels().empty() ||
        point_cloud_dialog.cloudWidget()->geometricModels().front().type !=
            PointCloudGeometricModelType::Plane ||
        point_cloud_dialog.cloudWidget()->activeGeometricModel() == 0) {
        return fail("Selection-scoped plane fitting did not create and activate a model.");
    }
    auto* hide_model_action = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudHideModelButton"));
    auto* show_model_action = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudShowModelButton"));
    auto* model_group = point_cloud_dialog.findChild<QGroupBox*>(
        QStringLiteral("PointCloudModelGroup"));
    if (!model_group) return fail("Point-cloud model management card is missing.");
    model_group->setChecked(true);
    if (!hide_model_action || !show_model_action || !hide_model_action->isEnabled() ||
        show_model_action->isEnabled()) {
        return fail("Point-cloud model visibility actions did not reflect the active model.");
    }
    hide_model_action->click();
    if (point_cloud_dialog.cloudWidget()->geometricModels().front().visible ||
        hide_model_action->isEnabled() || !show_model_action->isEnabled()) {
        return fail("Point-cloud model hide action did not update the model and command state.");
    }
    show_model_action->click();
    if (!point_cloud_dialog.cloudWidget()->geometricModels().front().visible) {
        return fail("Point-cloud model show action did not restore the overlay.");
    }
    point_cloud_dialog.setMeasurementMode(PointCloudMeasureMode::Distance);
    auto* measurement_group = point_cloud_dialog.findChild<QGroupBox*>(
        QStringLiteral("PointCloudMeasurementGroup"));
    if (!measurement_group) return fail("Point-cloud measurement card is missing.");
    measurement_group->setChecked(true);
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
    auto* measurement_results = point_cloud_dialog.findChild<QListWidget*>(
        QStringLiteral("PointCloudMeasurementList"));
    auto* measurement_hint = point_cloud_dialog.findChild<QLabel*>(
        QStringLiteral("PointCloudMeasurementHint"));
    if (!measurement_results || measurement_results->currentRow() != 0 ||
        point_cloud_dialog.cloudWidget()->highlightedIndices().size() != 2 ||
        !delete_measurement_action->isEnabled() || !clear_measurements_action->isEnabled() ||
        !export_measurements_action->isEnabled() || !measurement_hint ||
        !measurement_hint->text().contains(QStringLiteral("工具保持启用"))) {
        return fail("Completed measurement was not selected, highlighted, or kept continuous.");
    }
    auto* section_button = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudBeginSectionButton"));
    auto* section_group = point_cloud_dialog.findChild<QGroupBox*>(
        QStringLiteral("PointCloudSectionGroup"));
    if (!section_group) return fail("Point-cloud section analysis card is missing.");
    section_group->setChecked(true);
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
    auto* section_plot = point_cloud_dialog.findChild<PointCloudSectionPlotWidget*>(
        QStringLiteral("PointCloudSectionPlot"));
    auto* section_list = point_cloud_dialog.findChild<QListWidget*>(
        QStringLiteral("PointCloudSectionList"));
    QElapsedTimer section_wait;
    section_wait.start();
    while (section_plot && section_plot->sampleCount() < 8 && section_wait.elapsed() < 5000) {
        application.processEvents();
        QThread::msleep(10);
    }
    if (!section_plot || section_plot->sampleCount() < 8 || !section_list ||
        section_list->count() != 1 ||
        !point_cloud_dialog.grab().save(
            QDir::current().filePath(QStringLiteral("CameraView-point-cloud-section.png")))) {
        return fail("Persistent 3D point-cloud section workspace or plot rendering failed.");
    }
    const int measurements_before_rating = point_cloud_dialog.measurementCount();
    auto* evaluate_tolerances = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudEvaluateTolerancesButton"));
    auto* tolerance_summary = point_cloud_dialog.findChild<QLabel*>(
        QStringLiteral("PointCloudToleranceSummary"));
    auto* tolerance_group = point_cloud_dialog.findChild<QGroupBox*>(
        QStringLiteral("PointCloudToleranceGroup"));
    if (!tolerance_group) return fail("Point-cloud tolerance card is missing.");
    tolerance_group->setChecked(true);
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
        !point_cloud_tabs || point_cloud_tabs->count() != 4 ||
        point_cloud_tabs->tabToolTip(0) != QStringLiteral("数据与显示") ||
        point_cloud_tabs->tabToolTip(2) != QStringLiteral("几何测量")) {
        return fail("Four-stage 3D point-cloud workspace could not be rendered.");
    }
    const QStringList point_cloud_tab_snapshots{
        QStringLiteral("CameraView-point-cloud-dialog-data.png"),
        QStringLiteral("CameraView-point-cloud-dialog-processing.png"),
        QStringLiteral("CameraView-point-cloud-dialog-geometry.png"),
        QStringLiteral("CameraView-point-cloud-dialog-analysis.png")};
    for (int tab_index = 0; tab_index < point_cloud_tabs->count(); ++tab_index) {
        point_cloud_tabs->setCurrentIndex(tab_index);
        application.processEvents();
        if (!point_cloud_dialog.grab().save(
                QDir::current().filePath(point_cloud_tab_snapshots[tab_index]))) {
            return fail("3D point-cloud workbench snapshot could not be rendered.");
        }
    }
    point_cloud_tabs->setCurrentIndex(0);
    point_cloud_dialog.resize(960, 640);
    application.processEvents();
    auto* workspace_splitter = point_cloud_dialog.findChild<QSplitter*>(
        QStringLiteral("PointCloudWorkspaceSplitter"));
    auto* drawer_tabs = point_cloud_dialog.findChild<QTabWidget*>(
        QStringLiteral("PointCloudCompactDrawerTabs"));
    auto* drawer_toggle = point_cloud_dialog.findChild<QToolButton*>(
        QStringLiteral("PointCloudDrawerToggleButton"));
    auto* view_preset = point_cloud_dialog.findChild<QComboBox*>(
        QStringLiteral("PointCloudViewPresetCombo"));
    auto* texture_status = point_cloud_dialog.findChild<QLabel*>(
        QStringLiteral("PointCloudTextureStatus"));
    if (!workspace_splitter || workspace_splitter->orientation() != Qt::Vertical ||
        !drawer_tabs || drawer_tabs->isVisible() || !drawer_toggle || !drawer_toggle->isVisible() ||
        !view_preset || view_preset->isVisible() ||
        !texture_status || texture_status->isVisible() ||
        !point_cloud_dialog.grab().save(QDir::current().filePath(
            QStringLiteral("CameraView-point-cloud-dialog-compact.png")))) {
        return fail("Compact point-cloud workspace did not start with an on-demand drawer.");
    }
    drawer_toggle->click();
    application.processEvents();
    if (!drawer_tabs->isVisible() || !view_preset->isVisible() || view_preset->width() < 80 ||
        !point_cloud_dialog.grab().save(QDir::current().filePath(
            QStringLiteral("CameraView-point-cloud-dialog-compact-drawer.png")))) {
        return fail("Compact point-cloud tool drawer could not be expanded.");
    }
    point_cloud_tabs->setCurrentIndex(1);
    auto* selection_group = point_cloud_dialog.findChild<QGroupBox*>(
        QStringLiteral("PointCloudSelectionGroup"));
    auto* filter_group = point_cloud_dialog.findChild<QGroupBox*>(
        QStringLiteral("PointCloudSmartFilterGroup"));
    if (!selection_group || !filter_group || !selection_group->isChecked() || filter_group->isChecked()) {
        return fail("Point-cloud task page did not preserve single-card expansion.");
    }
    filter_group->setChecked(true);
    application.processEvents();
    if (!filter_group->isChecked() || selection_group->isChecked()) {
        return fail("Point-cloud task page allowed multiple operation cards to remain expanded.");
    }
    auto* delete_model_action = point_cloud_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudDeleteModelButton"));
    model_group->setChecked(true);
    if (!delete_model_action || !delete_model_action->isEnabled() ||
        !point_cloud_dialog.cloudWidget()->fittedPlane().valid) {
        return fail("Active plane model was not synchronized as the measurement reference.");
    }
    delete_model_action->click();
    if (!point_cloud_dialog.cloudWidget()->geometricModels().empty() ||
        point_cloud_dialog.cloudWidget()->fittedPlane().valid ||
        delete_model_action->isEnabled()) {
        return fail("Deleting the active plane model left a stale measurement reference.");
    }

    auto waitForCloudSize = [&application](PointCloudDialog& dialog, std::size_t expected) {
        QElapsedTimer timer;
        timer.start();
        while (dialog.cloud().Size() != expected && timer.elapsed() < 3000) {
            application.processEvents();
            QThread::msleep(10);
        }
        application.processEvents();
        return dialog.cloud().Size() == expected;
    };
    auto selectCropRegion = [&application](PointCloudDialog& dialog) {
        dialog.resize(1180, 760);
        dialog.show();
        application.processEvents();
        auto* begin = dialog.findChild<QPushButton*>(
            QStringLiteral("PointCloudBeginInteractiveCropButton"));
        if (!begin) return QVector<int>{};
        begin->click();
        const QRectF rectangle(dialog.cloudWidget()->rect().adjusted(220, 160, -220, -160));
        const QPolygonF polygon{rectangle.topLeft(), rectangle.topRight(),
            rectangle.bottomRight(), rectangle.bottomLeft()};
        const QVector<int> expected = dialog.cloudWidget()->indicesInScreenPolygon(polygon);
        const std::array<QPointF, 5> positions{rectangle.topLeft(), rectangle.topRight(),
            rectangle.bottomRight(), rectangle.bottomLeft(), rectangle.topLeft()};
        QMouseEvent press(QEvent::MouseButtonPress, positions[0], positions[0],
            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(dialog.cloudWidget(), &press);
        for (int index = 1; index < 4; ++index) {
            QMouseEvent move(QEvent::MouseMove, positions[index], positions[index],
                Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(dialog.cloudWidget(), &move);
        }
        QMouseEvent release(QEvent::MouseButtonRelease, positions[4], positions[4],
            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(dialog.cloudWidget(), &release);
        application.processEvents();
        return expected;
    };
    PointCloudDialog keep_crop_dialog(point_cloud);
    auto* keep_crop_action = keep_crop_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudKeepSelectionButton"));
    const QVector<int> keep_subset = selectCropRegion(keep_crop_dialog);
    if (keep_subset.isEmpty() || !keep_crop_action || !keep_crop_action->isEnabled()) {
        return fail("Point-cloud keep-selection action did not receive the selection.");
    }
    keep_crop_action->click();
    if (!waitForCloudSize(keep_crop_dialog, static_cast<std::size_t>(keep_subset.size()))) {
        std::cerr << "Keep crop expected " << keep_subset.size() << ", actual "
                  << keep_crop_dialog.cloud().Size() << '\n';
        return fail("Point-cloud keep-inside action did not apply the crop.");
    }
    auto* keep_clear_selection = keep_crop_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudClearSelectionButton"));
    if (!keep_crop_dialog.cloudWidget()->selectionPreviewIndices().isEmpty() ||
        !keep_clear_selection || keep_clear_selection->isEnabled()) {
        return fail("Point-cloud data changes did not invalidate the old point selection.");
    }
    PointCloudDialog remove_crop_dialog(point_cloud);
    auto* remove_crop_action = remove_crop_dialog.findChild<QPushButton*>(
        QStringLiteral("PointCloudRemoveSelectionButton"));
    const QVector<int> remove_subset = selectCropRegion(remove_crop_dialog);
    if (remove_subset.isEmpty() || !remove_crop_action || !remove_crop_action->isEnabled()) {
        return fail("Point-cloud remove-selection action did not receive the selection.");
    }
    remove_crop_action->click();
    if (!waitForCloudSize(remove_crop_dialog,
            point_cloud.Size() - static_cast<std::size_t>(remove_subset.size()))) {
        std::cerr << "Remove crop expected "
                  << point_cloud.Size() - static_cast<std::size_t>(remove_subset.size())
                  << ", actual " << remove_crop_dialog.cloud().Size() << '\n';
        return fail("Point-cloud keep-outside action did not apply the crop.");
    }
    return 0;
}

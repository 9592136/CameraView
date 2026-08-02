#include "qt/ImageSurface3DDialog.h"
#include "qt/ImageSurface3DWidget.h"
#include "qt/ProfileAnalysisDialog.h"
#include "qt/ProfilePlotWidget.h"
#include "qt/CameraViewTheme.h"

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QPixmap>

#include <cmath>
#include <cstring>
#include <iostream>

namespace {

int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
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
    surface.setResolution(64);
    surface.setVerticalScale(1.6);
    surface.show();
    application.processEvents();
    if (!surface.hasSurface() || surface.gridSize().width() != 64 || surface.gridSize().height() < 40) {
        return fail("3D surface grid was not generated.");
    }
    const QImage surface_snapshot = surface.grab().toImage();
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
    if (!surface_dialog.surfaceWidget()->hasSurface()) {
        return fail("3D dialog did not receive the image surface.");
    }
    surface_dialog.resize(1040, 720);
    surface_dialog.show();
    application.processEvents();
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
    return 0;
}

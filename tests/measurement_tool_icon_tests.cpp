#include "qt/MeasurementToolButton.h"

#include <QApplication>
#include <QImage>

#include <cstdint>
#include <iostream>
#include <set>
#include <vector>

namespace {

int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
}

std::uint64_t imageSignature(const QImage& image)
{
    constexpr std::uint64_t offset = 1469598103934665603ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = offset;
    for (int y = 0; y < image.height(); ++y) {
        const auto* row = reinterpret_cast<const unsigned char*>(image.constScanLine(y));
        for (qsizetype index = 0; index < image.bytesPerLine(); ++index) {
            hash ^= row[index];
            hash *= prime;
        }
    }
    return hash;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    const std::vector<MeasurementToolGlyph> glyphs{
        MeasurementToolGlyph::Calibration,
        MeasurementToolGlyph::Point,
        MeasurementToolGlyph::Length,
        MeasurementToolGlyph::Profile,
        MeasurementToolGlyph::Angle,
        MeasurementToolGlyph::Rectangle,
        MeasurementToolGlyph::Polygon,
        MeasurementToolGlyph::Polyline,
        MeasurementToolGlyph::Circle,
        MeasurementToolGlyph::Ellipse,
        MeasurementToolGlyph::SmartCount,
        MeasurementToolGlyph::SmartCountRun,
        MeasurementToolGlyph::EdgeSnap,
        MeasurementToolGlyph::DeleteMeasurement,
        MeasurementToolGlyph::ClearMeasurements,
        MeasurementToolGlyph::ExportCsv};

    std::set<std::uint64_t> signatures;
    for (const MeasurementToolGlyph glyph : glyphs) {
        const QIcon icon = measurementToolIcon(glyph);
        const QPixmap pixmap = icon.pixmap(QSize(48, 48), QIcon::Normal, QIcon::Off);
        if (icon.isNull() || pixmap.isNull()) return fail("A measurement tool icon was not rendered.");
        const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
        int visible_pixels = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (qAlpha(image.pixel(x, y)) > 16) ++visible_pixels;
            }
        }
        if (visible_pixels < 40) return fail("A measurement tool icon contains too little visible geometry.");
        signatures.insert(imageSignature(image));
    }
    if (signatures.size() != glyphs.size()) {
        return fail("Measurement functions did not produce unique icon graphics.");
    }
    const QIcon length_icon = measurementToolIcon(MeasurementToolGlyph::Length);
    const std::uint64_t normal_signature = imageSignature(
        length_icon.pixmap(QSize(48, 48), QIcon::Normal, QIcon::Off).toImage());
    const std::uint64_t active_signature = imageSignature(
        length_icon.pixmap(QSize(48, 48), QIcon::Selected, QIcon::On).toImage());
    if (normal_signature == active_signature) {
        return fail("Measurement tool icons did not adapt to their selected state.");
    }

    MeasurementToolButton button(
        MeasurementToolGlyph::Angle, QStringLiteral("角度"), QStringLiteral("角度测量"));
    if (button.glyph() != MeasurementToolGlyph::Angle || !button.isCheckable() ||
        !button.autoExclusive() || button.toolButtonStyle() != Qt::ToolButtonTextUnderIcon ||
        button.icon().isNull() || button.accessibleName() != QStringLiteral("角度") ||
        button.property("role").toString() != QStringLiteral("measurementTool")) {
        return fail("MeasurementToolButton did not expose the expected graphical-button behavior.");
    }
    return 0;
}

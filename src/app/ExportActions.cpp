#include "ExportActions.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "../storage/ImageExporter.h"
#include "../storage/MeasurementCsvExporter.h"

#include <fstream>
#include <sstream>
#include <utility>

namespace {

std::wstring MetadataValueOrUnknown(const std::wstring& value)
{
    return value.empty() ? L"Unknown" : value;
}

std::wstring BuildStitchMetadataText(const StitchResultMetadata& metadata)
{
    std::wostringstream output;
    output << L"CameraView Stitch Metadata\n";
    output << L"Backend: " << MetadataValueOrUnknown(metadata.backend) << L"\n";
    output << L"Layout: " << MetadataValueOrUnknown(metadata.layout_mode) << L"\n";
    output << L"Registration: " << MetadataValueOrUnknown(metadata.registration_method) << L"\n";
    output << L"Transform: " << MetadataValueOrUnknown(metadata.transform_model) << L"\n";
    output << L"Blend: " << MetadataValueOrUnknown(metadata.blend_mode) << L"\n";
    output << L"Overlap percent: " << metadata.overlap_percent << L"\n";
    output << L"Grid rows: " << metadata.grid_rows << L"\n";
    output << L"Grid columns: " << metadata.grid_cols << L"\n";
    output << L"Relations: " << metadata.relation_count << L"\n";
    output << L"Tiles: " << metadata.tiles.size() << L"\n";
    output << L"Tile positions:\n";
    for (std::size_t index = 0; index < metadata.tiles.size(); ++index) {
        const StitchResultTileMetadata& tile = metadata.tiles[index];
        output << L"  "
               << (index + 1U)
               << L": "
               << tile.width
               << L"x"
               << tile.height
               << L" @ "
               << tile.offset_x
               << L","
               << tile.offset_y;
        if (tile.estimated_position) {
            output << L" estimated";
        }
        output << L"\n";
    }
    return output.str();
}

} // namespace

ExportActionResult ExportActions::SaveMeasurementsCsv(
    const std::filesystem::path& path,
    const MeasurementCollection& measurements,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit,
    const std::wstring& objective_label)
{
    if (measurements.Empty()) {
        return {false, ExportActionStatus::NoMeasurements, L"No measurements to export."};
    }

    std::wstring error;
    if (!MeasurementCsvExporter::Save(path, measurements, calibration, display_unit, objective_label, error)) {
        return {
            false,
            ExportActionStatus::WriteFailed,
            error.empty() ? L"Failed to export CSV file." : error};
    }

    return {true, ExportActionStatus::Saved, L"CSV exported."};
}

ExportActionResult ExportActions::SaveImageBmp(
    const std::filesystem::path& path,
    const ImageFrame& frame,
    const MeasurementCollection& measurements,
    const std::wstring& display_mode,
    const CalibrationProfile* calibration)
{
    return SaveImage(path, frame, measurements, display_mode, calibration);
}

ExportActionResult ExportActions::SaveImage(
    const std::filesystem::path& path,
    const ImageFrame& frame,
    const MeasurementCollection& measurements,
    const std::wstring& display_mode,
    const CalibrationProfile* calibration)
{
    if (!frame.IsValid()) {
        return {false, ExportActionStatus::NoImageFrame, L"No image frame to export."};
    }

    std::wstring error;
    if (!ImageExporter::SaveRasterImage(
            path,
            frame,
            measurements.Lengths(),
            measurements.Angles(),
            measurements.Rectangles(),
            measurements.Polygons(),
            error,
            calibration)) {
        return {
            false,
            ExportActionStatus::WriteFailed,
            error.empty() ? L"Failed to export image." : error};
    }

    std::wstring message =
        L"Image exported: " + std::to_wstring(frame.width) + L"x" + std::to_wstring(frame.height);
    if (!display_mode.empty()) {
        message += L" (" + display_mode + L")";
    }
    message += L".";
    return {true, ExportActionStatus::Saved, std::move(message)};
}

ExportActionResult ExportActions::SaveStitchMetadata(
    const std::filesystem::path& path,
    const StitchResultMetadata& metadata)
{
    if (!metadata.available) {
        return {false, ExportActionStatus::NoStitchMetadata, L"No stitch metadata to save."};
    }

    return SaveUtf8TextFile(
        path,
        BuildStitchMetadataText(metadata),
        L"Failed to create stitch metadata.",
        L"Failed while writing stitch metadata.",
        L"Stitch metadata saved.");
}

ExportActionResult ExportActions::SaveDiagnosticReport(
    const std::filesystem::path& path,
    const std::wstring& report)
{
    return SaveUtf8TextFile(
        path,
        report,
        L"Failed to create diagnostic report.",
        L"Failed while writing diagnostic report.",
        L"Diagnostic report saved.");
}

ExportActionResult ExportActions::SaveReportHtml(
    const std::filesystem::path& path,
    const std::wstring& report)
{
    return SaveUtf8TextFile(
        path,
        report,
        L"Failed to create report.",
        L"Failed while writing report.",
        L"Report saved.");
}

ExportActionResult ExportActions::SaveReportTemplate(
    const std::filesystem::path& path,
    const std::wstring& template_text)
{
    return SaveUtf8TextFile(
        path,
        template_text,
        L"Failed to create report template.",
        L"Failed while writing report template.",
        L"Report template saved.");
}

ExportActionResult ExportActions::SaveUtf8TextFile(
    const std::filesystem::path& path,
    const std::wstring& text,
    const std::wstring& create_error,
    const std::wstring& write_error,
    const std::wstring& success_message)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return {false, ExportActionStatus::WriteFailed, create_error};
    }

    output << "\xEF\xBB\xBF";
    output << Utf8FromWide(text);
    if (!output) {
        return {false, ExportActionStatus::WriteFailed, write_error};
    }

    return {true, ExportActionStatus::Saved, success_message};
}

std::string ExportActions::Utf8FromWide(const std::wstring& text)
{
    if (text.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (size <= 0) {
        return {};
    }

    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        size,
        nullptr,
        nullptr);
    return result;
}

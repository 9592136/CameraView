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
#include <utility>

ExportActionResult ExportActions::SaveMeasurementsCsv(
    const std::filesystem::path& path,
    const MeasurementCollection& measurements,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit)
{
    if (measurements.Empty()) {
        return {false, ExportActionStatus::NoMeasurements, L"No measurements to export."};
    }

    std::wstring error;
    if (!MeasurementCsvExporter::Save(path, measurements, calibration, display_unit, error)) {
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
    const std::wstring& display_mode)
{
    return SaveImage(path, frame, measurements, display_mode);
}

ExportActionResult ExportActions::SaveImage(
    const std::filesystem::path& path,
    const ImageFrame& frame,
    const MeasurementCollection& measurements,
    const std::wstring& display_mode)
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
            error)) {
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

ExportActionResult ExportActions::SaveDiagnosticReport(
    const std::filesystem::path& path,
    const std::wstring& report)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return {false, ExportActionStatus::WriteFailed, L"Failed to create diagnostic report."};
    }

    output << "\xEF\xBB\xBF";
    output << Utf8FromWide(report);
    if (!output) {
        return {false, ExportActionStatus::WriteFailed, L"Failed while writing diagnostic report."};
    }

    return {true, ExportActionStatus::Saved, L"Diagnostic report saved."};
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

#pragma once

#include "../domain/CalibrationProfile.h"
#include "../domain/ImageFrame.h"
#include "../domain/MeasurementCollection.h"

#include <filesystem>
#include <string>

enum class ExportActionStatus {
    Saved,
    NoMeasurements,
    NoImageFrame,
    WriteFailed
};

struct ExportActionResult {
    bool saved = false;
    ExportActionStatus status = ExportActionStatus::WriteFailed;
    std::wstring message;
};

class ExportActions {
public:
    static ExportActionResult SaveMeasurementsCsv(
        const std::filesystem::path& path,
        const MeasurementCollection& measurements,
        const CalibrationProfile& calibration,
        MeasurementUnit display_unit,
        const std::wstring& objective_label = L"");

    static ExportActionResult SaveImageBmp(
        const std::filesystem::path& path,
        const ImageFrame& frame,
        const MeasurementCollection& measurements,
        const std::wstring& display_mode = L"",
        const CalibrationProfile* calibration = nullptr);

    static ExportActionResult SaveImage(
        const std::filesystem::path& path,
        const ImageFrame& frame,
        const MeasurementCollection& measurements,
        const std::wstring& display_mode = L"",
        const CalibrationProfile* calibration = nullptr);

    static ExportActionResult SaveDiagnosticReport(
        const std::filesystem::path& path,
        const std::wstring& report);

    static ExportActionResult SaveReportHtml(
        const std::filesystem::path& path,
        const std::wstring& report);

    static ExportActionResult SaveReportTemplate(
        const std::filesystem::path& path,
        const std::wstring& template_text);

private:
    static ExportActionResult SaveUtf8TextFile(
        const std::filesystem::path& path,
        const std::wstring& text,
        const std::wstring& create_error,
        const std::wstring& write_error,
        const std::wstring& success_message);
    static std::string Utf8FromWide(const std::wstring& text);
};

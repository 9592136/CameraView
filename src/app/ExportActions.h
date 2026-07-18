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
        MeasurementUnit display_unit);

    static ExportActionResult SaveImageBmp(
        const std::filesystem::path& path,
        const ImageFrame& frame,
        const MeasurementCollection& measurements,
        const std::wstring& display_mode = L"");

    static ExportActionResult SaveImage(
        const std::filesystem::path& path,
        const ImageFrame& frame,
        const MeasurementCollection& measurements,
        const std::wstring& display_mode = L"");

    static ExportActionResult SaveDiagnosticReport(
        const std::filesystem::path& path,
        const std::wstring& report);

private:
    static std::string Utf8FromWide(const std::wstring& text);
};

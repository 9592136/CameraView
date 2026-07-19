#pragma once

#include "../camera/ICameraDriver.h"
#include "../domain/CalibrationProfile.h"
#include "../domain/ImageFrame.h"
#include "../domain/MeasurementCollection.h"
#include "../imaging/PseudoColorMapper.h"
#include "../storage/DiagnosticReportBuilder.h"

#include <cstddef>
#include <string>
#include <vector>

struct DiagnosticReportActionInput {
    DiagnosticReportTimestamp generated;
    std::wstring status;
    std::wstring preview_telemetry;
    std::wstring viewport_zoom;
    bool preview_running = false;
    bool processing_running = false;
    CameraSdkDiagnostics sdk;
    int enumerated_cameras = 0;
    int selected_camera_index = -1;
    std::vector<CameraDevice> enumerated_devices;
    std::wstring latest_frame_source;
    ImageFrame latest_frame;
    std::wstring objective_label;
    CalibrationProfile calibration = CalibrationProfile::Uncalibrated();
    MeasurementUnit display_unit = MeasurementUnit::Pixels;
    std::wstring preview_display_mode;
    PseudoColorPalette pseudo_color = PseudoColorPalette::Original;
    std::size_t dye_profiles = 0;
    std::size_t fluorescence_channels = 0;
    std::size_t stitch_tiles = 0;
    int stitch_search_percent = 0;
    std::size_t edf_frames = 0;
    int edf_focus_radius = 1;
    bool processing_result_visible = false;
    std::wstring processing_result_kind;
    std::wstring processing_result_source;
    int processing_result_width = 0;
    int processing_result_height = 0;
    bool edf_composite_available = false;
    bool edf_focus_map_available = false;
};

enum class ImageReportTemplateSection {
    CurrentImage,
    ReportInformation,
    ReportNotes,
    MeasurementSummary,
    MeasurementTable,
    ImageDetails
};

enum class ImageReportTemplateAccent {
    Blue,
    Green,
    Gold,
    Magenta
};

enum class ImageReportTemplateImageSize {
    Original,
    FitPage,
    Compact
};

enum class ImageReportTemplatePageLayout {
    Standard,
    Wide,
    Compact
};

enum class ImageReportTemplatePrintOrientation {
    Portrait,
    Landscape
};

enum class ImageReportTemplateMeasurementPrecision {
    Automatic,
    TwoDecimals,
    ThreeDecimals
};

struct ImageReportTemplateOptions {
    std::wstring title = L"CameraView Image Report";
    std::wstring subtitle;
    ImageReportTemplateAccent accent = ImageReportTemplateAccent::Blue;
    ImageReportTemplateImageSize image_size = ImageReportTemplateImageSize::Original;
    ImageReportTemplatePageLayout page_layout = ImageReportTemplatePageLayout::Standard;
    ImageReportTemplatePrintOrientation print_orientation =
        ImageReportTemplatePrintOrientation::Portrait;
    ImageReportTemplateMeasurementPrecision measurement_precision =
        ImageReportTemplateMeasurementPrecision::Automatic;
    std::wstring image_caption;
    std::wstring current_image_heading;
    std::wstring report_information_heading;
    std::wstring notes_heading;
    std::wstring measurement_summary_heading;
    std::wstring measurement_table_heading;
    std::wstring image_details_heading;
    std::wstring footer_text;
    bool show_image = true;
    bool show_report_information = false;
    bool show_notes = false;
    bool show_measurement_summary = true;
    bool show_measurement_table = true;
    bool show_measurement_raw_values = true;
    bool group_measurements_by_type = false;
    bool show_calibration_details = true;
    bool show_processing_details = true;
    bool show_footer = true;
    std::wstring report_information_fields;
    std::wstring notes;
    std::vector<ImageReportTemplateSection> section_order;
};

class DiagnosticReportActions {
public:
    static std::wstring DefaultImageReportTemplate();
    static std::wstring BuildImageReportTemplate(const ImageReportTemplateOptions& options);
    static bool TryParseImageReportTemplateOptions(
        const std::wstring& template_text,
        ImageReportTemplateOptions& options);

    static std::wstring BuildReport(
        DiagnosticReportActionInput input,
        const MeasurementCollection& measurements,
        const std::wstring& template_text = L"");

    static std::wstring BuildImageReport(
        DiagnosticReportActionInput input,
        const MeasurementCollection& measurements,
        const std::wstring& image_file_name,
        const ImageFrame& report_image,
        const std::wstring& template_text = L"");

    static std::wstring BuildSdkTelemetry(const CameraSdkDiagnostics& diagnostics);
};

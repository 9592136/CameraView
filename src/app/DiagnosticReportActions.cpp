#include "DiagnosticReportActions.h"

#include "../domain/MeasurementFormatter.h"

#include <algorithm>
#include <cwchar>
#include <iomanip>
#include <sstream>
#include <utility>

namespace {

std::wstring CameraDeviceDisplayName(const CameraDevice& device)
{
    if (!device.display_name.empty()) {
        return device.display_name;
    }
    return L"Device " + std::to_wstring(device.index + 1);
}

std::wstring CameraDeviceSummary(const CameraDevice& device)
{
    return CameraDeviceDisplayName(device) + L" | type " + std::to_wstring(device.type);
}

std::wstring SelectedCameraSummary(const DiagnosticReportInput& input)
{
    if (input.selected_camera_index >= 0 &&
        static_cast<std::size_t>(input.selected_camera_index) < input.enumerated_devices.size()) {
        return CameraDeviceSummary(input.enumerated_devices[static_cast<std::size_t>(input.selected_camera_index)]);
    }
    return L"(none)";
}

std::wstring FrameSize(const ImageFrame& frame)
{
    if (!frame.IsValid()) {
        return L"(none)";
    }
    return std::to_wstring(frame.width) + L"x" + std::to_wstring(frame.height);
}

std::wstring ProcessingResultSize(const DiagnosticImageProcessingSummary& image_processing)
{
    if (image_processing.processing_result_width <= 0 ||
        image_processing.processing_result_height <= 0) {
        return L"(none)";
    }
    return std::to_wstring(image_processing.processing_result_width) +
        L"x" + std::to_wstring(image_processing.processing_result_height);
}

std::wstring FormatTimestamp(const DiagnosticReportTimestamp& timestamp)
{
    std::wostringstream stream;
    stream << timestamp.year << L"-" << std::setw(2) << std::setfill(L'0') << timestamp.month
           << L"-" << std::setw(2) << timestamp.day
           << L" " << std::setw(2) << timestamp.hour
           << L":" << std::setw(2) << timestamp.minute
           << L":" << std::setw(2) << timestamp.second
           << std::setfill(L' ');
    return stream.str();
}

std::wstring FormatDouble(double value, int precision)
{
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

const wchar_t* YesNo(bool value)
{
    return value ? L"Yes" : L"No";
}

void ReplaceAll(std::wstring& text, const std::wstring& token, const std::wstring& value)
{
    std::size_t position = 0;
    while ((position = text.find(token, position)) != std::wstring::npos) {
        text.replace(position, token.size(), value);
        position += value.size();
    }
}

std::wstring HtmlEscape(const std::wstring& text)
{
    std::wstring escaped;
    escaped.reserve(text.size());
    for (wchar_t value : text) {
        switch (value) {
        case L'&':
            escaped += L"&amp;";
            break;
        case L'<':
            escaped += L"&lt;";
            break;
        case L'>':
            escaped += L"&gt;";
            break;
        case L'"':
            escaped += L"&quot;";
            break;
        case L'\'':
            escaped += L"&#39;";
            break;
        default:
            escaped.push_back(value);
            break;
        }
    }
    return escaped;
}

std::wstring MultilineHtml(const std::wstring& text)
{
    std::wstring output = HtmlEscape(text);
    ReplaceAll(output, L"\r\n", L"\n");
    ReplaceAll(output, L"\r", L"\n");
    ReplaceAll(output, L"\n", L"<br>\n");
    return output;
}

int MeasurementValuePrecision(
    const MeasurementResult& result,
    ImageReportTemplateMeasurementPrecision precision)
{
    if (precision == ImageReportTemplateMeasurementPrecision::TwoDecimals) {
        return 2;
    }
    if (precision == ImageReportTemplateMeasurementPrecision::ThreeDecimals) {
        return 3;
    }
    if (result.kind == L"Angle") {
        return 2;
    }
    return result.unit == MeasurementUnit::Pixels ? 1 : 2;
}

void AppendMeasurementRow(
    std::wostringstream& table,
    int index,
    const MeasurementResult& result,
    ImageReportTemplateMeasurementPrecision precision,
    bool show_raw_values)
{
    table << L"<tr><td>" << index << L"</td><td>" << HtmlEscape(result.name)
          << L"</td><td>" << HtmlEscape(result.kind)
          << L"</td><td>" << FormatDouble(result.calibrated_value, MeasurementValuePrecision(result, precision))
          << L"</td><td>" << HtmlEscape(result.unit_label);
    if (show_raw_values) {
        table << L"</td><td>" << FormatDouble(result.pixel_value, 2);
    }
    table << L"</td></tr>\n";
}

void AppendMeasurementGroupRow(
    std::wostringstream& table,
    const std::wstring& label,
    bool show_raw_values)
{
    table << L"<tr class=\"measurement-group\"><th colspan=\""
          << (show_raw_values ? 6 : 5) << L"\">"
          << HtmlEscape(label) << L"</th></tr>\n";
}

std::wstring MeasurementTableHtml(
    const MeasurementCollection& measurements,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit,
    ImageReportTemplateMeasurementPrecision precision,
    bool show_raw_values,
    bool group_by_type)
{
    if (measurements.Empty()) {
        return L"<p class=\"empty\">No measurements.</p>";
    }

    std::wostringstream table;
    table << L"<table class=\"measurement-table\">\n";
    table << L"<thead><tr><th>#</th><th>Name</th><th>Type</th><th>Value</th><th>Unit</th>";
    if (show_raw_values) {
        table << L"<th>Raw</th>";
    }
    table << L"</tr></thead>\n";
    table << L"<tbody>\n";

    int index = 1;
    const auto& lengths = measurements.Lengths();
    if (group_by_type && !lengths.empty()) {
        AppendMeasurementGroupRow(table, L"Length measurements", show_raw_values);
    }
    for (const LengthMeasurement& measurement : lengths) {
        AppendMeasurementRow(
            table,
            index++,
            measurement.Evaluate(calibration, display_unit),
            precision,
            show_raw_values);
    }

    const auto& angles = measurements.Angles();
    if (group_by_type && !angles.empty()) {
        AppendMeasurementGroupRow(table, L"Angle measurements", show_raw_values);
    }
    for (const AngleMeasurement& measurement : angles) {
        AppendMeasurementRow(table, index++, measurement.Evaluate(), precision, show_raw_values);
    }

    const auto& rectangles = measurements.Rectangles();
    if (group_by_type && !rectangles.empty()) {
        AppendMeasurementGroupRow(table, L"Rectangle area measurements", show_raw_values);
    }
    for (const RectangleAreaMeasurement& measurement : rectangles) {
        AppendMeasurementRow(
            table,
            index++,
            measurement.Evaluate(calibration, display_unit),
            precision,
            show_raw_values);
    }

    const auto& polygons = measurements.Polygons();
    if (group_by_type && !polygons.empty()) {
        AppendMeasurementGroupRow(table, L"Polygon area measurements", show_raw_values);
    }
    for (const PolygonAreaMeasurement& measurement : polygons) {
        AppendMeasurementRow(
            table,
            index++,
            measurement.Evaluate(calibration, display_unit),
            precision,
            show_raw_values);
    }

    table << L"</tbody></table>";
    return table.str();
}

std::wstring MeasurementLinesHtml(
    const MeasurementCollection& measurements,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit)
{
    const std::vector<std::wstring> lines =
        MeasurementFormatter::FormatCollection(measurements, calibration, display_unit);
    if (lines.empty()) {
        return L"<p class=\"empty\">No measurements.</p>";
    }

    std::wostringstream output;
    output << L"<ul class=\"measurement-lines\">";
    for (const std::wstring& line : lines) {
        output << L"<li>" << HtmlEscape(line) << L"</li>";
    }
    output << L"</ul>";
    return output.str();
}

std::wstring TrimText(const std::wstring& text);

std::wstring ReportInformationFieldsHtml(const std::wstring& fields)
{
    if (TrimText(fields).empty()) {
        return L"<p class=\"empty\">No report information.</p>";
    }

    std::wostringstream table;
    table << L"<table class=\"field-table report-fields\">\n";

    int row = 1;
    std::size_t line_start = 0;
    while (line_start <= fields.size()) {
        const std::size_t line_end = fields.find(L'\n', line_start);
        const std::wstring raw_line = fields.substr(
            line_start,
            line_end == std::wstring::npos ? std::wstring::npos : line_end - line_start);
        const std::wstring line = TrimText(raw_line);
        if (!line.empty()) {
            const std::size_t equals = line.find(L'=');
            std::wstring label;
            std::wstring value;
            if (equals == std::wstring::npos) {
                label = L"Field " + std::to_wstring(row);
                value = line;
            } else {
                label = TrimText(line.substr(0, equals));
                value = TrimText(line.substr(equals + 1U));
                if (label.empty()) {
                    label = L"Field " + std::to_wstring(row);
                }
            }
            table << L"<tr><th>" << HtmlEscape(label) << L"</th><td>"
                  << MultilineHtml(value.empty() ? L"(empty)" : value) << L"</td></tr>\n";
            ++row;
        }
        if (line_end == std::wstring::npos) {
            break;
        }
        line_start = line_end + 1U;
    }

    if (row == 1) {
        return L"<p class=\"empty\">No report information.</p>";
    }

    table << L"</table>";
    return table.str();
}

std::wstring MeasurementSummaryHtml(const DiagnosticMeasurementSummary& measurement)
{
    std::wostringstream summary;
    summary << L"<dl class=\"summary-grid\">";
    summary << L"<dt>Objective</dt><dd>"
            << HtmlEscape(measurement.objective_label.empty() ? L"(none)" : measurement.objective_label)
            << L"</dd>";
    summary << L"<dt>Calibration</dt><dd>"
            << (measurement.calibrated
                    ? HtmlEscape(FormatDouble(measurement.microns_per_pixel, 8) + L" um/px")
                    : L"Uncalibrated")
            << L"</dd>";
    summary << L"<dt>Display unit</dt><dd>"
            << HtmlEscape(CalibrationProfile::UnitLabel(measurement.display_unit))
            << L"</dd>";
    summary << L"<dt>Total</dt><dd>" << measurement.total_measurements << L"</dd>";
    summary << L"<dt>Lengths</dt><dd>" << measurement.length_measurements << L"</dd>";
    summary << L"<dt>Angles</dt><dd>" << measurement.angle_measurements << L"</dd>";
    summary << L"<dt>Rectangle areas</dt><dd>" << measurement.rectangle_area_measurements << L"</dd>";
    summary << L"<dt>Polygon areas</dt><dd>" << measurement.polygon_area_measurements << L"</dd>";
    summary << L"</dl>";
    return summary.str();
}

std::wstring ImageTagHtml(
    const std::wstring& image_file_name,
    const ImageFrame& report_image)
{
    if (image_file_name.empty() || !report_image.IsValid()) {
        return L"<p class=\"empty\">No report image.</p>";
    }

    return L"<img class=\"report-image\" src=\"" + HtmlEscape(image_file_name) +
        L"\" alt=\"Current image with measurement overlay\">";
}

std::wstring TrimText(const std::wstring& text)
{
    const std::wstring whitespace = L" \t\r\n";
    const std::size_t first = text.find_first_not_of(whitespace);
    if (first == std::wstring::npos) {
        return {};
    }
    const std::size_t last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1U);
}

std::wstring MetadataEscape(const std::wstring& value)
{
    std::wstring output;
    output.reserve(value.size());
    for (wchar_t character : value) {
        switch (character) {
        case L'\\':
            output += L"\\\\";
            break;
        case L'\r':
            output += L"\\r";
            break;
        case L'\n':
            output += L"\\n";
            break;
        default:
            output.push_back(character);
            break;
        }
    }
    ReplaceAll(output, L"--", L"-\\-");
    return output;
}

std::wstring MetadataUnescape(std::wstring value)
{
    ReplaceAll(value, L"-\\-", L"--");

    std::wstring output;
    output.reserve(value.size());
    bool escaped = false;
    for (wchar_t character : value) {
        if (!escaped) {
            if (character == L'\\') {
                escaped = true;
            } else {
                output.push_back(character);
            }
            continue;
        }

        switch (character) {
        case L'n':
            output.push_back(L'\n');
            break;
        case L'r':
            output.push_back(L'\r');
            break;
        case L'\\':
            output.push_back(L'\\');
            break;
        default:
            output.push_back(character);
            break;
        }
        escaped = false;
    }
    if (escaped) {
        output.push_back(L'\\');
    }
    return output;
}

const wchar_t* AccentToken(ImageReportTemplateAccent accent)
{
    switch (accent) {
    case ImageReportTemplateAccent::Blue:
        return L"blue";
    case ImageReportTemplateAccent::Green:
        return L"green";
    case ImageReportTemplateAccent::Gold:
        return L"gold";
    case ImageReportTemplateAccent::Magenta:
        return L"magenta";
    default:
        return L"blue";
    }
}

bool AccentFromToken(const std::wstring& token, ImageReportTemplateAccent& accent)
{
    if (token == L"blue") {
        accent = ImageReportTemplateAccent::Blue;
        return true;
    }
    if (token == L"green") {
        accent = ImageReportTemplateAccent::Green;
        return true;
    }
    if (token == L"gold") {
        accent = ImageReportTemplateAccent::Gold;
        return true;
    }
    if (token == L"magenta") {
        accent = ImageReportTemplateAccent::Magenta;
        return true;
    }
    return false;
}

const wchar_t* AccentColor(ImageReportTemplateAccent accent)
{
    switch (accent) {
    case ImageReportTemplateAccent::Blue:
        return L"#5b86a3";
    case ImageReportTemplateAccent::Green:
        return L"#4f8a69";
    case ImageReportTemplateAccent::Gold:
        return L"#a3772f";
    case ImageReportTemplateAccent::Magenta:
        return L"#a45a7a";
    default:
        return L"#5b86a3";
    }
}

const wchar_t* AccentSoftColor(ImageReportTemplateAccent accent)
{
    switch (accent) {
    case ImageReportTemplateAccent::Blue:
        return L"#edf3f7";
    case ImageReportTemplateAccent::Green:
        return L"#edf6f1";
    case ImageReportTemplateAccent::Gold:
        return L"#f8f2e6";
    case ImageReportTemplateAccent::Magenta:
        return L"#f8edf2";
    default:
        return L"#edf3f7";
    }
}

const wchar_t* ImageSizeToken(ImageReportTemplateImageSize image_size)
{
    switch (image_size) {
    case ImageReportTemplateImageSize::Original:
        return L"original";
    case ImageReportTemplateImageSize::FitPage:
        return L"fit";
    case ImageReportTemplateImageSize::Compact:
        return L"compact";
    default:
        return L"original";
    }
}

bool ImageSizeFromToken(const std::wstring& token, ImageReportTemplateImageSize& image_size)
{
    if (token == L"original") {
        image_size = ImageReportTemplateImageSize::Original;
        return true;
    }
    if (token == L"fit") {
        image_size = ImageReportTemplateImageSize::FitPage;
        return true;
    }
    if (token == L"compact") {
        image_size = ImageReportTemplateImageSize::Compact;
        return true;
    }
    return false;
}

const wchar_t* ImageSizeCss(ImageReportTemplateImageSize image_size)
{
    switch (image_size) {
    case ImageReportTemplateImageSize::Original:
        return L"max-width: 100%; width: auto;";
    case ImageReportTemplateImageSize::FitPage:
        return L"max-width: 100%; width: 100%;";
    case ImageReportTemplateImageSize::Compact:
        return L"max-width: min(100%, 720px); width: auto;";
    default:
        return L"max-width: 100%; width: auto;";
    }
}

const wchar_t* PageLayoutToken(ImageReportTemplatePageLayout page_layout)
{
    switch (page_layout) {
    case ImageReportTemplatePageLayout::Standard:
        return L"standard";
    case ImageReportTemplatePageLayout::Wide:
        return L"wide";
    case ImageReportTemplatePageLayout::Compact:
        return L"compact";
    default:
        return L"standard";
    }
}

bool PageLayoutFromToken(const std::wstring& token, ImageReportTemplatePageLayout& page_layout)
{
    if (token == L"standard") {
        page_layout = ImageReportTemplatePageLayout::Standard;
        return true;
    }
    if (token == L"wide") {
        page_layout = ImageReportTemplatePageLayout::Wide;
        return true;
    }
    if (token == L"compact") {
        page_layout = ImageReportTemplatePageLayout::Compact;
        return true;
    }
    return false;
}

const wchar_t* PageLayoutCss(ImageReportTemplatePageLayout page_layout)
{
    switch (page_layout) {
    case ImageReportTemplatePageLayout::Standard:
        return L"max-width: 1120px; margin: 0 auto; padding: 28px;";
    case ImageReportTemplatePageLayout::Wide:
        return L"max-width: 1440px; margin: 0 auto; padding: 30px 34px;";
    case ImageReportTemplatePageLayout::Compact:
        return L"max-width: 920px; margin: 0 auto; padding: 18px 20px;";
    default:
        return L"max-width: 1120px; margin: 0 auto; padding: 28px;";
    }
}

const wchar_t* H1Css(ImageReportTemplatePageLayout page_layout)
{
    return page_layout == ImageReportTemplatePageLayout::Compact
        ? L"font-size: 23px; margin: 0 0 6px; font-weight: 650;"
        : L"font-size: 26px; margin: 0 0 8px; font-weight: 650;";
}

const wchar_t* H2Css(ImageReportTemplatePageLayout page_layout)
{
    return page_layout == ImageReportTemplatePageLayout::Compact
        ? L"font-size: 16px; margin: 20px 0 9px;"
        : L"font-size: 18px; margin: 26px 0 12px;";
}

const wchar_t* TableFontCss(ImageReportTemplatePageLayout page_layout)
{
    return page_layout == ImageReportTemplatePageLayout::Compact
        ? L"12px"
        : L"13px";
}

const wchar_t* TableCellPaddingCss(ImageReportTemplatePageLayout page_layout)
{
    return page_layout == ImageReportTemplatePageLayout::Compact
        ? L"5px 7px"
        : L"7px 9px";
}

const wchar_t* PrintOrientationToken(ImageReportTemplatePrintOrientation orientation)
{
    switch (orientation) {
    case ImageReportTemplatePrintOrientation::Portrait:
        return L"portrait";
    case ImageReportTemplatePrintOrientation::Landscape:
        return L"landscape";
    default:
        return L"portrait";
    }
}

bool PrintOrientationFromToken(
    const std::wstring& token,
    ImageReportTemplatePrintOrientation& orientation)
{
    if (token == L"portrait") {
        orientation = ImageReportTemplatePrintOrientation::Portrait;
        return true;
    }
    if (token == L"landscape") {
        orientation = ImageReportTemplatePrintOrientation::Landscape;
        return true;
    }
    return false;
}

const wchar_t* MeasurementPrecisionToken(ImageReportTemplateMeasurementPrecision precision)
{
    switch (precision) {
    case ImageReportTemplateMeasurementPrecision::Automatic:
        return L"auto";
    case ImageReportTemplateMeasurementPrecision::TwoDecimals:
        return L"2";
    case ImageReportTemplateMeasurementPrecision::ThreeDecimals:
        return L"3";
    default:
        return L"auto";
    }
}

bool MeasurementPrecisionFromToken(
    const std::wstring& token,
    ImageReportTemplateMeasurementPrecision& precision)
{
    if (token == L"auto") {
        precision = ImageReportTemplateMeasurementPrecision::Automatic;
        return true;
    }
    if (token == L"2") {
        precision = ImageReportTemplateMeasurementPrecision::TwoDecimals;
        return true;
    }
    if (token == L"3") {
        precision = ImageReportTemplateMeasurementPrecision::ThreeDecimals;
        return true;
    }
    return false;
}

std::wstring ImageCaptionTemplate(const ImageReportTemplateOptions& options)
{
    const std::wstring caption = TrimText(options.image_caption);
    if (caption.empty()) {
        return L"{{ImageFile}} | {{ImageSize}} | {{PreviewDisplayMode}}";
    }
    return options.image_caption;
}

const wchar_t* DefaultSectionHeading(ImageReportTemplateSection section)
{
    switch (section) {
    case ImageReportTemplateSection::CurrentImage:
        return L"Current Image";
    case ImageReportTemplateSection::ReportInformation:
        return L"Report Information";
    case ImageReportTemplateSection::ReportNotes:
        return L"Notes";
    case ImageReportTemplateSection::MeasurementSummary:
        return L"Measurement Summary";
    case ImageReportTemplateSection::MeasurementTable:
        return L"Measurement Data";
    case ImageReportTemplateSection::ImageDetails:
        return L"Image And Calibration";
    default:
        return L"Report Section";
    }
}

std::wstring SectionHeading(
    const ImageReportTemplateOptions& options,
    ImageReportTemplateSection section)
{
    const std::wstring* heading = nullptr;
    switch (section) {
    case ImageReportTemplateSection::CurrentImage:
        heading = &options.current_image_heading;
        break;
    case ImageReportTemplateSection::ReportInformation:
        heading = &options.report_information_heading;
        break;
    case ImageReportTemplateSection::ReportNotes:
        heading = &options.notes_heading;
        break;
    case ImageReportTemplateSection::MeasurementSummary:
        heading = &options.measurement_summary_heading;
        break;
    case ImageReportTemplateSection::MeasurementTable:
        heading = &options.measurement_table_heading;
        break;
    case ImageReportTemplateSection::ImageDetails:
        heading = &options.image_details_heading;
        break;
    default:
        break;
    }

    if (heading && !TrimText(*heading).empty()) {
        return *heading;
    }
    return DefaultSectionHeading(section);
}

std::wstring FooterTextTemplate(const ImageReportTemplateOptions& options)
{
    const std::wstring footer_text = TrimText(options.footer_text);
    if (footer_text.empty()) {
        return L"Generated by CameraView.";
    }
    return options.footer_text;
}

const wchar_t* SectionToken(ImageReportTemplateSection section)
{
    switch (section) {
    case ImageReportTemplateSection::CurrentImage:
        return L"image";
    case ImageReportTemplateSection::ReportInformation:
        return L"info";
    case ImageReportTemplateSection::ReportNotes:
        return L"notes";
    case ImageReportTemplateSection::MeasurementSummary:
        return L"summary";
    case ImageReportTemplateSection::MeasurementTable:
        return L"table";
    case ImageReportTemplateSection::ImageDetails:
        return L"details";
    default:
        return L"details";
    }
}

bool SectionFromToken(const std::wstring& token, ImageReportTemplateSection& section)
{
    if (token == L"image") {
        section = ImageReportTemplateSection::CurrentImage;
        return true;
    }
    if (token == L"info") {
        section = ImageReportTemplateSection::ReportInformation;
        return true;
    }
    if (token == L"notes") {
        section = ImageReportTemplateSection::ReportNotes;
        return true;
    }
    if (token == L"summary") {
        section = ImageReportTemplateSection::MeasurementSummary;
        return true;
    }
    if (token == L"table") {
        section = ImageReportTemplateSection::MeasurementTable;
        return true;
    }
    if (token == L"details") {
        section = ImageReportTemplateSection::ImageDetails;
        return true;
    }
    return false;
}

std::vector<ImageReportTemplateSection> DefaultSectionOrder()
{
    return {
        ImageReportTemplateSection::CurrentImage,
        ImageReportTemplateSection::ReportInformation,
        ImageReportTemplateSection::ReportNotes,
        ImageReportTemplateSection::MeasurementSummary,
        ImageReportTemplateSection::MeasurementTable,
        ImageReportTemplateSection::ImageDetails};
}

bool ContainsSection(
    const std::vector<ImageReportTemplateSection>& sections,
    ImageReportTemplateSection target)
{
    return std::find(sections.begin(), sections.end(), target) != sections.end();
}

std::vector<ImageReportTemplateSection> NormalizeSectionOrder(
    const std::vector<ImageReportTemplateSection>& section_order)
{
    std::vector<ImageReportTemplateSection> normalized;
    for (ImageReportTemplateSection section : section_order) {
        if (ContainsSection(DefaultSectionOrder(), section) &&
            !ContainsSection(normalized, section)) {
            normalized.push_back(section);
        }
    }
    for (ImageReportTemplateSection section : DefaultSectionOrder()) {
        if (!ContainsSection(normalized, section)) {
            normalized.push_back(section);
        }
    }
    return normalized;
}

std::wstring SectionOrderText(const std::vector<ImageReportTemplateSection>& section_order)
{
    const std::vector<ImageReportTemplateSection> order = NormalizeSectionOrder(section_order);

    std::wstring text;
    for (ImageReportTemplateSection section : order) {
        if (!text.empty()) {
            text += L",";
        }
        text += SectionToken(section);
    }
    return text;
}

std::vector<ImageReportTemplateSection> ParseSectionOrder(const std::wstring& text)
{
    std::vector<ImageReportTemplateSection> order;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t comma = text.find(L',', start);
        const std::wstring token =
            TrimText(text.substr(start, comma == std::wstring::npos ? std::wstring::npos : comma - start));
        ImageReportTemplateSection section = ImageReportTemplateSection::CurrentImage;
        if (SectionFromToken(token, section)) {
            order.push_back(section);
        }
        if (comma == std::wstring::npos) {
            break;
        }
        start = comma + 1U;
    }
    return NormalizeSectionOrder(order);
}

void AppendVisualTemplateMetadata(
    std::wostringstream& report,
    const ImageReportTemplateOptions& options,
    const std::wstring& title)
{
    report << L"<!-- CameraViewImageReportTemplateOptions\n";
    report << L"Title=" << MetadataEscape(title) << L"\n";
    report << L"Subtitle=" << MetadataEscape(options.subtitle) << L"\n";
    report << L"Accent=" << AccentToken(options.accent) << L"\n";
    report << L"ImageSize=" << ImageSizeToken(options.image_size) << L"\n";
    report << L"PageLayout=" << PageLayoutToken(options.page_layout) << L"\n";
    report << L"PrintOrientation=" << PrintOrientationToken(options.print_orientation) << L"\n";
    report << L"MeasurementPrecision=" << MeasurementPrecisionToken(options.measurement_precision) << L"\n";
    report << L"ImageCaption=" << MetadataEscape(options.image_caption) << L"\n";
    report << L"HeadingImage=" << MetadataEscape(options.current_image_heading) << L"\n";
    report << L"HeadingInfo=" << MetadataEscape(options.report_information_heading) << L"\n";
    report << L"HeadingNotes=" << MetadataEscape(options.notes_heading) << L"\n";
    report << L"HeadingSummary=" << MetadataEscape(options.measurement_summary_heading) << L"\n";
    report << L"HeadingTable=" << MetadataEscape(options.measurement_table_heading) << L"\n";
    report << L"HeadingDetails=" << MetadataEscape(options.image_details_heading) << L"\n";
    report << L"Image=" << (options.show_image ? L"1" : L"0") << L"\n";
    report << L"InfoVisible=" << (options.show_report_information ? L"1" : L"0") << L"\n";
    report << L"InfoFields=" << MetadataEscape(options.report_information_fields) << L"\n";
    report << L"NotesVisible=" << (options.show_notes ? L"1" : L"0") << L"\n";
    report << L"Notes=" << MetadataEscape(options.notes) << L"\n";
    report << L"Summary=" << (options.show_measurement_summary ? L"1" : L"0") << L"\n";
    report << L"Table=" << (options.show_measurement_table ? L"1" : L"0") << L"\n";
    report << L"MeasurementRaw=" << (options.show_measurement_raw_values ? L"1" : L"0") << L"\n";
    report << L"MeasurementGroups=" << (options.group_measurements_by_type ? L"1" : L"0") << L"\n";
    report << L"Calibration=" << (options.show_calibration_details ? L"1" : L"0") << L"\n";
    report << L"Processing=" << (options.show_processing_details ? L"1" : L"0") << L"\n";
    report << L"Footer=" << (options.show_footer ? L"1" : L"0") << L"\n";
    report << L"FooterText=" << MetadataEscape(options.footer_text) << L"\n";
    report << L"Order=" << SectionOrderText(options.section_order) << L"\n";
    report << L"-->\n";
}

bool ParseVisualTemplateBool(const std::wstring& value, bool fallback)
{
    const std::wstring text = TrimText(value);
    if (text == L"1" || text == L"true" || text == L"yes") {
        return true;
    }
    if (text == L"0" || text == L"false" || text == L"no") {
        return false;
    }
    return fallback;
}

bool MeasurementRawValuesVisible(const std::wstring& template_text)
{
    ImageReportTemplateOptions options;
    return !DiagnosticReportActions::TryParseImageReportTemplateOptions(template_text, options) ||
        options.show_measurement_raw_values;
}

bool MeasurementGroupsVisible(const std::wstring& template_text)
{
    ImageReportTemplateOptions options;
    return DiagnosticReportActions::TryParseImageReportTemplateOptions(template_text, options) &&
        options.group_measurements_by_type;
}

ImageReportTemplateMeasurementPrecision MeasurementPrecisionSetting(const std::wstring& template_text)
{
    ImageReportTemplateOptions options;
    if (DiagnosticReportActions::TryParseImageReportTemplateOptions(template_text, options)) {
        return options.measurement_precision;
    }
    return ImageReportTemplateMeasurementPrecision::Automatic;
}

DiagnosticReportInput BuildReportInput(
    DiagnosticReportActionInput input,
    const MeasurementCollection& measurements)
{
    DiagnosticReportInput report;
    report.generated = input.generated;
    report.status = std::move(input.status);
    report.preview_telemetry = std::move(input.preview_telemetry);
    report.viewport_zoom = std::move(input.viewport_zoom);
    report.preview_running = input.preview_running;
    report.processing_running = input.processing_running;
    report.sdk = std::move(input.sdk);
    report.sdk_telemetry = DiagnosticReportBuilder::BuildSdkTelemetry(report.sdk);
    report.enumerated_cameras = input.enumerated_cameras;
    report.selected_camera_index = input.selected_camera_index;
    report.enumerated_devices = std::move(input.enumerated_devices);
    report.latest_frame_source = std::move(input.latest_frame_source);
    report.latest_frame = std::move(input.latest_frame);

    report.measurement.objective_label = std::move(input.objective_label);
    report.measurement.calibrated = input.calibration.IsCalibrated();
    report.measurement.microns_per_pixel = input.calibration.MicronsPerPixel();
    report.measurement.display_unit = input.display_unit;
    report.measurement.total_measurements = measurements.Count();
    report.measurement.length_measurements = measurements.LengthCount();
    report.measurement.angle_measurements = measurements.AngleCount();
    report.measurement.rectangle_area_measurements = measurements.RectangleCount();
    report.measurement.polygon_area_measurements = measurements.PolygonCount();

    report.image_processing.pseudo_color = input.pseudo_color;
    report.image_processing.preview_display_mode = std::move(input.preview_display_mode);
    report.image_processing.dye_profiles = input.dye_profiles;
    report.image_processing.fluorescence_channels = input.fluorescence_channels;
    report.image_processing.stitch_tiles = input.stitch_tiles;
    report.image_processing.stitch_search_percent = input.stitch_search_percent;
    report.image_processing.edf_frames = input.edf_frames;
    report.image_processing.edf_focus_radius = input.edf_focus_radius;
    report.image_processing.processing_result_visible = input.processing_result_visible;
    report.image_processing.processing_result_kind = std::move(input.processing_result_kind);
    report.image_processing.processing_result_source = std::move(input.processing_result_source);
    report.image_processing.processing_result_width = input.processing_result_width;
    report.image_processing.processing_result_height = input.processing_result_height;
    report.image_processing.edf_composite_available = input.edf_composite_available;
    report.image_processing.edf_focus_map_available = input.edf_focus_map_available;
    return report;
}

std::wstring ApplyImageReportTemplate(
    const DiagnosticReportInput& report,
    const MeasurementCollection& measurements,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit,
    const std::wstring& image_file_name,
    const ImageFrame& report_image,
    const std::wstring& template_text)
{
    std::wstring output = template_text.empty()
        ? DiagnosticReportActions::DefaultImageReportTemplate()
        : template_text;

    ReplaceAll(output, L"{{Generated}}", HtmlEscape(FormatTimestamp(report.generated)));
    ReplaceAll(output, L"{{Status}}", HtmlEscape(report.status));
    ReplaceAll(output, L"{{PreviewTelemetry}}",
        HtmlEscape(report.preview_telemetry.empty() ? L"(none)" : report.preview_telemetry));
    ReplaceAll(output, L"{{ViewportZoom}}",
        HtmlEscape(report.viewport_zoom.empty() ? L"(none)" : report.viewport_zoom));
    ReplaceAll(output, L"{{PreviewRunning}}", YesNo(report.preview_running));
    ReplaceAll(output, L"{{ProcessingRunning}}", YesNo(report.processing_running));
    ReplaceAll(output, L"{{SdkTelemetry}}", HtmlEscape(report.sdk_telemetry));
    ReplaceAll(output, L"{{SelectedCameraIndex}}",
        report.selected_camera_index >= 0
            ? std::to_wstring(report.selected_camera_index + 1)
            : L"(none)");
    ReplaceAll(output, L"{{SelectedCamera}}", HtmlEscape(SelectedCameraSummary(report)));
    ReplaceAll(output, L"{{LatestFrameSource}}",
        HtmlEscape(report.latest_frame_source.empty() ? L"(none)" : report.latest_frame_source));
    ReplaceAll(output, L"{{LatestFrameSize}}", HtmlEscape(FrameSize(report.latest_frame)));
    ReplaceAll(output, L"{{Objective}}",
        HtmlEscape(report.measurement.objective_label.empty() ? L"(none)" : report.measurement.objective_label));
    ReplaceAll(output, L"{{Calibrated}}", YesNo(report.measurement.calibrated));
    ReplaceAll(output, L"{{MicronsPerPixel}}", FormatDouble(report.measurement.microns_per_pixel, 8));
    ReplaceAll(output, L"{{DisplayUnit}}",
        HtmlEscape(CalibrationProfile::UnitLabel(report.measurement.display_unit)));
    ReplaceAll(output, L"{{TotalMeasurements}}", std::to_wstring(report.measurement.total_measurements));
    ReplaceAll(output, L"{{LengthMeasurements}}", std::to_wstring(report.measurement.length_measurements));
    ReplaceAll(output, L"{{AngleMeasurements}}", std::to_wstring(report.measurement.angle_measurements));
    ReplaceAll(output, L"{{RectangleAreaMeasurements}}",
        std::to_wstring(report.measurement.rectangle_area_measurements));
    ReplaceAll(output, L"{{PolygonAreaMeasurements}}",
        std::to_wstring(report.measurement.polygon_area_measurements));
    ReplaceAll(output, L"{{PreviewDisplayMode}}",
        HtmlEscape(report.image_processing.preview_display_mode.empty()
            ? L"(none)"
            : report.image_processing.preview_display_mode));
    ReplaceAll(output, L"{{PseudoColor}}", HtmlEscape(PseudoColorMapper::Label(report.image_processing.pseudo_color)));
    ReplaceAll(output, L"{{DyeProfiles}}", std::to_wstring(report.image_processing.dye_profiles));
    ReplaceAll(output, L"{{FluorescenceChannels}}",
        std::to_wstring(report.image_processing.fluorescence_channels));
    ReplaceAll(output, L"{{StitchTiles}}", std::to_wstring(report.image_processing.stitch_tiles));
    ReplaceAll(output, L"{{StitchSearchPercent}}",
        std::to_wstring(report.image_processing.stitch_search_percent));
    ReplaceAll(output, L"{{EdfFrames}}", std::to_wstring(report.image_processing.edf_frames));
    ReplaceAll(output, L"{{EdfFocusRadius}}", std::to_wstring(report.image_processing.edf_focus_radius));
    ReplaceAll(output, L"{{ProcessingResultKind}}",
        HtmlEscape(report.image_processing.processing_result_kind.empty()
            ? L"(none)"
            : report.image_processing.processing_result_kind));
    ReplaceAll(output, L"{{ProcessingResultSource}}",
        HtmlEscape(report.image_processing.processing_result_source.empty()
            ? L"(none)"
            : report.image_processing.processing_result_source));
    ReplaceAll(output, L"{{ProcessingResultSize}}", ProcessingResultSize(report.image_processing));
    ReplaceAll(output, L"{{EdfCompositeAvailable}}",
        YesNo(report.image_processing.edf_composite_available));
    ReplaceAll(output, L"{{EdfFocusMapAvailable}}",
        YesNo(report.image_processing.edf_focus_map_available));

    ReplaceAll(output, L"{{ImageFile}}", HtmlEscape(image_file_name));
    ReplaceAll(output, L"{{ImageWidth}}",
        report_image.IsValid() ? std::to_wstring(report_image.width) : L"0");
    ReplaceAll(output, L"{{ImageHeight}}",
        report_image.IsValid() ? std::to_wstring(report_image.height) : L"0");
    ReplaceAll(output, L"{{ImageSize}}", HtmlEscape(FrameSize(report_image)));
    ReplaceAll(output, L"{{ImageTag}}", ImageTagHtml(image_file_name, report_image));
    ReplaceAll(output, L"{{MeasurementSummary}}", MeasurementSummaryHtml(report.measurement));
    ReplaceAll(output, L"{{MeasurementTable}}",
        MeasurementTableHtml(
            measurements,
            calibration,
            display_unit,
            MeasurementPrecisionSetting(output),
            MeasurementRawValuesVisible(output),
            MeasurementGroupsVisible(output)));
    ReplaceAll(output, L"{{MeasurementLines}}",
        MeasurementLinesHtml(measurements, calibration, display_unit));

    return output;
}

} // namespace

std::wstring DiagnosticReportActions::DefaultImageReportTemplate()
{
    return BuildImageReportTemplate(ImageReportTemplateOptions{});
}

std::wstring DiagnosticReportActions::BuildImageReportTemplate(const ImageReportTemplateOptions& options)
{
    const std::wstring title = options.title.empty()
        ? L"CameraView Image Report"
        : options.title;

    std::wostringstream report;
    report << LR"(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
)";
    AppendVisualTemplateMetadata(report, options, title);
    report << LR"(<title>)" << HtmlEscape(title) << LR"(</title>
<style>
body { margin: 0; background: #eef1f4; color: #1f2933; font-family: "Segoe UI", Arial, sans-serif; }
.page { )" << PageLayoutCss(options.page_layout) << LR"( background: #ffffff; min-height: 100vh; }
header { border-bottom: 2px solid )" << AccentColor(options.accent) << LR"(; margin-bottom: 22px; padding-bottom: 14px; }
h1 { )" << H1Css(options.page_layout) << LR"( }
h2 { )" << H2Css(options.page_layout) << LR"( }
.subtitle { color: #3f5365; font-size: 14px; line-height: 1.45; margin: -2px 0 9px; }
.meta { color: #607080; font-size: 13px; }
.report-image { display: block; )" << ImageSizeCss(options.image_size) << LR"( height: auto; border: 1px solid #c8d0d9; background: #111820; }
.caption { margin-top: 8px; color: #607080; font-size: 13px; }
.notes { background: #f8fafc; border-left: 4px solid )" << AccentColor(options.accent) << LR"(; line-height: 1.55; margin: 0; padding: 12px 14px; }
.summary-grid { display: grid; grid-template-columns: 160px 1fr; gap: 8px 14px; margin: 0; }
.summary-grid dt { color: #607080; }
.summary-grid dd { margin: 0; font-weight: 600; }
.measurement-table { border-collapse: collapse; width: 100%; font-size: )" << TableFontCss(options.page_layout) << LR"(; }
.measurement-table th, .measurement-table td { border: 1px solid #d8dee6; padding: )" << TableCellPaddingCss(options.page_layout) << LR"(; text-align: left; }
.measurement-table th { background: )" << AccentSoftColor(options.accent) << LR"(; }
.measurement-table .measurement-group th { background: #f8fafc; color: #3f5365; font-weight: 650; }
.field-table { border-collapse: collapse; width: 100%; font-size: )" << TableFontCss(options.page_layout) << LR"(; }
.field-table th, .field-table td { border-bottom: 1px solid #e1e6ec; padding: 7px 4px; text-align: left; vertical-align: top; }
.field-table th { width: 210px; color: #607080; font-weight: 600; }
.empty { color: #7a8793; font-style: italic; }
footer { border-top: 1px solid #d8dee6; color: #607080; font-size: 12px; margin-top: 28px; padding-top: 10px; }
@page { size: A4 )" << PrintOrientationToken(options.print_orientation) << LR"(; margin: 12mm; }
@media print { body { background: #ffffff; } .page { max-width: none; min-height: 0; } }
</style>
</head>
<body>
<main class="page">
<header>
<h1>)" << HtmlEscape(title) << LR"(</h1>
)";
    if (!TrimText(options.subtitle).empty()) {
        report << LR"(<div class="subtitle">)" << MultilineHtml(options.subtitle) << LR"(</div>
)";
    }
    report << LR"(
<div class="meta">Generated: {{Generated}}</div>
</header>

)";

    const std::vector<ImageReportTemplateSection> section_order =
        NormalizeSectionOrder(options.section_order);

    for (ImageReportTemplateSection section : section_order) {
        switch (section) {
        case ImageReportTemplateSection::CurrentImage:
            if (!options.show_image) {
                break;
            }
            {
                const std::wstring image_caption = ImageCaptionTemplate(options);
                report << LR"(<section>
<h2>)" << HtmlEscape(SectionHeading(options, section)) << LR"(</h2>
{{ImageTag}}
<div class="caption">)" << MultilineHtml(image_caption) << LR"(</div>
</section>

)";
            }
            break;
        case ImageReportTemplateSection::ReportInformation:
            if (!options.show_report_information) {
                break;
            }
            report << LR"(<section>
<h2>)" << HtmlEscape(SectionHeading(options, section)) << LR"(</h2>
)" << ReportInformationFieldsHtml(options.report_information_fields) << LR"(
</section>

)";
            break;
        case ImageReportTemplateSection::ReportNotes:
            if (!options.show_notes) {
                break;
            }
            report << LR"(<section>
<h2>)" << HtmlEscape(SectionHeading(options, section)) << LR"(</h2>
)";
            if (TrimText(options.notes).empty()) {
                report << LR"(<p class="empty">No notes.</p>
)";
            } else {
                report << LR"(<p class="notes">)" << MultilineHtml(options.notes) << LR"(</p>
)";
            }
            report << LR"(</section>

)";
            break;
        case ImageReportTemplateSection::MeasurementSummary:
            if (!options.show_measurement_summary) {
                break;
            }
            report << LR"(<section>
<h2>)" << HtmlEscape(SectionHeading(options, section)) << LR"(</h2>
{{MeasurementSummary}}
</section>

)";
            break;
        case ImageReportTemplateSection::MeasurementTable:
            if (!options.show_measurement_table) {
                break;
            }
            report << LR"(<section>
<h2>)" << HtmlEscape(SectionHeading(options, section)) << LR"(</h2>
{{MeasurementTable}}
</section>

)";
            break;
        case ImageReportTemplateSection::ImageDetails:
            if (!options.show_calibration_details && !options.show_processing_details) {
                break;
            }
            report << LR"(<section>
<h2>)" << HtmlEscape(SectionHeading(options, section)) << LR"(</h2>
<table class="field-table">
)";
            if (options.show_calibration_details) {
                report << LR"(<tr><th>Objective</th><td>{{Objective}}</td></tr>
<tr><th>Calibrated</th><td>{{Calibrated}}</td></tr>
<tr><th>Microns per pixel</th><td>{{MicronsPerPixel}}</td></tr>
<tr><th>Display unit</th><td>{{DisplayUnit}}</td></tr>
<tr><th>Image source</th><td>{{LatestFrameSource}}</td></tr>
<tr><th>Current image size</th><td>{{ImageSize}}</td></tr>
<tr><th>Viewport zoom</th><td>{{ViewportZoom}}</td></tr>
)";
            }
            if (options.show_processing_details) {
                report << LR"(<tr><th>Pseudo color</th><td>{{PseudoColor}}</td></tr>
<tr><th>Fluorescence channels</th><td>{{FluorescenceChannels}}</td></tr>
<tr><th>Stitch tiles</th><td>{{StitchTiles}}</td></tr>
<tr><th>EDF frames</th><td>{{EdfFrames}}</td></tr>
)";
            }
            report << LR"(</table>
</section>

)";
            break;
        default:
            break;
        }
    }

    if (options.show_footer) {
        report << LR"(<footer>)" << MultilineHtml(FooterTextTemplate(options)) << LR"(</footer>
)";
    }

    report << LR"(</main>
</body>
</html>
)";
    return report.str();
}

bool DiagnosticReportActions::TryParseImageReportTemplateOptions(
    const std::wstring& template_text,
    ImageReportTemplateOptions& options)
{
    constexpr const wchar_t* kMarker = L"<!-- CameraViewImageReportTemplateOptions";
    const std::size_t marker = template_text.find(kMarker);
    if (marker == std::wstring::npos) {
        return false;
    }

    const std::size_t content_start = marker + std::wcslen(kMarker);
    const std::size_t content_end = template_text.find(L"-->", content_start);
    if (content_end == std::wstring::npos) {
        return false;
    }

    ImageReportTemplateOptions parsed;
    const std::wstring metadata = template_text.substr(content_start, content_end - content_start);
    std::size_t line_start = 0;
    while (line_start <= metadata.size()) {
        const std::size_t line_end = metadata.find(L'\n', line_start);
        const std::wstring line = TrimText(metadata.substr(
            line_start,
            line_end == std::wstring::npos ? std::wstring::npos : line_end - line_start));
        const std::size_t equals = line.find(L'=');
        if (equals != std::wstring::npos) {
            const std::wstring key = TrimText(line.substr(0, equals));
            const std::wstring value = TrimText(line.substr(equals + 1U));
            if (key == L"Title") {
                const std::wstring title = MetadataUnescape(value);
                parsed.title = title.empty() ? parsed.title : title;
            } else if (key == L"Subtitle") {
                parsed.subtitle = MetadataUnescape(value);
            } else if (key == L"Accent") {
                ImageReportTemplateAccent accent = parsed.accent;
                if (AccentFromToken(value, accent)) {
                    parsed.accent = accent;
                }
            } else if (key == L"ImageSize") {
                ImageReportTemplateImageSize image_size = parsed.image_size;
                if (ImageSizeFromToken(value, image_size)) {
                    parsed.image_size = image_size;
                }
            } else if (key == L"PageLayout") {
                ImageReportTemplatePageLayout page_layout = parsed.page_layout;
                if (PageLayoutFromToken(value, page_layout)) {
                    parsed.page_layout = page_layout;
                }
            } else if (key == L"PrintOrientation") {
                ImageReportTemplatePrintOrientation orientation = parsed.print_orientation;
                if (PrintOrientationFromToken(value, orientation)) {
                    parsed.print_orientation = orientation;
                }
            } else if (key == L"MeasurementPrecision") {
                ImageReportTemplateMeasurementPrecision precision = parsed.measurement_precision;
                if (MeasurementPrecisionFromToken(value, precision)) {
                    parsed.measurement_precision = precision;
                }
            } else if (key == L"ImageCaption") {
                parsed.image_caption = MetadataUnescape(value);
            } else if (key == L"HeadingImage") {
                parsed.current_image_heading = MetadataUnescape(value);
            } else if (key == L"HeadingInfo") {
                parsed.report_information_heading = MetadataUnescape(value);
            } else if (key == L"HeadingNotes") {
                parsed.notes_heading = MetadataUnescape(value);
            } else if (key == L"HeadingSummary") {
                parsed.measurement_summary_heading = MetadataUnescape(value);
            } else if (key == L"HeadingTable") {
                parsed.measurement_table_heading = MetadataUnescape(value);
            } else if (key == L"HeadingDetails") {
                parsed.image_details_heading = MetadataUnescape(value);
            } else if (key == L"Image") {
                parsed.show_image = ParseVisualTemplateBool(value, parsed.show_image);
            } else if (key == L"InfoVisible") {
                parsed.show_report_information =
                    ParseVisualTemplateBool(value, parsed.show_report_information);
            } else if (key == L"InfoFields") {
                parsed.report_information_fields = MetadataUnescape(value);
            } else if (key == L"NotesVisible") {
                parsed.show_notes = ParseVisualTemplateBool(value, parsed.show_notes);
            } else if (key == L"Notes") {
                parsed.notes = MetadataUnescape(value);
            } else if (key == L"Summary") {
                parsed.show_measurement_summary =
                    ParseVisualTemplateBool(value, parsed.show_measurement_summary);
            } else if (key == L"Table") {
                parsed.show_measurement_table =
                    ParseVisualTemplateBool(value, parsed.show_measurement_table);
            } else if (key == L"MeasurementRaw") {
                parsed.show_measurement_raw_values =
                    ParseVisualTemplateBool(value, parsed.show_measurement_raw_values);
            } else if (key == L"MeasurementGroups") {
                parsed.group_measurements_by_type =
                    ParseVisualTemplateBool(value, parsed.group_measurements_by_type);
            } else if (key == L"Calibration") {
                parsed.show_calibration_details =
                    ParseVisualTemplateBool(value, parsed.show_calibration_details);
            } else if (key == L"Processing") {
                parsed.show_processing_details =
                    ParseVisualTemplateBool(value, parsed.show_processing_details);
            } else if (key == L"Footer") {
                parsed.show_footer = ParseVisualTemplateBool(value, parsed.show_footer);
            } else if (key == L"FooterText") {
                parsed.footer_text = MetadataUnescape(value);
            } else if (key == L"Order") {
                parsed.section_order = ParseSectionOrder(value);
            }
        }
        if (line_end == std::wstring::npos) {
            break;
        }
        line_start = line_end + 1U;
    }

    options = std::move(parsed);
    return true;
}

std::wstring DiagnosticReportActions::BuildReport(
    DiagnosticReportActionInput input,
    const MeasurementCollection& measurements,
    const std::wstring& template_text)
{
    const DiagnosticReportInput report = BuildReportInput(std::move(input), measurements);
    return template_text.empty()
        ? DiagnosticReportBuilder::Build(report)
        : DiagnosticReportBuilder::BuildFromTemplate(report, template_text);
}

std::wstring DiagnosticReportActions::BuildImageReport(
    DiagnosticReportActionInput input,
    const MeasurementCollection& measurements,
    const std::wstring& image_file_name,
    const ImageFrame& report_image,
    const std::wstring& template_text)
{
    const CalibrationProfile calibration = input.calibration;
    const MeasurementUnit display_unit = input.display_unit;
    const DiagnosticReportInput report = BuildReportInput(std::move(input), measurements);
    return ApplyImageReportTemplate(
        report,
        measurements,
        calibration,
        display_unit,
        image_file_name,
        report_image,
        template_text);
}

std::wstring DiagnosticReportActions::BuildSdkTelemetry(const CameraSdkDiagnostics& diagnostics)
{
    return DiagnosticReportBuilder::BuildSdkTelemetry(diagnostics);
}

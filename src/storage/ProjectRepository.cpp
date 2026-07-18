#include "ProjectRepository.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <fstream>
#include <iomanip>
#include <cmath>
#include <regex>
#include <sstream>
#include <utility>
#include <vector>

namespace {

std::string WideToUtf8(const std::wstring& value)
{
    if (value.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (size <= 0) {
        return {};
    }

    std::string output(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        output.data(),
        size,
        nullptr,
        nullptr);
    return output;
}

std::wstring Utf8ToWide(const std::string& value)
{
    if (value.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    if (size <= 0) {
        return {};
    }

    std::wstring output(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        output.data(),
        size);
    return output;
}

std::string JsonEscape(const std::wstring& value)
{
    std::string output;
    for (char ch : WideToUtf8(value)) {
        switch (ch) {
        case '\\':
            output += "\\\\";
            break;
        case '"':
            output += "\\\"";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            output.push_back(ch);
            break;
        }
    }
    return output;
}

std::wstring JsonUnescape(const std::string& value)
{
    std::string output;
    output.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        const char ch = value[index];
        if (ch != '\\' || index + 1 >= value.size()) {
            output.push_back(ch);
            continue;
        }

        const char next = value[++index];
        switch (next) {
        case '\\':
            output.push_back('\\');
            break;
        case '"':
            output.push_back('"');
            break;
        case 'n':
            output.push_back('\n');
            break;
        case 'r':
            output.push_back('\r');
            break;
        case 't':
            output.push_back('\t');
            break;
        default:
            output.push_back(next);
            break;
        }
    }
    return Utf8ToWide(output);
}

bool TryParseDouble(const std::string& text, double& value)
{
    try {
        std::size_t parsed = 0;
        value = std::stod(text, &parsed);
        return parsed == text.size() && std::isfinite(value);
    } catch (...) {
        value = 0.0;
        return false;
    }
}

bool TryParseByte(const std::string& text, unsigned char& value)
{
    try {
        std::size_t parsed = 0;
        const int parsed_value = std::stoi(text, &parsed);
        if (parsed != text.size() || parsed_value < 0 || parsed_value > 255) {
            value = 0;
            return false;
        }
        value = static_cast<unsigned char>(parsed_value);
        return true;
    } catch (...) {
        value = 0;
        return false;
    }
}

} // namespace

bool ProjectRepository::Save(const std::filesystem::path& path, const ProjectDocument& document, std::wstring& error)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        error = L"Failed to create project file.";
        return false;
    }

    output << "{\n";
    output << "  \"format\": \"CameraViewProject\",\n";
    output << "  \"version\": 8,\n";
    output << "  \"calibration\": {\n";
    output << "    \"microns_per_pixel\": " << std::fixed << std::setprecision(10)
           << document.calibration.MicronsPerPixel() << "\n";
    output << "  },\n";
    output << "  \"selected_objective\": \"" << JsonEscape(document.selected_objective) << "\",\n";
    output << "  \"objective_calibrations\": [\n";
    for (std::size_t index = 0; index < document.objective_calibrations.size(); ++index) {
        const ObjectiveCalibrationDocument& objective_calibration = document.objective_calibrations[index];
        if (index > 0) {
            output << ",\n";
        }
        output << "    {\n";
        output << "      \"objective\": \"" << JsonEscape(objective_calibration.objective) << "\",\n";
        output << "      \"microns_per_pixel\": " << std::fixed << std::setprecision(10)
               << objective_calibration.calibration.MicronsPerPixel() << "\n";
        output << "    }";
    }
    if (!document.objective_calibrations.empty()) {
        output << "\n";
    }
    output << "  ],\n";
    output << "  \"measurements\": [\n";
    bool first_measurement = true;
    auto write_separator = [&]() {
        if (!first_measurement) {
            output << ",\n";
        }
        first_measurement = false;
    };

    for (const LengthMeasurement& measurement : document.measurements) {
        const ImagePoint first = measurement.First();
        const ImagePoint second = measurement.Second();
        write_separator();
        output << "    {\n";
        output << "      \"type\": \"length\",\n";
        output << "      \"name\": \"" << JsonEscape(measurement.Name()) << "\",\n";
        output << "      \"first\": {\"x\": " << std::setprecision(10) << first.x
               << ", \"y\": " << first.y << "},\n";
        output << "      \"second\": {\"x\": " << second.x
               << ", \"y\": " << second.y << "}\n";
        output << "    }";
    }

    for (const AngleMeasurement& measurement : document.angle_measurements) {
        const ImagePoint first = measurement.First();
        const ImagePoint vertex = measurement.Vertex();
        const ImagePoint second = measurement.Second();
        write_separator();
        output << "    {\n";
        output << "      \"type\": \"angle\",\n";
        output << "      \"name\": \"" << JsonEscape(measurement.Name()) << "\",\n";
        output << "      \"first\": {\"x\": " << std::setprecision(10) << first.x
               << ", \"y\": " << first.y << "},\n";
        output << "      \"vertex\": {\"x\": " << vertex.x
               << ", \"y\": " << vertex.y << "},\n";
        output << "      \"second\": {\"x\": " << second.x
               << ", \"y\": " << second.y << "}\n";
        output << "    }";
    }

    for (const RectangleAreaMeasurement& measurement : document.rectangle_measurements) {
        const ImagePoint first = measurement.First();
        const ImagePoint second = measurement.Second();
        write_separator();
        output << "    {\n";
        output << "      \"type\": \"rectangle_area\",\n";
        output << "      \"name\": \"" << JsonEscape(measurement.Name()) << "\",\n";
        output << "      \"first\": {\"x\": " << std::setprecision(10) << first.x
               << ", \"y\": " << first.y << "},\n";
        output << "      \"second\": {\"x\": " << second.x
               << ", \"y\": " << second.y << "}\n";
        output << "    }";
    }

    for (const PolygonAreaMeasurement& measurement : document.polygon_measurements) {
        write_separator();
        output << "    {\n";
        output << "      \"type\": \"polygon_area\",\n";
        output << "      \"name\": \"" << JsonEscape(measurement.Name()) << "\",\n";
        output << "      \"points\": [";
        const std::vector<ImagePoint>& points = measurement.Points();
        for (std::size_t index = 0; index < points.size(); ++index) {
            if (index > 0) {
                output << ", ";
            }
            output << "{\"x\": " << std::setprecision(10) << points[index].x
                   << ", \"y\": " << points[index].y << "}";
        }
        output << "]\n";
        output << "    }";
    }
    if (!first_measurement) {
        output << "\n";
    }
    output << "  ],\n";
    output << "  \"dye_profiles\": [\n";
    for (std::size_t index = 0; index < document.dye_profiles.size(); ++index) {
        const DyeProfile& dye = document.dye_profiles[index];
        if (index > 0) {
            output << ",\n";
        }
        output << "    {\n";
        output << "      \"name\": \"" << JsonEscape(dye.name) << "\",\n";
        output << "      \"excitation_nm\": " << std::setprecision(10) << dye.excitation_nm << ",\n";
        output << "      \"emission_nm\": " << dye.emission_nm << ",\n";
        output << "      \"color\": {\"r\": " << static_cast<int>(dye.color.r)
               << ", \"g\": " << static_cast<int>(dye.color.g)
               << ", \"b\": " << static_cast<int>(dye.color.b) << "}\n";
        output << "    }";
    }
    if (!document.dye_profiles.empty()) {
        output << "\n";
    }
    output << "  ],\n";
    output << "  \"fluorescence_channels\": [\n";
    for (std::size_t index = 0; index < document.fluorescence_channels.size(); ++index) {
        const FluorescenceChannelRecipe& channel = document.fluorescence_channels[index];
        if (index > 0) {
            output << ",\n";
        }
        output << "    {\n";
        output << "      \"name\": \"" << JsonEscape(channel.name) << "\",\n";
        output << "      \"visible\": " << (channel.visible ? "true" : "false") << ",\n";
        output << "      \"black_level\": " << static_cast<int>(channel.black_level) << ",\n";
        output << "      \"white_level\": " << static_cast<int>(channel.white_level) << ",\n";
        output << "      \"color\": {\"r\": " << static_cast<int>(channel.color.r)
               << ", \"g\": " << static_cast<int>(channel.color.g)
               << ", \"b\": " << static_cast<int>(channel.color.b) << "}\n";
        output << "    }";
    }
    if (!document.fluorescence_channels.empty()) {
        output << "\n";
    }
    output << "  ],\n";
    output << "  \"processing_settings\": {\n";
    output << "    \"edf_focus_radius\": " << document.processing_settings.edf_focus_radius << ",\n";
    output << "    \"stitch_search_percent\": " << document.processing_settings.stitch_search_percent << "\n";
    output << "  }\n";
    output << "}\n";

    if (!output) {
        error = L"Failed while writing project file.";
        return false;
    }

    error.clear();
    return true;
}

bool ProjectRepository::Load(const std::filesystem::path& path, ProjectDocument& document, std::wstring& error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = L"Failed to open project file.";
        return false;
    }

    std::ostringstream stream;
    stream << input.rdbuf();
    std::string text = stream.str();
    if (text.rfind("\xEF\xBB\xBF", 0) == 0) {
        text.erase(0, 3);
    }

    if (text.find("\"format\"") == std::string::npos ||
        text.find("\"CameraViewProject\"") == std::string::npos) {
        error = L"Unsupported project file.";
        return false;
    }

    ProjectDocument loaded;

    const std::regex calibration_pattern(
        R"project("calibration"\s*:\s*\{\s*"microns_per_pixel"\s*:\s*([-+0-9.eE]+)\s*\})project");
    std::smatch calibration_match;
    if (std::regex_search(text, calibration_match, calibration_pattern)) {
        double microns_per_pixel = 0.0;
        if (!TryParseDouble(calibration_match[1].str(), microns_per_pixel)) {
            error = L"Invalid calibration value in project file.";
            return false;
        }
        loaded.calibration = CalibrationProfile::FromMicronsPerPixel(microns_per_pixel);
    }

    const std::regex selected_objective_pattern(
        R"project("selected_objective"\s*:\s*"((?:\\.|[^"])*)")project");
    std::smatch selected_objective_match;
    if (std::regex_search(text, selected_objective_match, selected_objective_pattern)) {
        loaded.selected_objective = JsonUnescape(selected_objective_match[1].str());
    }

    const std::regex objective_calibration_pattern(
        R"project(\{\s*"objective"\s*:\s*"((?:\\.|[^"])*)"\s*,\s*"microns_per_pixel"\s*:\s*([-+0-9.eE]+)\s*\})project");
    auto objective_begin =
        std::sregex_iterator(text.begin(), text.end(), objective_calibration_pattern);
    auto objective_end = std::sregex_iterator();
    for (auto iterator = objective_begin; iterator != objective_end; ++iterator) {
        const std::smatch& match = *iterator;
        double microns_per_pixel = 0.0;
        if (!TryParseDouble(match[2].str(), microns_per_pixel)) {
            error = L"Invalid objective calibration value in project file.";
            return false;
        }
        ObjectiveCalibrationDocument objective_calibration;
        objective_calibration.objective = JsonUnescape(match[1].str());
        if (objective_calibration.objective.empty()) {
            error = L"Objective calibration in project file has an empty objective.";
            return false;
        }
        objective_calibration.calibration =
            CalibrationProfile::FromMicronsPerPixel(microns_per_pixel);
        loaded.objective_calibrations.push_back(std::move(objective_calibration));
    }

    const std::regex measurement_pattern(
        R"project(\{\s*"type"\s*:\s*"length"\s*,\s*"name"\s*:\s*"((?:\\.|[^"])*)"\s*,\s*"first"\s*:\s*\{\s*"x"\s*:\s*([-+0-9.eE]+)\s*,\s*"y"\s*:\s*([-+0-9.eE]+)\s*\}\s*,\s*"second"\s*:\s*\{\s*"x"\s*:\s*([-+0-9.eE]+)\s*,\s*"y"\s*:\s*([-+0-9.eE]+)\s*\}\s*\})project");

    auto begin = std::sregex_iterator(text.begin(), text.end(), measurement_pattern);
    auto end = std::sregex_iterator();
    for (auto iterator = begin; iterator != end; ++iterator) {
        const std::smatch& match = *iterator;
        double first_x = 0.0;
        double first_y = 0.0;
        double second_x = 0.0;
        double second_y = 0.0;
        if (!TryParseDouble(match[2].str(), first_x) ||
            !TryParseDouble(match[3].str(), first_y) ||
            !TryParseDouble(match[4].str(), second_x) ||
            !TryParseDouble(match[5].str(), second_y)) {
            error = L"Invalid measurement coordinates in project file.";
            return false;
        }
        loaded.measurements.emplace_back(
            JsonUnescape(match[1].str()),
            ImagePoint{first_x, first_y},
            ImagePoint{second_x, second_y});
    }

    const std::regex angle_pattern(
        R"project(\{\s*"type"\s*:\s*"angle"\s*,\s*"name"\s*:\s*"((?:\\.|[^"])*)"\s*,\s*"first"\s*:\s*\{\s*"x"\s*:\s*([-+0-9.eE]+)\s*,\s*"y"\s*:\s*([-+0-9.eE]+)\s*\}\s*,\s*"vertex"\s*:\s*\{\s*"x"\s*:\s*([-+0-9.eE]+)\s*,\s*"y"\s*:\s*([-+0-9.eE]+)\s*\}\s*,\s*"second"\s*:\s*\{\s*"x"\s*:\s*([-+0-9.eE]+)\s*,\s*"y"\s*:\s*([-+0-9.eE]+)\s*\}\s*\})project");

    begin = std::sregex_iterator(text.begin(), text.end(), angle_pattern);
    for (auto iterator = begin; iterator != end; ++iterator) {
        const std::smatch& match = *iterator;
        double first_x = 0.0;
        double first_y = 0.0;
        double vertex_x = 0.0;
        double vertex_y = 0.0;
        double second_x = 0.0;
        double second_y = 0.0;
        if (!TryParseDouble(match[2].str(), first_x) ||
            !TryParseDouble(match[3].str(), first_y) ||
            !TryParseDouble(match[4].str(), vertex_x) ||
            !TryParseDouble(match[5].str(), vertex_y) ||
            !TryParseDouble(match[6].str(), second_x) ||
            !TryParseDouble(match[7].str(), second_y)) {
            error = L"Invalid angle measurement coordinates in project file.";
            return false;
        }
        loaded.angle_measurements.emplace_back(
            JsonUnescape(match[1].str()),
            ImagePoint{first_x, first_y},
            ImagePoint{vertex_x, vertex_y},
            ImagePoint{second_x, second_y});
    }

    const std::regex rectangle_pattern(
        R"project(\{\s*"type"\s*:\s*"rectangle_area"\s*,\s*"name"\s*:\s*"((?:\\.|[^"])*)"\s*,\s*"first"\s*:\s*\{\s*"x"\s*:\s*([-+0-9.eE]+)\s*,\s*"y"\s*:\s*([-+0-9.eE]+)\s*\}\s*,\s*"second"\s*:\s*\{\s*"x"\s*:\s*([-+0-9.eE]+)\s*,\s*"y"\s*:\s*([-+0-9.eE]+)\s*\}\s*\})project");

    begin = std::sregex_iterator(text.begin(), text.end(), rectangle_pattern);
    for (auto iterator = begin; iterator != end; ++iterator) {
        const std::smatch& match = *iterator;
        double first_x = 0.0;
        double first_y = 0.0;
        double second_x = 0.0;
        double second_y = 0.0;
        if (!TryParseDouble(match[2].str(), first_x) ||
            !TryParseDouble(match[3].str(), first_y) ||
            !TryParseDouble(match[4].str(), second_x) ||
            !TryParseDouble(match[5].str(), second_y)) {
            error = L"Invalid rectangle measurement coordinates in project file.";
            return false;
        }
        loaded.rectangle_measurements.emplace_back(
            JsonUnescape(match[1].str()),
            ImagePoint{first_x, first_y},
            ImagePoint{second_x, second_y});
    }

    const std::regex polygon_pattern(
        R"project(\{\s*"type"\s*:\s*"polygon_area"\s*,\s*"name"\s*:\s*"((?:\\.|[^"])*)"\s*,\s*"points"\s*:\s*\[([^\]]*)\]\s*\})project");
    const std::regex point_pattern(
        R"project(\{\s*"x"\s*:\s*([-+0-9.eE]+)\s*,\s*"y"\s*:\s*([-+0-9.eE]+)\s*\})project");

    begin = std::sregex_iterator(text.begin(), text.end(), polygon_pattern);
    for (auto iterator = begin; iterator != end; ++iterator) {
        const std::smatch& match = *iterator;
        std::vector<ImagePoint> points;

        const std::string points_text = match[2].str();
        auto point_begin = std::sregex_iterator(points_text.begin(), points_text.end(), point_pattern);
        auto point_end = std::sregex_iterator();
        for (auto point_iterator = point_begin; point_iterator != point_end; ++point_iterator) {
            const std::smatch& point_match = *point_iterator;
            double x = 0.0;
            double y = 0.0;
            if (!TryParseDouble(point_match[1].str(), x) ||
                !TryParseDouble(point_match[2].str(), y)) {
                error = L"Invalid polygon measurement coordinates in project file.";
                return false;
            }
            points.push_back(ImagePoint{x, y});
        }

        if (points.size() < 3) {
            error = L"Polygon measurement in project file has fewer than three points.";
            return false;
        }

        loaded.polygon_measurements.emplace_back(
            JsonUnescape(match[1].str()),
            std::move(points));
    }

    const std::regex dye_pattern(
        R"project(\{\s*"name"\s*:\s*"((?:\\.|[^"])*)"\s*,\s*"excitation_nm"\s*:\s*([-+0-9.eE]+)\s*,\s*"emission_nm"\s*:\s*([-+0-9.eE]+)\s*,\s*"color"\s*:\s*\{\s*"r"\s*:\s*([0-9]+)\s*,\s*"g"\s*:\s*([0-9]+)\s*,\s*"b"\s*:\s*([0-9]+)\s*\}\s*\})project");

    begin = std::sregex_iterator(text.begin(), text.end(), dye_pattern);
    for (auto iterator = begin; iterator != end; ++iterator) {
        const std::smatch& match = *iterator;
        double excitation_nm = 0.0;
        double emission_nm = 0.0;
        unsigned char red = 0;
        unsigned char green = 0;
        unsigned char blue = 0;
        if (!TryParseDouble(match[2].str(), excitation_nm) ||
            !TryParseDouble(match[3].str(), emission_nm) ||
            !TryParseByte(match[4].str(), red) ||
            !TryParseByte(match[5].str(), green) ||
            !TryParseByte(match[6].str(), blue) ||
            excitation_nm < 0.0 ||
            emission_nm < 0.0) {
            error = L"Invalid dye profile in project file.";
            return false;
        }

        DyeProfile dye;
        dye.name = JsonUnescape(match[1].str());
        if (dye.name.empty()) {
            error = L"Dye profile in project file has an empty name.";
            return false;
        }
        dye.excitation_nm = excitation_nm;
        dye.emission_nm = emission_nm;
        dye.color = RgbColor{red, green, blue};
        loaded.dye_profiles.push_back(std::move(dye));
    }

    const std::regex channel_pattern(
        R"project(\{\s*"name"\s*:\s*"((?:\\.|[^"])*)"\s*,\s*"visible"\s*:\s*(true|false)\s*,\s*"black_level"\s*:\s*([0-9]+)\s*,\s*"white_level"\s*:\s*([0-9]+)\s*,\s*"color"\s*:\s*\{\s*"r"\s*:\s*([0-9]+)\s*,\s*"g"\s*:\s*([0-9]+)\s*,\s*"b"\s*:\s*([0-9]+)\s*\}\s*\})project");

    begin = std::sregex_iterator(text.begin(), text.end(), channel_pattern);
    for (auto iterator = begin; iterator != end; ++iterator) {
        const std::smatch& match = *iterator;
        unsigned char black_level = 0;
        unsigned char white_level = 255;
        unsigned char red = 0;
        unsigned char green = 0;
        unsigned char blue = 0;
        if (!TryParseByte(match[3].str(), black_level) ||
            !TryParseByte(match[4].str(), white_level) ||
            !TryParseByte(match[5].str(), red) ||
            !TryParseByte(match[6].str(), green) ||
            !TryParseByte(match[7].str(), blue) ||
            white_level <= black_level) {
            error = L"Invalid fluorescence channel settings in project file.";
            return false;
        }

        FluorescenceChannelRecipe channel;
        channel.name = JsonUnescape(match[1].str());
        channel.visible = match[2].str() == "true";
        channel.black_level = black_level;
        channel.white_level = white_level;
        channel.color = RgbColor{red, green, blue};
        loaded.fluorescence_channels.push_back(std::move(channel));
    }

    const std::regex edf_radius_pattern(R"("edf_focus_radius"\s*:\s*([0-9]+))");
    std::smatch edf_radius_match;
    if (std::regex_search(text, edf_radius_match, edf_radius_pattern)) {
        unsigned char radius = 1;
        if (!TryParseByte(edf_radius_match[1].str(), radius) || radius < 1 || radius > 16) {
            error = L"Invalid EDF focus radius in project file.";
            return false;
        }
        loaded.processing_settings.edf_focus_radius = radius;
    }

    const std::regex stitch_search_pattern(R"("stitch_search_percent"\s*:\s*([0-9]+))");
    std::smatch stitch_search_match;
    if (std::regex_search(text, stitch_search_match, stitch_search_pattern)) {
        unsigned char search_percent = 50;
        if (!TryParseByte(stitch_search_match[1].str(), search_percent) ||
            search_percent < 5 ||
            search_percent > 100) {
            error = L"Invalid stitch search percent in project file.";
            return false;
        }
        loaded.processing_settings.stitch_search_percent = search_percent;
    }

    document = std::move(loaded);
    error.clear();
    return true;
}

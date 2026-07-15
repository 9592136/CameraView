#include "DyeProfileFormParser.h"

#include "../platform/TextInputParser.h"

DyeProfileInputResult DyeProfileFormParser::Parse(const DyeProfileInput& input)
{
    DyeProfile dye;
    dye.name = TextInputParser::Trim(input.name);
    if (dye.name.empty()) {
        return {
            DyeProfileInputStatus::MissingName,
            std::nullopt,
            L"Dye name is required."
        };
    }

    if (!TextInputParser::TryParseNonNegativeDouble(input.excitation_nm, dye.excitation_nm) ||
        !TextInputParser::TryParseNonNegativeDouble(input.emission_nm, dye.emission_nm)) {
        return {
            DyeProfileInputStatus::InvalidWavelengths,
            std::nullopt,
            L"Dye wavelengths must be non-negative numbers."
        };
    }

    int red = 0;
    int green = 0;
    int blue = 0;
    if (!TextInputParser::TryParseByte(input.red, red) ||
        !TextInputParser::TryParseByte(input.green, green) ||
        !TextInputParser::TryParseByte(input.blue, blue)) {
        return {
            DyeProfileInputStatus::InvalidColor,
            std::nullopt,
            L"Dye RGB values must be 0-255."
        };
    }

    dye.color = RgbColor{
        static_cast<unsigned char>(red),
        static_cast<unsigned char>(green),
        static_cast<unsigned char>(blue)
    };
    return {
        DyeProfileInputStatus::Valid,
        dye,
        L""
    };
}

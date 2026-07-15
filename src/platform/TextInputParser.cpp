#include "TextInputParser.h"

#include <cmath>
#include <cstdlib>
#include <cwctype>
#include <limits>

std::wstring TextInputParser::Trim(const std::wstring& text)
{
    std::size_t begin = 0;
    while (begin < text.size() && std::iswspace(text[begin])) {
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin && std::iswspace(text[end - 1])) {
        --end;
    }

    return text.substr(begin, end - begin);
}

bool TextInputParser::TryParsePositiveDouble(const std::wstring& text, double& value)
{
    return TryParseDouble(text, value) && value > 0.0;
}

bool TextInputParser::TryParsePositiveFloat(const std::wstring& text, float& value)
{
    double parsed = 0.0;
    if (!TryParsePositiveDouble(text, parsed) ||
        parsed > static_cast<double>(std::numeric_limits<float>::max())) {
        value = 0.0f;
        return false;
    }

    value = static_cast<float>(parsed);
    return std::isfinite(value);
}

bool TextInputParser::TryParseNonNegativeDouble(const std::wstring& text, double& value)
{
    return TryParseDouble(text, value) && value >= 0.0;
}

bool TextInputParser::TryParseByte(const std::wstring& text, int& value)
{
    long parsed = 0;
    if (!TryParseLong(text, parsed) || parsed < 0 || parsed > 255) {
        value = 0;
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}

bool TextInputParser::TryParseIntegerRange(const std::wstring& text, int min_value, int max_value, int& value)
{
    long parsed = 0;
    if (!TryParseLong(text, parsed) || parsed < min_value || parsed > max_value) {
        value = min_value;
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}

bool TextInputParser::TryParseDouble(const std::wstring& text, double& value)
{
    wchar_t* end = nullptr;
    value = std::wcstod(text.c_str(), &end);
    return end != text.c_str() && std::isfinite(value);
}

bool TextInputParser::TryParseLong(const std::wstring& text, long& value)
{
    wchar_t* end = nullptr;
    value = std::wcstol(text.c_str(), &end, 10);
    return end != text.c_str();
}

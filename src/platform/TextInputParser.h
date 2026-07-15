#pragma once

#include <string>

class TextInputParser {
public:
    static std::wstring Trim(const std::wstring& text);
    static bool TryParsePositiveDouble(const std::wstring& text, double& value);
    static bool TryParsePositiveFloat(const std::wstring& text, float& value);
    static bool TryParseNonNegativeDouble(const std::wstring& text, double& value);
    static bool TryParseByte(const std::wstring& text, int& value);
    static bool TryParseIntegerRange(const std::wstring& text, int min_value, int max_value, int& value);

private:
    static bool TryParseDouble(const std::wstring& text, double& value);
    static bool TryParseLong(const std::wstring& text, long& value);
};

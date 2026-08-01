#pragma once
/*
 * StringOps.h
 * 无状态文本工具集。集中散落在多处的小工具（目前为 Trim），
 * 避免每个模块各写一份相同实现。
 */
#include <string>

namespace platform {

// 去除首尾空白（空格、制表符、换行）。
// 原分散于 TextInputParser::Trim 与 DiagnosticReportActions::TrimText，
// 现统一到这里作为唯一实现来源。
inline std::wstring Trim(const std::wstring& text)
{
    const std::wstring whitespace = L" \t\n\r\f\v";
    const auto start = text.find_first_not_of(whitespace);
    if (start == std::wstring::npos) {
        return std::wstring();
    }
    const auto end = text.find_last_not_of(whitespace);
    return text.substr(start, end - start + 1);
}

// Parse a positive integer from a wide string; returns 0 on failure.
inline int ParsePositiveInteger(const std::wstring& text)
{
    if (text.empty()) return 0;
    try {
        std::size_t pos = 0;
        int val = std::stoi(text, &pos);
        if (pos != text.size() || val < 0) return 0;
        return val;
    } catch (...) {
        return 0;
    }
}

// Parse a positive double from a wide string; returns 0.0 on failure.
inline double ParsePositiveDouble(const std::wstring& text)
{
    if (text.empty()) return 0.0;
    try {
        std::size_t pos = 0;
        double val = std::stod(text, &pos);
        if (pos != text.size() || val <= 0.0) return 0.0;
        return val;
    } catch (...) {
        return 0.0;
    }
}

// Parse an exposure time string (e.g. "125ms", "1.5s") into milliseconds.
inline double ParseExposureTime(const std::wstring& text)
{
    if (text.empty()) return 0.0;
    const auto trimmed = Trim(text);
    if (trimmed.empty()) return 0.0;
    // Try "ms" suffix
    if (trimmed.size() >= 2 && (trimmed.substr(trimmed.size() - 2) == L"ms")) {
        double val = ParsePositiveDouble(trimmed.substr(0, trimmed.size() - 2));
        return val;
    }
    // Try "s" suffix
    if (trimmed.back() == L's') {
        double val = ParsePositiveDouble(trimmed.substr(0, trimmed.size() - 1));
        return val * 1000.0;
    }
    // No suffix: treat as ms
    return ParsePositiveDouble(trimmed);
}

} // namespace platform

#pragma once

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <string>

inline std::wstring Lowercase(std::wstring value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return value;
}

inline bool IsSupportedStitchImagePath(const std::filesystem::path& path)
{
    const std::wstring extension = Lowercase(path.extension().wstring());
    return extension == L".bmp" ||
        extension == L".jpg" ||
        extension == L".jpeg" ||
        extension == L".png" ||
        extension == L".tif" ||
        extension == L".tiff";
}

inline int NaturalCompare(const std::wstring& left, const std::wstring& right)
{
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < left.size() && j < right.size()) {
        if (std::iswdigit(left[i]) && std::iswdigit(right[j])) {
            unsigned long long left_value = 0;
            unsigned long long right_value = 0;
            while (i < left.size() && std::iswdigit(left[i])) {
                left_value = left_value * 10ULL + static_cast<unsigned long long>(left[i] - L'0');
                ++i;
            }
            while (j < right.size() && std::iswdigit(right[j])) {
                right_value = right_value * 10ULL + static_cast<unsigned long long>(right[j] - L'0');
                ++j;
            }
            if (left_value != right_value) {
                return left_value < right_value ? -1 : 1;
            }
            continue;
        }

        const wchar_t left_char = static_cast<wchar_t>(std::towlower(left[i]));
        const wchar_t right_char = static_cast<wchar_t>(std::towlower(right[j]));
        if (left_char != right_char) {
            return left_char < right_char ? -1 : 1;
        }
        ++i;
        ++j;
    }
    if (left.size() == right.size()) {
        return 0;
    }
    return left.size() < right.size() ? -1 : 1;
}

inline bool NaturalPathLess(const std::filesystem::path& left, const std::filesystem::path& right)
{
    return NaturalCompare(left.filename().wstring(), right.filename().wstring()) < 0;
}

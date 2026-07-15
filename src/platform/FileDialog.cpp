#include "FileDialog.h"

#include <commdlg.h>

#include <algorithm>

namespace {
constexpr wchar_t kCsvFilter[] = L"CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
constexpr wchar_t kBmpFilter[] = L"BMP Files (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0";
constexpr wchar_t kTextFilter[] = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
constexpr wchar_t kProjectFilter[] =
    L"CameraView Project (*.cvproj)\0*.cvproj\0JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";

void CopyDefaultName(const wchar_t* default_name, wchar_t (&buffer)[MAX_PATH])
{
    if (!default_name) {
        return;
    }
    const std::wstring value(default_name);
    const std::size_t count = std::min<std::size_t>(value.size(), MAX_PATH - 1U);
    std::copy_n(value.c_str(), count, buffer);
    buffer[count] = L'\0';
}

bool ShowFileDialog(
    HWND owner,
    const wchar_t* default_name,
    const wchar_t* filter,
    const wchar_t* default_extension,
    DWORD flags,
    bool save,
    std::wstring& selected_path)
{
    wchar_t file_name[MAX_PATH] = {};
    CopyDefaultName(default_name, file_name);

    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = file_name;
    dialog.nMaxFile = static_cast<DWORD>(MAX_PATH);
    dialog.lpstrDefExt = default_extension;
    dialog.Flags = flags;

    const BOOL accepted = save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
    if (!accepted) {
        return false;
    }

    selected_path = file_name;
    return true;
}
} // namespace

bool FileDialog::SaveCsv(HWND owner, std::wstring& selected_path)
{
    return ShowFileDialog(
        owner,
        L"measurements.csv",
        kCsvFilter,
        L"csv",
        OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
        true,
        selected_path);
}

bool FileDialog::SaveBmp(HWND owner, std::wstring& selected_path)
{
    return ShowFileDialog(
        owner,
        L"CameraViewImage.bmp",
        kBmpFilter,
        L"bmp",
        OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
        true,
        selected_path);
}

bool FileDialog::OpenBmp(HWND owner, std::wstring& selected_path)
{
    return ShowFileDialog(
        owner,
        nullptr,
        kBmpFilter,
        L"bmp",
        OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
        false,
        selected_path);
}

bool FileDialog::SaveText(HWND owner, std::wstring& selected_path)
{
    return ShowFileDialog(
        owner,
        L"CameraViewDiagnostics.txt",
        kTextFilter,
        L"txt",
        OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
        true,
        selected_path);
}

bool FileDialog::SaveProject(HWND owner, std::wstring& selected_path)
{
    return ShowFileDialog(
        owner,
        L"CameraViewProject.cvproj",
        kProjectFilter,
        L"cvproj",
        OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
        true,
        selected_path);
}

bool FileDialog::OpenProject(HWND owner, std::wstring& selected_path)
{
    return ShowFileDialog(
        owner,
        nullptr,
        kProjectFilter,
        nullptr,
        OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
        false,
        selected_path);
}

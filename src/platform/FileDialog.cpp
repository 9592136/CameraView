#include "FileDialog.h"

#include <commdlg.h>
#include <shlobj.h>

#include <algorithm>
#include <vector>

namespace {
constexpr wchar_t kCsvFilter[] = L"CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0";
constexpr wchar_t kBmpFilter[] = L"BMP Files (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0";
constexpr wchar_t kImageSaveFilter[] =
    L"Image Files (*.bmp;*.jpg;*.jpeg;*.png;*.tif;*.tiff)\0*.bmp;*.jpg;*.jpeg;*.png;*.tif;*.tiff\0"
    L"BMP Files (*.bmp)\0*.bmp\0"
    L"JPEG Files (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0"
    L"PNG Files (*.png)\0*.png\0"
    L"TIFF Files (*.tif;*.tiff)\0*.tif;*.tiff\0"
    L"All Files (*.*)\0*.*\0";
constexpr wchar_t kImageOpenFilter[] =
    L"Image Files (*.bmp;*.jpg;*.jpeg;*.png;*.tif;*.tiff)\0*.bmp;*.jpg;*.jpeg;*.png;*.tif;*.tiff\0"
    L"BMP Files (*.bmp)\0*.bmp\0"
    L"JPEG Files (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0"
    L"PNG Files (*.png)\0*.png\0"
    L"TIFF Files (*.tif;*.tiff)\0*.tif;*.tiff\0"
    L"All Files (*.*)\0*.*\0";
constexpr wchar_t kTextFilter[] = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
constexpr wchar_t kReportSaveFilter[] =
    L"HTML Reports (*.html;*.htm)\0*.html;*.htm\0All Files (*.*)\0*.*\0";
constexpr wchar_t kTemplateSaveFilter[] =
    L"Report Templates (*.html;*.htm;*.tpl;*.txt)\0*.html;*.htm;*.tpl;*.txt\0"
    L"HTML Files (*.html;*.htm)\0*.html;*.htm\0"
    L"Text Files (*.txt)\0*.txt\0"
    L"All Files (*.*)\0*.*\0";
constexpr wchar_t kTemplateOpenFilter[] =
    L"Report Templates (*.html;*.htm;*.tpl;*.txt)\0*.html;*.htm;*.tpl;*.txt\0"
    L"HTML Files (*.html;*.htm)\0*.html;*.htm\0"
    L"Text Files (*.txt)\0*.txt\0"
    L"All Files (*.*)\0*.*\0";
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

bool ShowMultiOpenFileDialog(
    HWND owner,
    const wchar_t* filter,
    std::vector<std::wstring>& selected_paths)
{
    std::vector<wchar_t> buffer(32768, L'\0');

    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    if (!GetOpenFileNameW(&dialog)) {
        return false;
    }

    selected_paths.clear();
    const wchar_t* cursor = buffer.data();
    std::wstring first(cursor);
    cursor += first.size() + 1;
    if (*cursor == L'\0') {
        selected_paths.push_back(std::move(first));
        return true;
    }

    std::wstring directory = std::move(first);
    if (!directory.empty() && directory.back() != L'\\' && directory.back() != L'/') {
        directory += L'\\';
    }
    while (*cursor != L'\0') {
        std::wstring file_name(cursor);
        selected_paths.push_back(directory + file_name);
        cursor += file_name.size() + 1;
    }
    return !selected_paths.empty();
}
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

bool FileDialog::SaveImage(HWND owner, std::wstring& selected_path)
{
    return ShowFileDialog(
        owner,
        L"CameraViewImage.bmp",
        kImageSaveFilter,
        L"bmp",
        OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
        true,
        selected_path);
}

bool FileDialog::OpenImage(HWND owner, std::wstring& selected_path)
{
    return ShowFileDialog(
        owner,
        nullptr,
        kImageOpenFilter,
        nullptr,
        OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
        false,
        selected_path);
}

bool FileDialog::OpenImages(HWND owner, std::vector<std::wstring>& selected_paths)
{
    return ShowMultiOpenFileDialog(owner, kImageOpenFilter, selected_paths);
}

bool FileDialog::OpenImageDirectory(HWND owner, std::wstring& selected_path)
{
    BROWSEINFOW browse = {};
    browse.hwndOwner = owner;
    browse.lpszTitle = L"Select image directory";
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE item = SHBrowseForFolderW(&browse);
    if (!item) {
        return false;
    }

    wchar_t path[MAX_PATH] = {};
    const BOOL ok = SHGetPathFromIDListW(item, path);
    CoTaskMemFree(item);
    if (!ok || path[0] == L'\0') {
        return false;
    }

    selected_path = path;
    return true;
}
bool FileDialog::SaveReport(HWND owner, std::wstring& selected_path)
{
    return ShowFileDialog(
        owner,
        L"CameraViewReport.html",
        kReportSaveFilter,
        L"html",
        OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
        true,
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

bool FileDialog::OpenText(HWND owner, std::wstring& selected_path)
{
    return ShowFileDialog(
        owner,
        nullptr,
        kTemplateOpenFilter,
        nullptr,
        OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
        false,
        selected_path);
}

bool FileDialog::SaveTemplate(HWND owner, std::wstring& selected_path)
{
    return ShowFileDialog(
        owner,
        L"CameraViewReportTemplate.html",
        kTemplateSaveFilter,
        L"html",
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

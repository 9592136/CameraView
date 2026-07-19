#pragma once

#include <windows.h>

#include <string>

class FileDialog {
public:
    static bool SaveCsv(HWND owner, std::wstring& selected_path);
    static bool SaveBmp(HWND owner, std::wstring& selected_path);
    static bool SaveImage(HWND owner, std::wstring& selected_path);
    static bool OpenImage(HWND owner, std::wstring& selected_path);
    static bool SaveReport(HWND owner, std::wstring& selected_path);
    static bool SaveText(HWND owner, std::wstring& selected_path);
    static bool OpenText(HWND owner, std::wstring& selected_path);
    static bool SaveTemplate(HWND owner, std::wstring& selected_path);
    static bool SaveProject(HWND owner, std::wstring& selected_path);
    static bool OpenProject(HWND owner, std::wstring& selected_path);
};

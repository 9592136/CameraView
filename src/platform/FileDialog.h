#pragma once

#include <windows.h>

#include <string>

class FileDialog {
public:
    static bool SaveCsv(HWND owner, std::wstring& selected_path);
    static bool SaveBmp(HWND owner, std::wstring& selected_path);
    static bool OpenBmp(HWND owner, std::wstring& selected_path);
    static bool SaveText(HWND owner, std::wstring& selected_path);
    static bool SaveProject(HWND owner, std::wstring& selected_path);
    static bool OpenProject(HWND owner, std::wstring& selected_path);
};

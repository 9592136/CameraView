#include "ui/CameraPreviewApp.h"
#include "ui/MainMenu.h"
#include "ui/WindowProperties.h"

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command)
{
    InitCommonControls();

    const wchar_t* class_name = L"MUCamCameraViewWindow";

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = class_name;

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"Failed to register window class.", L"CameraView", MB_ICONERROR | MB_OK);
        return 1;
    }

    HMENU main_menu = CreateMainMenu(UILanguage::English);
    HWND hwnd = CreateWindowExW(
        0,
        class_name,
        L"CameraView - MUCam Preview",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1100,
        760,
        nullptr,
        main_menu,
        instance,
        nullptr);

    if (!hwnd) {
        DestroyMenu(main_menu);
        MessageBoxW(nullptr, L"Failed to create main window.", L"CameraView", MB_ICONERROR | MB_OK);
        return 1;
    }

    SetLanguageProperty(hwnd, UILanguage::English);
    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}

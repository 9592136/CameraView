#pragma once

#include <windows.h>
#include <cstdlib>

#include "i18n/Localization.h"
#include "ui/UIConstants.h"

// ---- Function panel visibility --------------------------------------------

inline bool IsFunctionPanelVisible(HWND hwnd)
{
    const HANDLE value = GetPropW(hwnd, kFunctionPanelVisibleProperty);
    if (!value) {
        return true;
    }
    return value == reinterpret_cast<HANDLE>(kFunctionPanelVisibleValue);
}

inline bool IsFunctionPanelDockedLeft(HWND hwnd)
{
    const HANDLE value = GetPropW(hwnd, kFunctionPanelDockLeftProperty);
    if (!value) {
        return true;
    }
    return value == reinterpret_cast<HANDLE>(kFunctionPanelDockLeftValue);
}

inline int FunctionPanelWidth(HWND hwnd)
{
    return static_cast<int>(reinterpret_cast<INT_PTR>(GetPropW(hwnd, kFunctionPanelWidthProperty)));
}

inline void SetFunctionPanelWidthProperty(HWND hwnd, int width)
{
    if (width > 0) {
        SetPropW(
            hwnd,
            kFunctionPanelWidthProperty,
            reinterpret_cast<HANDLE>(static_cast<INT_PTR>(width)));
    } else {
        RemovePropW(hwnd, kFunctionPanelWidthProperty);
    }
}

inline void SetFunctionPanelVisibleProperty(HWND hwnd, bool visible)
{
    SetPropW(
        hwnd,
        kFunctionPanelVisibleProperty,
        reinterpret_cast<HANDLE>(visible ? kFunctionPanelVisibleValue : kFunctionPanelHiddenValue));
}

inline void SetFunctionPanelDockLeftProperty(HWND hwnd, bool dock_left)
{
    SetPropW(
        hwnd,
        kFunctionPanelDockLeftProperty,
        reinterpret_cast<HANDLE>(dock_left ? kFunctionPanelDockLeftValue : kFunctionPanelDockRightValue));
}

inline void RemoveFunctionPanelVisibleProperty(HWND hwnd)
{
    RemovePropW(hwnd, kFunctionPanelVisibleProperty);
}

inline void RemoveFunctionPanelDockLeftProperty(HWND hwnd)
{
    RemovePropW(hwnd, kFunctionPanelDockLeftProperty);
}

inline void RemoveFunctionPanelWidthProperty(HWND hwnd)
{
    RemovePropW(hwnd, kFunctionPanelWidthProperty);
}

// ---- Language property ---------------------------------------------------

constexpr const wchar_t* kLanguageProperty = L"CameraViewLanguage";
constexpr UINT_PTR kLanguageEnglishValue = 0;
constexpr UINT_PTR kLanguageChineseValue = 1;

inline void SetLanguageProperty(HWND hwnd, UILanguage lang)
{
    SetPropW(hwnd, kLanguageProperty,
             reinterpret_cast<HANDLE>(lang == UILanguage::Chinese ? kLanguageChineseValue : kLanguageEnglishValue));
}

inline UILanguage GetLanguageProperty(HWND hwnd)
{
    HANDLE value = GetPropW(hwnd, kLanguageProperty);
    return (value == reinterpret_cast<HANDLE>(kLanguageChineseValue)) ? UILanguage::Chinese : UILanguage::English;
}

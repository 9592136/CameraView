#pragma once

#include <windows.h>
#include <cstdlib>

#include "ui/UIConstants.h"
#include "ui/WindowLayout.h"
#include "ui/WindowProperties.h"

// These are defined here (before CameraViewCoordinator.h) so that the
// template methods in the coordinator headers can find them via
// ordinary lookup at instantiation time.
// The actual GetPreviewRect / GetStatusRect are defined later in this TU;
// they only differ from GetClientRect when panels are visible, so for the
// purpose of invalidation (which marks the whole client area) we just
// use GetClientRect here.
inline void InvalidatePreview(HWND hwnd)
{
    RECT rc{};
    GetClientRect(hwnd, &rc);
    InvalidateRect(hwnd, &rc, FALSE);
}

inline void InvalidateStatus(HWND hwnd)
{
    RECT rc{};
    GetClientRect(hwnd, &rc);
    InvalidateRect(hwnd, &rc, FALSE);
}

inline RECT GetPreviewRect(HWND hwnd)
{
    RECT rect = {};
    GetClientRect(hwnd, &rect);
    return WindowLayout::PreviewRect(
        rect,
        IsFunctionPanelVisible(hwnd),
        IsFunctionPanelDockedLeft(hwnd),
        FunctionPanelWidth(hwnd));
}

inline RECT GetSidePanelRect(HWND hwnd)
{
    RECT rect = {};
    GetClientRect(hwnd, &rect);
    return WindowLayout::SidePanelRect(
        rect,
        IsFunctionPanelVisible(hwnd),
        IsFunctionPanelDockedLeft(hwnd),
        FunctionPanelWidth(hwnd));
}

inline RECT GetStatusRect(HWND hwnd)
{
    RECT rect = {};
    GetClientRect(hwnd, &rect);
    return WindowLayout::StatusRect(rect);
}

void LayoutControls(HWND hwnd, bool repaint_children = true);

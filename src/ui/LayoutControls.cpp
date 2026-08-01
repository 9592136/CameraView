#include "ui/LayoutUtils.h"
#include "ui/CameraPreviewApp.h"
#include "ui/WindowControlLayout.h"

void LayoutControls(HWND hwnd, bool repaint_children)
{
    RECT client = {};
    GetClientRect(hwnd, &client);

    CameraPreviewApp* app = GetApp(hwnd);
    const int panel_category = app ? app->PanelCategory() : 0;
    const bool function_panel_visible =
        app ? app->FunctionPanelVisible() : IsFunctionPanelVisible(hwnd);
    const bool function_panel_docked_left =
        app ? app->FunctionPanelDockedLeft() : IsFunctionPanelDockedLeft(hwnd);
    const int function_panel_width = app ? app->FunctionPanelWidth() : FunctionPanelWidth(hwnd);
    if (app) {
        app->ClampPanelScroll();
    }
    const int panel_scroll_offset = app ? app->PanelScrollOffset() : 0;
    const std::vector<WindowControlPlacement> placements =
        WindowControlLayout::Compute(
            client,
            panel_category,
            panel_scroll_offset,
            function_panel_visible,
            function_panel_docked_left,
            function_panel_width);
    std::vector<HWND> paused_redraw_controls;
    if (!repaint_children) {
        paused_redraw_controls.reserve(placements.size());
        for (const WindowControlPlacement& placement : placements) {
            HWND control = GetDlgItem(hwnd, placement.control_id);
            if (control) {
                SendMessageW(control, WM_SETREDRAW, FALSE, 0);
                paused_redraw_controls.push_back(control);
            }
        }
    }

    HDWP deferred = BeginDeferWindowPos(static_cast<int>(placements.size()));
    for (const WindowControlPlacement& placement : placements) {
        HWND control = GetDlgItem(hwnd, placement.control_id);
        if (!control) {
            continue;
        }

        UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_DEFERERASE;
        if (placement.visible) {
            flags |= SWP_SHOWWINDOW;
            if (deferred) {
                HDWP next_deferred = DeferWindowPos(
                    deferred,
                    control,
                    nullptr,
                    placement.bounds.left,
                    placement.bounds.top,
                    placement.bounds.right - placement.bounds.left,
                    placement.bounds.bottom - placement.bounds.top,
                    flags);
                if (next_deferred) {
                    deferred = next_deferred;
                    continue;
                }
                EndDeferWindowPos(deferred);
                deferred = nullptr;
            }
            SetWindowPos(
                control,
                nullptr,
                placement.bounds.left,
                placement.bounds.top,
                placement.bounds.right - placement.bounds.left,
                placement.bounds.bottom - placement.bounds.top,
                flags);
        } else {
            flags |= SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE;
            if (deferred) {
                HDWP next_deferred = DeferWindowPos(
                    deferred,
                    control,
                    nullptr,
                    0,
                    0,
                    0,
                    0,
                    flags);
                if (next_deferred) {
                    deferred = next_deferred;
                    continue;
                }
                EndDeferWindowPos(deferred);
                deferred = nullptr;
            }
            SetWindowPos(control, nullptr, 0, 0, 0, 0, flags);
        }
    }
    if (deferred) {
        EndDeferWindowPos(deferred);
    }
    for (HWND control : paused_redraw_controls) {
        SendMessageW(control, WM_SETREDRAW, TRUE, 0);
    }
    if (app) {
        app->SyncPanelScrollBar();
    }
}

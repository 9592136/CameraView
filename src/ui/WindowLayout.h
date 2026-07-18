#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

class WindowLayout {
public:
    static int ToolbarHeight();
    static int StatusHeight();
    static int ComputeSidePanelWidth(const RECT& client_rect, bool show_side_panel = true);
    static RECT PreviewRect(
        const RECT& client_rect,
        bool show_side_panel = true,
        bool dock_panel_left = true);
    static RECT SidePanelRect(
        const RECT& client_rect,
        bool show_side_panel = true,
        bool dock_panel_left = true);
    static RECT StatusRect(const RECT& client_rect);
};

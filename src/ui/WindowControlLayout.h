#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>
#include <vector>

struct WindowControlPlacement {
    int control_id = 0;
    RECT bounds = {};
    bool visible = true;
};

class WindowControlLayout {
public:
    static std::vector<WindowControlPlacement> Compute(
        const RECT& client_rect,
        int panel_category = 0,
        int panel_scroll_offset = 0,
        bool show_side_panel = true,
        bool dock_panel_left = true);
    static int PanelScrollMax(
        const RECT& client_rect,
        int panel_category = 0,
        bool show_side_panel = true,
        bool dock_panel_left = true);
    static int PanelScrollPage(
        const RECT& client_rect,
        bool show_side_panel = true,
        bool dock_panel_left = true);
    static const std::vector<std::wstring>& PanelCategoryLabels();
    static int NormalizePanelCategory(int panel_category);
    static int PanelCategoryFromCardControl(int control_id);
    static const std::vector<int>& SideControlIds();
};

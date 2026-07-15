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
    static int ComputeSidePanelWidth(const RECT& client_rect);
    static RECT PreviewRect(const RECT& client_rect);
    static RECT SidePanelRect(const RECT& client_rect);
    static RECT StatusRect(const RECT& client_rect);
};

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "../domain/ImageFrame.h"
#include "ImageViewport.h"

#include <string>

struct ViewportPanState {
    bool active = false;
    POINT last_point = {};
};

struct ViewportPanBeginResult {
    bool started = false;
    bool capture_mouse = false;
};

struct ViewportPanContinueResult {
    bool handled = false;
    bool preview_changed = false;
};

class ViewportInteractionActions {
public:
    static bool ZoomAt(
        ImageViewport& viewport,
        const RECT& preview_rect,
        const ImageFrame& frame,
        POINT point,
        short wheel_delta);

    static std::wstring FormatZoomStatus(double zoom);
    static std::wstring FormatZoomValue(double zoom);

    static ViewportPanBeginResult BeginPan(
        ViewportPanState& pan_state,
        const RECT& preview_rect,
        const ImageFrame& frame,
        POINT point,
        bool edit_session_active);

    static ViewportPanContinueResult ContinuePan(
        ViewportPanState& pan_state,
        ImageViewport& viewport,
        const RECT& preview_rect,
        const ImageFrame& frame,
        POINT point);

    static bool EndPan(ViewportPanState& pan_state);
};

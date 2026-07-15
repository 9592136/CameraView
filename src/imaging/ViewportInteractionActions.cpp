#include "ViewportInteractionActions.h"

#include <cmath>
#include <string>

bool ViewportInteractionActions::ZoomAt(
    ImageViewport& viewport,
    const RECT& preview_rect,
    const ImageFrame& frame,
    POINT point,
    short wheel_delta)
{
    if (!PtInRect(&preview_rect, point) || wheel_delta == 0 || !frame.IsValid()) {
        return false;
    }
    return viewport.ZoomAt(preview_rect, frame, point, wheel_delta);
}

std::wstring ViewportInteractionActions::FormatZoomStatus(double zoom)
{
    return L"Zoom: " + FormatZoomValue(zoom) + L".";
}

std::wstring ViewportInteractionActions::FormatZoomValue(double zoom)
{
    const int percent = static_cast<int>(std::lround(zoom * 100.0));
    return std::to_wstring(percent) + L"%";
}

ViewportPanBeginResult ViewportInteractionActions::BeginPan(
    ViewportPanState& pan_state,
    const RECT& preview_rect,
    const ImageFrame& frame,
    POINT point,
    bool edit_session_active)
{
    ViewportPanBeginResult result;
    if (edit_session_active || !PtInRect(&preview_rect, point) || !frame.IsValid()) {
        return result;
    }

    pan_state.active = true;
    pan_state.last_point = point;
    result.started = true;
    result.capture_mouse = true;
    return result;
}

ViewportPanContinueResult ViewportInteractionActions::ContinuePan(
    ViewportPanState& pan_state,
    ImageViewport& viewport,
    const RECT& preview_rect,
    const ImageFrame& frame,
    POINT point)
{
    ViewportPanContinueResult result;
    if (!pan_state.active) {
        return result;
    }

    result.handled = true;
    if (!frame.IsValid()) {
        pan_state.last_point = point;
        return result;
    }

    const int delta_x = point.x - pan_state.last_point.x;
    const int delta_y = point.y - pan_state.last_point.y;
    pan_state.last_point = point;
    result.preview_changed = viewport.PanBy(preview_rect, frame, delta_x, delta_y);
    return result;
}

bool ViewportInteractionActions::EndPan(ViewportPanState& pan_state)
{
    if (!pan_state.active) {
        return false;
    }
    pan_state.active = false;
    return true;
}

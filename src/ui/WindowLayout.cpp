#include "WindowLayout.h"

#include <algorithm>

namespace {

constexpr int kToolbarHeight = 48;
constexpr int kStatusHeight = 28;
constexpr int kMinSidePanelWidth = 300;
constexpr int kDefaultMaxSidePanelWidth = 420;
constexpr int kUserMaxSidePanelWidth = 560;
constexpr int kMinPreviewWidth = 420;

} // namespace

int WindowLayout::ToolbarHeight()
{
    return kToolbarHeight;
}

int WindowLayout::StatusHeight()
{
    return kStatusHeight;
}

int WindowLayout::ComputeSidePanelWidth(
    const RECT& client_rect,
    bool show_side_panel,
    int requested_width)
{
    if (!show_side_panel) {
        return 0;
    }
    const int width = client_rect.right - client_rect.left;
    if (width < kMinPreviewWidth + kMinSidePanelWidth) {
        return 0;
    }
    const int scaled_width = width / 3;
    const int available_width = width - kMinPreviewWidth;
    const int default_width = std::min(
        available_width,
        std::clamp(scaled_width, kMinSidePanelWidth, kDefaultMaxSidePanelWidth));
    if (requested_width <= 0) {
        return default_width;
    }

    const int max_user_width = std::min(available_width, kUserMaxSidePanelWidth);
    return std::clamp(requested_width, kMinSidePanelWidth, max_user_width);
}

RECT WindowLayout::PreviewRect(
    const RECT& client_rect,
    bool show_side_panel,
    bool dock_panel_left,
    int requested_side_panel_width)
{
    RECT rect = client_rect;
    rect.top = kToolbarHeight;
    rect.bottom = std::max(rect.top, rect.bottom - kStatusHeight);
    const int side_panel_width = ComputeSidePanelWidth(rect, show_side_panel, requested_side_panel_width);
    if (dock_panel_left) {
        rect.left = std::min(rect.right, rect.left + side_panel_width);
    } else {
        rect.right = std::max(rect.left, rect.right - side_panel_width);
    }
    return rect;
}

RECT WindowLayout::SidePanelRect(
    const RECT& client_rect,
    bool show_side_panel,
    bool dock_panel_left,
    int requested_side_panel_width)
{
    RECT rect = client_rect;
    rect.top = kToolbarHeight;
    rect.bottom = std::max(rect.top, rect.bottom - kStatusHeight);
    const int side_panel_width = ComputeSidePanelWidth(rect, show_side_panel, requested_side_panel_width);
    if (dock_panel_left) {
        rect.right = std::min(rect.right, rect.left + side_panel_width);
    } else {
        rect.left = std::max(rect.left, rect.right - side_panel_width);
    }
    return rect;
}

RECT WindowLayout::StatusRect(const RECT& client_rect)
{
    RECT rect = client_rect;
    rect.top = std::max(rect.top, rect.bottom - kStatusHeight);
    return rect;
}

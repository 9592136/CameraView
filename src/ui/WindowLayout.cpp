#include "WindowLayout.h"

#include <algorithm>

namespace {

constexpr int kToolbarHeight = 48;
constexpr int kStatusHeight = 28;
constexpr int kMinSidePanelWidth = 300;
constexpr int kMaxSidePanelWidth = 420;
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

int WindowLayout::ComputeSidePanelWidth(const RECT& client_rect)
{
    const int width = client_rect.right - client_rect.left;
    if (width < kMinPreviewWidth + kMinSidePanelWidth) {
        return 0;
    }
    const int scaled_width = width / 3;
    const int available_width = width - kMinPreviewWidth;
    return std::min(
        available_width,
        std::clamp(scaled_width, kMinSidePanelWidth, kMaxSidePanelWidth));
}

RECT WindowLayout::PreviewRect(const RECT& client_rect)
{
    RECT rect = client_rect;
    rect.top = kToolbarHeight;
    rect.bottom = std::max(rect.top, rect.bottom - kStatusHeight);
    rect.right = std::max(rect.left, rect.right - ComputeSidePanelWidth(rect));
    return rect;
}

RECT WindowLayout::SidePanelRect(const RECT& client_rect)
{
    RECT rect = client_rect;
    rect.top = kToolbarHeight;
    rect.bottom = std::max(rect.top, rect.bottom - kStatusHeight);
    rect.left = rect.right - ComputeSidePanelWidth(rect);
    return rect;
}

RECT WindowLayout::StatusRect(const RECT& client_rect)
{
    RECT rect = client_rect;
    rect.top = std::max(rect.top, rect.bottom - kStatusHeight);
    return rect;
}

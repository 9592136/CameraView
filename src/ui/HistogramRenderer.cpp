#include "HistogramRenderer.h"

#include <algorithm>
#include <cstdio>

HistogramRenderer::HistogramRenderer() {}

HistogramRenderer::~HistogramRenderer()
{
    if (h_grid_pen_)   { DeleteObject(h_grid_pen_);   h_grid_pen_   = nullptr; }
    if (h_axis_pen_)   { DeleteObject(h_axis_pen_);   h_axis_pen_   = nullptr; }
    if (h_bg_brush_)   { DeleteObject(h_bg_brush_);   h_bg_brush_   = nullptr; }
    if (h_bar_pen_)    { DeleteObject(h_bar_pen_);    h_bar_pen_    = nullptr; }
    if (h_bar_brush_)  { DeleteObject(h_bar_brush_);  h_bar_brush_  = nullptr; }
}

void HistogramRenderer::EnsureTools()
{
    if (!h_grid_pen_)  h_grid_pen_  = CreatePen(PS_SOLID, 1, RGB(60, 60, 65));
    if (!h_axis_pen_)  h_axis_pen_  = CreatePen(PS_SOLID, 1, RGB(170, 170, 175));
    if (!h_bg_brush_)  h_bg_brush_  = CreateSolidBrush(RGB(32, 32, 36));
}

COLORREF HistogramRenderer::BarColor(HistogramChannel channel) const
{
    switch (channel) {
    case HistogramChannel::Luminance: return RGB(220, 220, 220);
    case HistogramChannel::Red:       return RGB(255, 80, 80);
    case HistogramChannel::Green:     return RGB(80, 255, 80);
    case HistogramChannel::Blue:      return RGB(80, 80, 255);
    }
    return RGB(220, 220, 220);
}

void HistogramRenderer::Draw(HDC hdc, const RECT& rect, const HistogramData& data,
                              HistogramChannel channel)
{
    if (!visible) return;

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return;

    EnsureTools();

    // Recreate channel-specific pen/brush only when channel changes
    if (!h_bar_pen_ || channel != last_channel_) {
        if (h_bar_pen_)   { DeleteObject(h_bar_pen_);   h_bar_pen_   = nullptr; }
        if (h_bar_brush_) { DeleteObject(h_bar_brush_); h_bar_brush_ = nullptr; }
        COLORREF c = BarColor(channel);
        h_bar_pen_   = CreatePen(PS_SOLID, 1, c);
        h_bar_brush_ = CreateSolidBrush(c);
        last_channel_ = channel;
    }

    // --- Background ---
    FillRect(hdc, &rect, h_bg_brush_);

    const int margin_left  = 6;
    const int margin_right = 6;
    const int margin_top   = 6;
    const int margin_bottom = 22;  // Reserve space for axis labels

    const int chart_left   = rect.left + margin_left;
    const int chart_right  = rect.right - margin_right;
    const int chart_top    = rect.top + margin_top;
    const int chart_bottom = rect.bottom - margin_bottom;
    const int chart_width  = chart_right - chart_left;
    const int chart_height = chart_bottom - chart_top;

    if (chart_width <= 0 || chart_height <= 0) return;

    // --- Grid lines (4 horizontal) ---
    HGDIOBJ old_pen = SelectObject(hdc, h_grid_pen_);
    for (int i = 0; i <= 4; ++i) {
        const int y = chart_bottom - (chart_height * i) / 4;
        MoveToEx(hdc, chart_left, y, nullptr);
        LineTo(hdc, chart_right, y);
    }
    // Vertical guides at shadows (quarter tones)
    for (int i = 0; i <= 4; ++i) {
        const int x = chart_left + (chart_width * i) / 4;
        MoveToEx(hdc, x, chart_top, nullptr);
        LineTo(hdc, x, chart_bottom);
    }

    // --- Axis ---
    SelectObject(hdc, h_axis_pen_);
    MoveToEx(hdc, chart_left, chart_bottom, nullptr);
    LineTo(hdc, chart_right, chart_bottom);
    MoveToEx(hdc, chart_left, chart_bottom, nullptr);
    LineTo(hdc, chart_left, chart_top);

    // --- X-axis labels (0, 128, 255) ---
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(140, 140, 145));
    HFONT small_font = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                    CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                    DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HGDIOBJ old_font = SelectObject(hdc, small_font);
    RECT label_rect = {chart_left, chart_bottom + 1, chart_left + 30, rect.bottom};
    DrawTextW(hdc, L"0", -1, &label_rect, DT_LEFT | DT_TOP | DT_SINGLELINE);
    label_rect = {chart_left + chart_width / 2 - 15, chart_bottom + 1,
                  chart_left + chart_width / 2 + 15, rect.bottom};
    DrawTextW(hdc, L"128", -1, &label_rect, DT_CENTER | DT_TOP | DT_SINGLELINE);
    label_rect = {chart_right - 30, chart_bottom + 1, chart_right, rect.bottom};
    DrawTextW(hdc, L"255", -1, &label_rect, DT_RIGHT | DT_TOP | DT_SINGLELINE);
    SelectObject(hdc, old_font);
    DeleteObject(small_font);

    // --- Bars ---
    if (data.max_count == 0) {
        SelectObject(hdc, old_pen);
        return;
    }

    const double scale     = static_cast<double>(chart_height) / static_cast<double>(data.max_count);
    const double bar_scale = static_cast<double>(chart_width) / 256.0;
    const int    bar_w     = std::max(1, static_cast<int>(bar_scale));

    SelectObject(hdc, h_bar_pen_);
    SelectObject(hdc, h_bar_brush_);

    if (bar_w == 1) {
        // Narrow mode: draw vertical lines
        for (int i = 0; i < 256; ++i) {
            if (data.bins[i] == 0) continue;
            const int bar_h = std::max(1, static_cast<int>(static_cast<double>(data.bins[i]) * scale));
            const int bar_x = chart_left + static_cast<int>(static_cast<double>(i) * bar_scale);
            MoveToEx(hdc, bar_x, chart_bottom, nullptr);
            LineTo(hdc, bar_x, chart_bottom - bar_h);
        }
    } else {
        // Wide mode: draw filled rectangles
        RECT bar_rect;
        for (int i = 0; i < 256; ++i) {
            if (data.bins[i] == 0) continue;
            const int bar_h = std::max(1, static_cast<int>(static_cast<double>(data.bins[i]) * scale));
            const int bar_x = chart_left + static_cast<int>(static_cast<double>(i) * bar_scale);
            bar_rect.left   = bar_x;
            bar_rect.top    = chart_bottom - bar_h;
            bar_rect.right  = bar_x + bar_w;
            bar_rect.bottom = chart_bottom;
            FillRect(hdc, &bar_rect, h_bar_brush_);
        }
    }

    // Restore stock objects (not our cached ones — they'll be reused)
    SelectObject(hdc, GetStockObject(NULL_PEN));
    SelectObject(hdc, GetStockObject(NULL_BRUSH));

    // --- Median marker line ---
    if (data.total_pixels > 0) {
        HPEN median_pen = CreatePen(PS_DOT, 1, RGB(255, 220, 100));
        SelectObject(hdc, median_pen);
        const int med_x = chart_left + static_cast<int>(static_cast<double>(data.stats.median) * bar_scale);
        MoveToEx(hdc, med_x, chart_top, nullptr);
        LineTo(hdc, med_x, chart_bottom);
        SelectObject(hdc, GetStockObject(NULL_PEN));
        DeleteObject(median_pen);
    }
}

void HistogramRenderer::DrawStats(HDC hdc, int x, int y, int width,
                                   const HistogramStats& stats, HistogramChannel channel)
{
    SetBkMode(hdc, TRANSPARENT);
    COLORREF color = BarColor(channel);

    WCHAR buf[128];
    swprintf_s(buf, L"Min: %d   Max: %d   Mean: %.1f   Median: %d",
               stats.min_value, stats.max_value, stats.mean, stats.median);

    // Draw across the width in three columns
    RECT r = {x, y, x + width, y + 18};
    SetTextColor(hdc, RGB(150, 150, 155));
    DrawTextW(hdc, buf, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

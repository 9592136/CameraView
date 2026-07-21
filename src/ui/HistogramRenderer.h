#pragma once

#include "../imaging/HistogramCalculator.h"

#include <Windows.h>

/// Renders a 256-bin histogram chart into a given rectangle using GDI.
class HistogramRenderer
{
public:
    HistogramRenderer();
    ~HistogramRenderer();

    /// Draw the histogram chart.
    /// @param hdc    Device context (back-buffer or paint DC).
    /// @param rect   Client rectangle to draw into.
    /// @param data   Pre-computed histogram data (including stats).
    /// @param channel Currently selected channel (used for bar colour).
    void Draw(HDC hdc, const RECT& rect, const HistogramData& data,
              HistogramChannel channel);

    /// Render stats summary below the chart.
    void DrawStats(HDC hdc, int x, int y, int width,
                   const HistogramStats& stats, HistogramChannel channel);

    bool visible = true;

private:
    COLORREF BarColor(HistogramChannel channel) const;

    void EnsureTools();

    // Cached GDI objects — created once, reused every frame.
    HPEN  h_grid_pen_      = nullptr;
    HPEN  h_axis_pen_      = nullptr;
    HBRUSH h_bg_brush_     = nullptr;
    HPEN  h_bar_pen_       = nullptr;    ///< Reused for current channel
    HBRUSH h_bar_brush_    = nullptr;    ///< Reused for current channel
    HistogramChannel last_channel_ = static_cast<HistogramChannel>(-1);
};

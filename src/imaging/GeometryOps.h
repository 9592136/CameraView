#pragma once
/*
 * GeometryOps.h
 * 无状态几何工具集，集中散落在各处的坐标/矩形样板，避免重复实现。
 * 注意：图像<->屏幕坐标的完整换算（含平移/缩放）已由 ImageViewport 提供，
 * 此处不重复实现，仅放真正跨文件复用的小工具。
 */
#include <windows.h>

#include <algorithm>
#include <cmath>

namespace imaging {

// 从 RECT 取得宽/高，替代散落的 rect.right - rect.left 样板。
inline int RectWidth(const RECT& rect) { return rect.right - rect.left; }
inline int RectHeight(const RECT& rect) { return rect.bottom - rect.top; }

// 计算保持比例的 fit-to-window 缩放，与 ViewTransform::ComputeBaseScale 同义。
// 集中到此处，避免 main.cpp 等重复手写该逻辑。
inline double FitScale(double image_width, double image_height,
                       double view_width, double view_height)
{
    if (image_width <= 0.0 || image_height <= 0.0 ||
        view_width <= 0.0 || view_height <= 0.0) {
        return 1.0;
    }
    return std::min(view_width / image_width, view_height / image_height);
}

} // namespace imaging

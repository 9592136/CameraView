#pragma once

#include <windows.h>

#include "../domain/ImageFrame.h"
#include "../domain/MeasurementCollection.h"
#include "../imaging/ImageViewport.h"

#include <cstddef>
#include <optional>

struct MeasurementHitTestResult {
    MeasurementReference measurement;
    EditablePoint point = EditablePoint::None;
    std::size_t flat_index = 0;
    std::size_t point_index = 0;
};

class MeasurementHitTester {
public:
    static std::optional<MeasurementHitTestResult> FindEditableHandle(
        const MeasurementCollection& measurements,
        ImageViewport& image_viewport,
        const RECT& preview,
        const ImageFrame& frame,
        POINT screen_point,
        int hit_radius_pixels = 10);

private:
    static int SquaredDistance(POINT first, POINT second);
};

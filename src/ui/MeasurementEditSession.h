#pragma once

#include "../domain/Geometry.h"
#include "../domain/MeasurementCollection.h"
#include "MeasurementHitTester.h"

#include <cstddef>
#include <optional>

class MeasurementEditSession {
public:
    bool IsActive() const { return active_; }

    void Begin(const MeasurementHitTestResult& hit);
    std::optional<MeasurementReference> ApplyTo(MeasurementCollection& measurements, ImagePoint image_point) const;
    void Clear();

private:
    bool active_ = false;
    MeasurementReference measurement_;
    EditablePoint point_ = EditablePoint::None;
    std::size_t point_index_ = 0;
};

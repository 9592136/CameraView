#include "MeasurementEditSession.h"

void MeasurementEditSession::Begin(const MeasurementHitTestResult& hit)
{
    active_ = true;
    measurement_ = hit.measurement;
    point_ = hit.point;
    point_index_ = hit.point_index;
}

std::optional<MeasurementReference> MeasurementEditSession::ApplyTo(
    MeasurementCollection& measurements,
    ImagePoint image_point) const
{
    if (!active_) {
        return std::nullopt;
    }

    if (!measurements.SetPoint(measurement_, point_, point_index_, image_point)) {
        return std::nullopt;
    }

    return measurement_;
}

void MeasurementEditSession::Clear()
{
    active_ = false;
    measurement_ = MeasurementReference();
    point_ = EditablePoint::None;
    point_index_ = 0;
}

#include "MeasurementOverlayModelBuilder.h"

MeasurementOverlayModel MeasurementOverlayModelBuilder::Build(
    const MeasurementCollection& measurements,
    const CalibrationProfile& calibration,
    MeasurementUnit display_unit,
    const MeasurementInteractionPending& pending)
{
    MeasurementOverlayModel model;
    model.calibration = &calibration;
    model.display_unit = display_unit;
    model.lengths = &measurements.Lengths();
    model.angles = &measurements.Angles();
    model.rectangles = &measurements.Rectangles();
    model.polygons = &measurements.Polygons();
    model.pending = ToOverlayPending(pending);
    return model;
}

MeasurementOverlayPending MeasurementOverlayModelBuilder::ToOverlayPending(
    const MeasurementInteractionPending& pending)
{
    MeasurementOverlayPending overlay_pending;
    switch (pending.kind) {
    case MeasurementInteractionPendingKind::Point:
        overlay_pending.kind = OverlayPendingKind::Point;
        overlay_pending.first = pending.first;
        break;
    case MeasurementInteractionPendingKind::Angle:
        overlay_pending.kind = OverlayPendingKind::Angle;
        overlay_pending.first = pending.first;
        overlay_pending.second = pending.second;
        break;
    case MeasurementInteractionPendingKind::Polygon:
        overlay_pending.kind = OverlayPendingKind::Polygon;
        overlay_pending.polygon_points = pending.polygon_points;
        break;
    case MeasurementInteractionPendingKind::None:
    default:
        overlay_pending.kind = OverlayPendingKind::None;
        break;
    }
    return overlay_pending;
}

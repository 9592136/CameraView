#include "MeasurementHitTester.h"

int MeasurementHitTester::SquaredDistance(POINT first, POINT second)
{
    const int dx = first.x - second.x;
    const int dy = first.y - second.y;
    return dx * dx + dy * dy;
}

std::optional<MeasurementHitTestResult> MeasurementHitTester::FindEditableHandle(
    const MeasurementCollection& measurements,
    ImageViewport& image_viewport,
    const RECT& preview,
    const ImageFrame& frame,
    POINT screen_point,
    int hit_radius_pixels)
{
    int best_distance = hit_radius_pixels * hit_radius_pixels;
    std::optional<MeasurementHitTestResult> found;

    auto consider = [&](MeasurementReference candidate, EditablePoint point, std::size_t candidate_point_index, ImagePoint image_point) {
        const POINT handle = image_viewport.ImageToScreen(preview, frame, image_point);
        const int distance = SquaredDistance(handle, screen_point);
        if (distance <= best_distance) {
            best_distance = distance;
            MeasurementHitTestResult result;
            result.measurement = candidate;
            result.point = point;
            result.flat_index = measurements.FlatIndexOf(candidate);
            result.point_index = candidate_point_index;
            found = result;
        }
    };

    const std::vector<LengthMeasurement>& lengths = measurements.Lengths();
    const std::vector<AngleMeasurement>& angles = measurements.Angles();
    const std::vector<RectangleAreaMeasurement>& rectangles = measurements.Rectangles();
    const std::vector<PolygonAreaMeasurement>& polygons = measurements.Polygons();

    for (std::size_t index = 0; index < lengths.size(); ++index) {
        const MeasurementReference reference{MeasurementKind::Length, index};
        consider(reference, EditablePoint::First, 0, lengths[index].First());
        consider(reference, EditablePoint::Second, 1, lengths[index].Second());
    }
    for (std::size_t index = 0; index < angles.size(); ++index) {
        const MeasurementReference reference{MeasurementKind::Angle, index};
        consider(reference, EditablePoint::First, 0, angles[index].First());
        consider(reference, EditablePoint::Vertex, 1, angles[index].Vertex());
        consider(reference, EditablePoint::Second, 2, angles[index].Second());
    }
    for (std::size_t index = 0; index < rectangles.size(); ++index) {
        const MeasurementReference reference{MeasurementKind::RectangleArea, index};
        consider(reference, EditablePoint::First, 0, rectangles[index].First());
        consider(reference, EditablePoint::Second, 1, rectangles[index].Second());
    }
    for (std::size_t index = 0; index < polygons.size(); ++index) {
        const MeasurementReference reference{MeasurementKind::PolygonArea, index};
        const std::vector<ImagePoint>& points = polygons[index].Points();
        for (std::size_t point = 0; point < points.size(); ++point) {
            consider(reference, EditablePoint::First, point, points[point]);
        }
    }

    return found;
}

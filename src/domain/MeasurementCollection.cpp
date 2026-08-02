#include "MeasurementCollection.h"

#include <utility>

std::size_t MeasurementCollection::Count() const
{
    return lengths_.size() + angles_.size() + rectangles_.size() + polygons_.size() +
        points_.size() + polylines_.size() + circles_.size() + ellipses_.size();
}

LengthMeasurement& MeasurementCollection::AddLength(std::wstring name, ImagePoint first, ImagePoint second)
{
    lengths_.emplace_back(std::move(name), first, second);
    return lengths_.back();
}

AngleMeasurement& MeasurementCollection::AddAngle(std::wstring name, ImagePoint first, ImagePoint vertex, ImagePoint second)
{
    angles_.emplace_back(std::move(name), first, vertex, second);
    return angles_.back();
}

RectangleAreaMeasurement& MeasurementCollection::AddRectangleArea(std::wstring name, ImagePoint first, ImagePoint second)
{
    rectangles_.emplace_back(std::move(name), first, second);
    return rectangles_.back();
}

PolygonAreaMeasurement& MeasurementCollection::AddPolygonArea(std::wstring name, std::vector<ImagePoint> points)
{
    polygons_.emplace_back(std::move(name), std::move(points));
    return polygons_.back();
}

PointMeasurement& MeasurementCollection::AddPoint(std::wstring name, ImagePoint point)
{
    points_.emplace_back(std::move(name), point);
    return points_.back();
}

PolylineMeasurement& MeasurementCollection::AddPolyline(std::wstring name, std::vector<ImagePoint> points)
{
    polylines_.emplace_back(std::move(name), std::move(points));
    return polylines_.back();
}

CircleMeasurement& MeasurementCollection::AddCircle(std::wstring name, ImagePoint center, ImagePoint edge)
{
    circles_.emplace_back(std::move(name), center, edge);
    return circles_.back();
}

EllipseMeasurement& MeasurementCollection::AddEllipse(std::wstring name, ImagePoint first, ImagePoint second)
{
    ellipses_.emplace_back(std::move(name), first, second);
    return ellipses_.back();
}

void MeasurementCollection::Clear()
{
    lengths_.clear();
    angles_.clear();
    rectangles_.clear();
    polygons_.clear();
    points_.clear();
    polylines_.clear();
    circles_.clear();
    ellipses_.clear();
}

void MeasurementCollection::SetAll(
    std::vector<LengthMeasurement> lengths,
    std::vector<AngleMeasurement> angles,
    std::vector<RectangleAreaMeasurement> rectangles,
    std::vector<PolygonAreaMeasurement> polygons,
    std::vector<PointMeasurement> points,
    std::vector<PolylineMeasurement> polylines,
    std::vector<CircleMeasurement> circles,
    std::vector<EllipseMeasurement> ellipses)
{
    lengths_ = std::move(lengths);
    angles_ = std::move(angles);
    rectangles_ = std::move(rectangles);
    polygons_ = std::move(polygons);
    points_ = std::move(points);
    polylines_ = std::move(polylines);
    circles_ = std::move(circles);
    ellipses_ = std::move(ellipses);
}

std::optional<MeasurementReference> MeasurementCollection::AtFlatIndex(std::size_t selection) const
{
    if (selection < lengths_.size()) {
        return MeasurementReference{MeasurementKind::Length, selection};
    }
    selection -= lengths_.size();
    if (selection < angles_.size()) {
        return MeasurementReference{MeasurementKind::Angle, selection};
    }
    selection -= angles_.size();
    if (selection < rectangles_.size()) {
        return MeasurementReference{MeasurementKind::RectangleArea, selection};
    }
    selection -= rectangles_.size();
    if (selection < polygons_.size()) {
        return MeasurementReference{MeasurementKind::PolygonArea, selection};
    }
    selection -= polygons_.size();
    if (selection < points_.size()) return MeasurementReference{MeasurementKind::Point, selection};
    selection -= points_.size();
    if (selection < polylines_.size()) return MeasurementReference{MeasurementKind::Polyline, selection};
    selection -= polylines_.size();
    if (selection < circles_.size()) return MeasurementReference{MeasurementKind::Circle, selection};
    selection -= circles_.size();
    if (selection < ellipses_.size()) return MeasurementReference{MeasurementKind::Ellipse, selection};
    return std::nullopt;
}

std::size_t MeasurementCollection::FlatIndexOf(MeasurementReference reference) const
{
    switch (reference.kind) {
    case MeasurementKind::Length:
        return reference.index;
    case MeasurementKind::Angle:
        return lengths_.size() + reference.index;
    case MeasurementKind::RectangleArea:
        return lengths_.size() + angles_.size() + reference.index;
    case MeasurementKind::PolygonArea:
        return lengths_.size() + angles_.size() + rectangles_.size() + reference.index;
    case MeasurementKind::Point:
        return lengths_.size() + angles_.size() + rectangles_.size() + polygons_.size() + reference.index;
    case MeasurementKind::Polyline:
        return lengths_.size() + angles_.size() + rectangles_.size() + polygons_.size() + points_.size() + reference.index;
    case MeasurementKind::Circle:
        return lengths_.size() + angles_.size() + rectangles_.size() + polygons_.size() + points_.size() + polylines_.size() + reference.index;
    case MeasurementKind::Ellipse:
        return lengths_.size() + angles_.size() + rectangles_.size() + polygons_.size() + points_.size() + polylines_.size() + circles_.size() + reference.index;
    case MeasurementKind::None:
    default:
        return Count();
    }
}

std::wstring MeasurementCollection::Name(MeasurementReference reference) const
{
    switch (reference.kind) {
    case MeasurementKind::Length:
        if (reference.index < lengths_.size()) {
            return lengths_[reference.index].Name();
        }
        break;
    case MeasurementKind::Angle:
        if (reference.index < angles_.size()) {
            return angles_[reference.index].Name();
        }
        break;
    case MeasurementKind::RectangleArea:
        if (reference.index < rectangles_.size()) {
            return rectangles_[reference.index].Name();
        }
        break;
    case MeasurementKind::PolygonArea:
        if (reference.index < polygons_.size()) {
            return polygons_[reference.index].Name();
        }
        break;
    case MeasurementKind::Point:
        if (reference.index < points_.size()) return points_[reference.index].Name();
        break;
    case MeasurementKind::Polyline:
        if (reference.index < polylines_.size()) return polylines_[reference.index].Name();
        break;
    case MeasurementKind::Circle:
        if (reference.index < circles_.size()) return circles_[reference.index].Name();
        break;
    case MeasurementKind::Ellipse:
        if (reference.index < ellipses_.size()) return ellipses_[reference.index].Name();
        break;
    case MeasurementKind::None:
    default:
        break;
    }
    return {};
}

bool MeasurementCollection::SetName(MeasurementReference reference, const std::wstring& name)
{
    switch (reference.kind) {
    case MeasurementKind::Length:
        if (reference.index < lengths_.size()) {
            lengths_[reference.index].SetName(name);
            return true;
        }
        break;
    case MeasurementKind::Angle:
        if (reference.index < angles_.size()) {
            angles_[reference.index].SetName(name);
            return true;
        }
        break;
    case MeasurementKind::RectangleArea:
        if (reference.index < rectangles_.size()) {
            rectangles_[reference.index].SetName(name);
            return true;
        }
        break;
    case MeasurementKind::PolygonArea:
        if (reference.index < polygons_.size()) {
            polygons_[reference.index].SetName(name);
            return true;
        }
        break;
    case MeasurementKind::Point:
        if (reference.index < points_.size()) { points_[reference.index].SetName(name); return true; }
        break;
    case MeasurementKind::Polyline:
        if (reference.index < polylines_.size()) { polylines_[reference.index].SetName(name); return true; }
        break;
    case MeasurementKind::Circle:
        if (reference.index < circles_.size()) { circles_[reference.index].SetName(name); return true; }
        break;
    case MeasurementKind::Ellipse:
        if (reference.index < ellipses_.size()) { ellipses_[reference.index].SetName(name); return true; }
        break;
    case MeasurementKind::None:
    default:
        break;
    }
    return false;
}

bool MeasurementCollection::SetPoint(
    MeasurementReference reference,
    EditablePoint point,
    std::size_t point_index,
    ImagePoint image_point)
{
    switch (reference.kind) {
    case MeasurementKind::Length:
        if (reference.index < lengths_.size()) {
            if (point == EditablePoint::First) {
                lengths_[reference.index].SetFirst(image_point);
                return true;
            }
            if (point == EditablePoint::Second) {
                lengths_[reference.index].SetSecond(image_point);
                return true;
            }
        }
        break;
    case MeasurementKind::Angle:
        if (reference.index < angles_.size()) {
            if (point == EditablePoint::First) {
                angles_[reference.index].SetFirst(image_point);
                return true;
            }
            if (point == EditablePoint::Vertex) {
                angles_[reference.index].SetVertex(image_point);
                return true;
            }
            if (point == EditablePoint::Second) {
                angles_[reference.index].SetSecond(image_point);
                return true;
            }
        }
        break;
    case MeasurementKind::RectangleArea:
        if (reference.index < rectangles_.size()) {
            if (point == EditablePoint::First) {
                rectangles_[reference.index].SetFirst(image_point);
                return true;
            }
            if (point == EditablePoint::Second) {
                rectangles_[reference.index].SetSecond(image_point);
                return true;
            }
        }
        break;
    case MeasurementKind::PolygonArea:
        if (reference.index < polygons_.size()) {
            polygons_[reference.index].SetPoint(point_index, image_point);
            return point_index < polygons_[reference.index].Points().size();
        }
        break;
    case MeasurementKind::Point:
        if (reference.index < points_.size()) { points_[reference.index].SetPoint(image_point); return true; }
        break;
    case MeasurementKind::Polyline:
        if (reference.index < polylines_.size() && point_index < polylines_[reference.index].Points().size()) {
            polylines_[reference.index].SetPoint(point_index, image_point); return true;
        }
        break;
    case MeasurementKind::Circle:
        if (reference.index < circles_.size()) {
            if (point == EditablePoint::First) { circles_[reference.index].SetCenter(image_point); return true; }
            if (point == EditablePoint::Second) { circles_[reference.index].SetEdge(image_point); return true; }
        }
        break;
    case MeasurementKind::Ellipse:
        if (reference.index < ellipses_.size()) {
            if (point == EditablePoint::First) { ellipses_[reference.index].SetFirst(image_point); return true; }
            if (point == EditablePoint::Second) { ellipses_[reference.index].SetSecond(image_point); return true; }
        }
        break;
    case MeasurementKind::None:
    default:
        break;
    }
    return false;
}

bool MeasurementCollection::EraseAtFlatIndex(std::size_t selection)
{
    if (selection < lengths_.size()) {
        lengths_.erase(lengths_.begin() + static_cast<std::ptrdiff_t>(selection));
        return true;
    }
    selection -= lengths_.size();
    if (selection < angles_.size()) {
        angles_.erase(angles_.begin() + static_cast<std::ptrdiff_t>(selection));
        return true;
    }
    selection -= angles_.size();
    if (selection < rectangles_.size()) {
        rectangles_.erase(rectangles_.begin() + static_cast<std::ptrdiff_t>(selection));
        return true;
    }
    selection -= rectangles_.size();
    if (selection < polygons_.size()) {
        polygons_.erase(polygons_.begin() + static_cast<std::ptrdiff_t>(selection));
        return true;
    }
    selection -= polygons_.size();
    if (selection < points_.size()) { points_.erase(points_.begin() + static_cast<std::ptrdiff_t>(selection)); return true; }
    selection -= points_.size();
    if (selection < polylines_.size()) { polylines_.erase(polylines_.begin() + static_cast<std::ptrdiff_t>(selection)); return true; }
    selection -= polylines_.size();
    if (selection < circles_.size()) { circles_.erase(circles_.begin() + static_cast<std::ptrdiff_t>(selection)); return true; }
    selection -= circles_.size();
    if (selection < ellipses_.size()) { ellipses_.erase(ellipses_.begin() + static_cast<std::ptrdiff_t>(selection)); return true; }
    return false;
}

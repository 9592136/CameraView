#pragma once

#include "Measurement.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

enum class MeasurementKind {
    None,
    Length,
    Angle,
    RectangleArea,
    PolygonArea,
    Point,
    Polyline,
    Circle,
    Ellipse
};

enum class EditablePoint {
    None,
    First,
    Vertex,
    Second
};

struct MeasurementReference {
    MeasurementKind kind = MeasurementKind::None;
    std::size_t index = 0;
};

class MeasurementCollection {
public:
    const std::vector<LengthMeasurement>& Lengths() const { return lengths_; }
    const std::vector<AngleMeasurement>& Angles() const { return angles_; }
    const std::vector<RectangleAreaMeasurement>& Rectangles() const { return rectangles_; }
    const std::vector<PolygonAreaMeasurement>& Polygons() const { return polygons_; }
    const std::vector<PointMeasurement>& Points() const { return points_; }
    const std::vector<PolylineMeasurement>& Polylines() const { return polylines_; }
    const std::vector<CircleMeasurement>& Circles() const { return circles_; }
    const std::vector<EllipseMeasurement>& Ellipses() const { return ellipses_; }
    const std::vector<MeasurementOverlayStyle>& Styles() const { return styles_; }

    std::size_t LengthCount() const { return lengths_.size(); }
    std::size_t AngleCount() const { return angles_.size(); }
    std::size_t RectangleCount() const { return rectangles_.size(); }
    std::size_t PolygonCount() const { return polygons_.size(); }
    std::size_t PointCount() const { return points_.size(); }
    std::size_t PolylineCount() const { return polylines_.size(); }
    std::size_t CircleCount() const { return circles_.size(); }
    std::size_t EllipseCount() const { return ellipses_.size(); }
    std::size_t Count() const;
    bool Empty() const { return Count() == 0; }

    LengthMeasurement& AddLength(std::wstring name, ImagePoint first, ImagePoint second);
    AngleMeasurement& AddAngle(std::wstring name, ImagePoint first, ImagePoint vertex, ImagePoint second);
    RectangleAreaMeasurement& AddRectangleArea(std::wstring name, ImagePoint first, ImagePoint second);
    PolygonAreaMeasurement& AddPolygonArea(std::wstring name, std::vector<ImagePoint> points);
    PointMeasurement& AddPoint(std::wstring name, ImagePoint point);
    PolylineMeasurement& AddPolyline(std::wstring name, std::vector<ImagePoint> points);
    CircleMeasurement& AddCircle(std::wstring name, ImagePoint center, ImagePoint edge);
    EllipseMeasurement& AddEllipse(std::wstring name, ImagePoint first, ImagePoint second);

    void Clear();
    void SetAll(
        std::vector<LengthMeasurement> lengths,
        std::vector<AngleMeasurement> angles,
        std::vector<RectangleAreaMeasurement> rectangles,
        std::vector<PolygonAreaMeasurement> polygons,
        std::vector<PointMeasurement> points = {},
        std::vector<PolylineMeasurement> polylines = {},
        std::vector<CircleMeasurement> circles = {},
        std::vector<EllipseMeasurement> ellipses = {});
    void SetStyles(std::vector<MeasurementOverlayStyle> styles);

    std::optional<MeasurementReference> AtFlatIndex(std::size_t selection) const;
    std::size_t FlatIndexOf(MeasurementReference reference) const;
    std::wstring Name(MeasurementReference reference) const;
    bool SetName(MeasurementReference reference, const std::wstring& name);
    bool SetPoint(MeasurementReference reference, EditablePoint point, std::size_t point_index, ImagePoint image_point);
    MeasurementOverlayStyle Style(MeasurementReference reference) const;
    bool SetStyle(MeasurementReference reference, MeasurementOverlayStyle style);
    bool Translate(MeasurementReference reference, ImagePoint delta);
    bool EraseAtFlatIndex(std::size_t selection);

    static MeasurementOverlayStyle DefaultStyle(MeasurementKind kind);

private:
    std::vector<LengthMeasurement> lengths_;
    std::vector<AngleMeasurement> angles_;
    std::vector<RectangleAreaMeasurement> rectangles_;
    std::vector<PolygonAreaMeasurement> polygons_;
    std::vector<PointMeasurement> points_;
    std::vector<PolylineMeasurement> polylines_;
    std::vector<CircleMeasurement> circles_;
    std::vector<EllipseMeasurement> ellipses_;
    std::vector<MeasurementOverlayStyle> styles_;
};

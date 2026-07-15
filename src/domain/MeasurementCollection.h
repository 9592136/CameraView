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
    PolygonArea
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

    std::size_t LengthCount() const { return lengths_.size(); }
    std::size_t AngleCount() const { return angles_.size(); }
    std::size_t RectangleCount() const { return rectangles_.size(); }
    std::size_t PolygonCount() const { return polygons_.size(); }
    std::size_t Count() const;
    bool Empty() const { return Count() == 0; }

    LengthMeasurement& AddLength(std::wstring name, ImagePoint first, ImagePoint second);
    AngleMeasurement& AddAngle(std::wstring name, ImagePoint first, ImagePoint vertex, ImagePoint second);
    RectangleAreaMeasurement& AddRectangleArea(std::wstring name, ImagePoint first, ImagePoint second);
    PolygonAreaMeasurement& AddPolygonArea(std::wstring name, std::vector<ImagePoint> points);

    void Clear();
    void SetAll(
        std::vector<LengthMeasurement> lengths,
        std::vector<AngleMeasurement> angles,
        std::vector<RectangleAreaMeasurement> rectangles,
        std::vector<PolygonAreaMeasurement> polygons);

    std::optional<MeasurementReference> AtFlatIndex(std::size_t selection) const;
    std::size_t FlatIndexOf(MeasurementReference reference) const;
    std::wstring Name(MeasurementReference reference) const;
    bool SetName(MeasurementReference reference, const std::wstring& name);
    bool SetPoint(MeasurementReference reference, EditablePoint point, std::size_t point_index, ImagePoint image_point);
    bool EraseAtFlatIndex(std::size_t selection);

private:
    std::vector<LengthMeasurement> lengths_;
    std::vector<AngleMeasurement> angles_;
    std::vector<RectangleAreaMeasurement> rectangles_;
    std::vector<PolygonAreaMeasurement> polygons_;
};

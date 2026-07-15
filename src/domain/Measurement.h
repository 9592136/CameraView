#pragma once

#include "CalibrationProfile.h"
#include "Geometry.h"

#include <cstddef>
#include <string>
#include <vector>

struct MeasurementResult {
    std::wstring name;
    std::wstring kind;
    double pixel_value = 0.0;
    double calibrated_value = 0.0;
    MeasurementUnit unit = MeasurementUnit::Pixels;
    std::wstring unit_label;
};

class LengthMeasurement {
public:
    LengthMeasurement() = default;
    LengthMeasurement(std::wstring name, ImagePoint first, ImagePoint second);

    const std::wstring& Name() const { return name_; }
    ImagePoint First() const { return first_; }
    ImagePoint Second() const { return second_; }
    void SetName(std::wstring name);
    void SetFirst(ImagePoint point);
    void SetSecond(ImagePoint point);

    double PixelLength() const;
    MeasurementResult Evaluate(const CalibrationProfile& calibration, MeasurementUnit output_unit) const;

private:
    std::wstring name_;
    ImagePoint first_;
    ImagePoint second_;
};

class AngleMeasurement {
public:
    AngleMeasurement() = default;
    AngleMeasurement(std::wstring name, ImagePoint first, ImagePoint vertex, ImagePoint second);

    const std::wstring& Name() const { return name_; }
    ImagePoint First() const { return first_; }
    ImagePoint Vertex() const { return vertex_; }
    ImagePoint Second() const { return second_; }
    void SetName(std::wstring name);
    void SetFirst(ImagePoint point);
    void SetVertex(ImagePoint point);
    void SetSecond(ImagePoint point);

    double Degrees() const;
    MeasurementResult Evaluate() const;

private:
    std::wstring name_;
    ImagePoint first_;
    ImagePoint vertex_;
    ImagePoint second_;
};

class RectangleAreaMeasurement {
public:
    RectangleAreaMeasurement() = default;
    RectangleAreaMeasurement(std::wstring name, ImagePoint first, ImagePoint second);

    const std::wstring& Name() const { return name_; }
    ImagePoint First() const { return first_; }
    ImagePoint Second() const { return second_; }
    void SetName(std::wstring name);
    void SetFirst(ImagePoint point);
    void SetSecond(ImagePoint point);

    double PixelArea() const;
    MeasurementResult Evaluate(const CalibrationProfile& calibration, MeasurementUnit output_unit) const;

private:
    std::wstring name_;
    ImagePoint first_;
    ImagePoint second_;
};

class PolygonAreaMeasurement {
public:
    PolygonAreaMeasurement() = default;
    PolygonAreaMeasurement(std::wstring name, std::vector<ImagePoint> points);

    const std::wstring& Name() const { return name_; }
    const std::vector<ImagePoint>& Points() const { return points_; }
    void SetName(std::wstring name);
    void SetPoints(std::vector<ImagePoint> points);
    void SetPoint(std::size_t index, ImagePoint point);

    double PixelArea() const;
    MeasurementResult Evaluate(const CalibrationProfile& calibration, MeasurementUnit output_unit) const;

private:
    std::wstring name_;
    std::vector<ImagePoint> points_;
};

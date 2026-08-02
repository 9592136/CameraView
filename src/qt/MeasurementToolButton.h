#pragma once

#include <QIcon>
#include <QToolButton>

enum class MeasurementToolGlyph {
    Calibration,
    Point,
    Length,
    Profile,
    Angle,
    Rectangle,
    Polygon,
    Polyline,
    Circle,
    Ellipse,
    SmartCount,
    SmartCountRun,
    EdgeSnap,
    DeleteMeasurement,
    ClearMeasurements,
    ExportCsv
};

QIcon measurementToolIcon(MeasurementToolGlyph glyph);

class MeasurementToolButton final : public QToolButton {
public:
    MeasurementToolButton(
        MeasurementToolGlyph glyph,
        const QString& text,
        const QString& toolTip,
        QWidget* parent = nullptr);

    MeasurementToolGlyph glyph() const { return glyph_; }

private:
    MeasurementToolGlyph glyph_;
};

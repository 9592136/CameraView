#pragma once

#include "domain/CalibrationProfile.h"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLabel;

class CalibrationDialog final : public QDialog {
public:
    CalibrationDialog(
        double pixelDistance,
        double initialRealLength,
        MeasurementUnit initialUnit,
        QWidget* parent = nullptr);

    double realLength() const;
    MeasurementUnit unit() const;
    CalibrationProfile profile() const;

private:
    void updatePreview();

    double pixel_distance_ = 0.0;
    QDoubleSpinBox* length_spin_ = nullptr;
    QComboBox* unit_combo_ = nullptr;
    QLabel* scale_preview_label_ = nullptr;
};

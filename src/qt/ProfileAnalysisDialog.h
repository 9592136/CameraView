#pragma once

#include "domain/CalibrationProfile.h"
#include "imaging/ImageProfileSampler.h"

#include <QDialog>

class QLabel;
class ProfilePlotWidget;
class QComboBox;

class ProfileAnalysisDialog final : public QDialog {
    Q_OBJECT

public:
    ProfileAnalysisDialog(
        ImageFrame frame,
        ImagePoint first,
        ImagePoint second,
        CalibrationProfile calibration,
        MeasurementUnit displayUnit,
        const QString& sourceName,
        QWidget* parent = nullptr);

    const ImageProfileResult& profile() const { return profile_; }
    ProfilePlotWidget* plotWidget() const { return plot_; }

private:
    void recomputeProfile();
    void exportCsv();
    void exportPlot();
    double distanceScale() const;
    QString distanceUnitLabel() const;

    ImageFrame frame_;
    ImagePoint first_;
    ImagePoint second_;
    CalibrationProfile calibration_;
    MeasurementUnit display_unit_ = MeasurementUnit::Pixels;
    ImageProfileResult profile_;
    QComboBox* channel_combo_ = nullptr;
    QLabel* summary_label_ = nullptr;
    ProfilePlotWidget* plot_ = nullptr;
};

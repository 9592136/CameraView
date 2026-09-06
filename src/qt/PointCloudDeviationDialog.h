#pragma once

#include "pointcloud/PointCloudDeviationDistribution.h"

#include <QDialog>
#include <QWidget>

class QLabel;

class PointCloudDeviationPlotWidget final : public QWidget {
    Q_OBJECT
public:
    explicit PointCloudDeviationPlotWidget(QWidget* parent = nullptr);
    void setDistribution(
        const PointCloudDeviationDistribution& distribution,
        const QString& unit);
    int binCount() const { return static_cast<int>(distribution_.bins.size()); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    PointCloudDeviationDistribution distribution_;
    QString unit_;
};

class PointCloudDeviationDialog final : public QDialog {
    Q_OBJECT
public:
    PointCloudDeviationDialog(
        PointCloudDeviationDistribution distribution,
        const QString& unit,
        QWidget* parent = nullptr);
    const PointCloudDeviationDistribution& distribution() const { return distribution_; }
    PointCloudDeviationPlotWidget* plotWidget() const { return plot_; }

private:
    void exportCsv();
    PointCloudDeviationDistribution distribution_;
    QString unit_;
    QLabel* summary_ = nullptr;
    PointCloudDeviationPlotWidget* plot_ = nullptr;
};

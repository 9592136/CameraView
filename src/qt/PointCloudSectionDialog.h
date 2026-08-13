#pragma once

#include "pointcloud/PointCloudSection.h"

#include <QDialog>
#include <QWidget>

class QLabel;

class PointCloudSectionPlotWidget final : public QWidget {
    Q_OBJECT
public:
    explicit PointCloudSectionPlotWidget(QWidget* parent = nullptr);
    void setProfile(const PointCloudSectionProfile& profile, const QString& unit);
    int sampleCount() const { return static_cast<int>(profile_.samples.size()); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    PointCloudSectionProfile profile_;
    QString unit_;
};

class PointCloudSectionDialog final : public QDialog {
    Q_OBJECT
public:
    PointCloudSectionDialog(
        PointCloudSectionProfile profile,
        const QString& unit,
        QWidget* parent = nullptr);
    const PointCloudSectionProfile& profile() const { return profile_; }
    PointCloudSectionPlotWidget* plotWidget() const { return plot_; }

private:
    void exportCsv();
    PointCloudSectionProfile profile_;
    QString unit_;
    QLabel* summary_ = nullptr;
    PointCloudSectionPlotWidget* plot_ = nullptr;
};

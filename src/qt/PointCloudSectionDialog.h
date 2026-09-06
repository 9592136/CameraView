#pragma once

#include "pointcloud/PointCloudSection.h"

#include <QDialog>
#include <QPointF>
#include <QWidget>

#include <cstdint>
#include <vector>

class QLabel;

class PointCloudSectionPlotWidget final : public QWidget {
    Q_OBJECT
public:
    explicit PointCloudSectionPlotWidget(QWidget* parent = nullptr);
    void setProfile(const PointCloudSectionProfile& profile, const QString& unit);
    void setProfiles(
        const std::vector<PointCloudSectionProfile>& profiles,
        std::uint64_t activeProfileId,
        const QString& unit);
    void setCursors(double firstDistance, double secondDistance);
    int sampleCount() const { return static_cast<int>(profile_.samples.size()); }

signals:
    void cursorPositionsChanged(double firstDistance, double secondDistance);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QRectF plotRect() const;
    double distanceFromX(double x) const;
    void resetViewRange();
    PointCloudSectionProfile profile_;
    std::vector<PointCloudSectionProfile> profiles_;
    std::uint64_t active_profile_id_ = 0;
    QString unit_;
    double cursor_first_ = 0.0;
    double cursor_second_ = 0.0;
    double view_minimum_ = 0.0;
    double view_maximum_ = 0.0;
    int dragged_cursor_ = 0;
    QPointF hover_position_;
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

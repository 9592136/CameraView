#pragma once

#include "imaging/ImageProfileSampler.h"

#include <QString>
#include <QWidget>

class ProfilePlotWidget final : public QWidget {
    Q_OBJECT

public:
    explicit ProfilePlotWidget(QWidget* parent = nullptr);

    void setProfile(
        const ImageProfileResult& profile,
        double distanceScale,
        const QString& distanceUnit,
        const QString& channelLabel);
    int profileSampleCount() const { return static_cast<int>(profile_.samples.size()); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QRectF plotRect() const;

    ImageProfileResult profile_;
    double distance_scale_ = 1.0;
    QString distance_unit_ = QStringLiteral("px");
    QString channel_label_;
    int hover_index_ = -1;
};

#include "ProfilePlotWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

ProfilePlotWidget::ProfilePlotWidget(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("ProfilePlot"));
    setMinimumSize(640, 360);
    setMouseTracking(true);
}

void ProfilePlotWidget::setProfile(
    const ImageProfileResult& profile,
    double distanceScale,
    const QString& distanceUnit,
    const QString& channelLabel)
{
    profile_ = profile;
    distance_scale_ = distanceScale > 0.0 ? distanceScale : 1.0;
    distance_unit_ = distanceUnit;
    channel_label_ = channelLabel;
    hover_index_ = -1;
    update();
}

QRectF ProfilePlotWidget::plotRect() const
{
    return QRectF(rect()).adjusted(66.0, 28.0, -28.0, -58.0);
}

void ProfilePlotWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(12, 18, 25));
    const QRectF plot = plotRect();
    painter.fillRect(plot, QColor(15, 23, 32));

    painter.setPen(QPen(QColor(53, 67, 84), 1.0));
    for (int step = 0; step <= 5; ++step) {
        const double y = plot.bottom() - plot.height() * step / 5.0;
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.setPen(QColor(149, 162, 180));
        painter.drawText(
            QRectF(5.0, y - 9.0, 52.0, 18.0),
            Qt::AlignRight | Qt::AlignVCenter,
            QString::number(qRound(255.0 * step / 5.0)));
        painter.setPen(QPen(QColor(53, 67, 84), 1.0));
    }
    for (int step = 0; step <= 5; ++step) {
        const double x = plot.left() + plot.width() * step / 5.0;
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        const double distance = profile_.pixel_length * distance_scale_ * step / 5.0;
        painter.setPen(QColor(149, 162, 180));
        painter.drawText(
            QRectF(x - 38.0, plot.bottom() + 7.0, 76.0, 18.0),
            Qt::AlignHCenter | Qt::AlignTop,
            QString::number(distance, 'g', 4));
        painter.setPen(QPen(QColor(53, 67, 84), 1.0));
    }
    painter.setPen(QPen(QColor(86, 103, 124), 1.2));
    painter.drawRect(plot);
    painter.setPen(QColor(192, 202, 216));
    painter.drawText(
        QRectF(plot.left(), height() - 28.0, plot.width(), 20.0),
        Qt::AlignHCenter | Qt::AlignVCenter,
        tr("距离 (%1)").arg(distance_unit_));
    painter.save();
    painter.translate(18.0, plot.center().y());
    painter.rotate(-90.0);
    painter.drawText(QRectF(-plot.height() / 2.0, -12.0, plot.height(), 20.0),
        Qt::AlignCenter, tr("%1强度").arg(channel_label_));
    painter.restore();

    if (!profile_.IsValid()) {
        painter.setPen(QColor(138, 151, 169));
        painter.drawText(plot, Qt::AlignCenter, tr("没有有效剖线数据"));
        return;
    }

    QPainterPath line;
    QPainterPath fill;
    for (std::size_t index = 0; index < profile_.samples.size(); ++index) {
        const ImageProfileSample& sample = profile_.samples[index];
        const double x = plot.left() + plot.width() * sample.distance_pixels / profile_.pixel_length;
        const double y = plot.bottom() - plot.height() * std::clamp(sample.intensity / 255.0, 0.0, 1.0);
        if (index == 0) {
            line.moveTo(x, y);
            fill.moveTo(x, plot.bottom());
            fill.lineTo(x, y);
        } else {
            line.lineTo(x, y);
            fill.lineTo(x, y);
        }
    }
    fill.lineTo(plot.right(), plot.bottom());
    fill.closeSubpath();
    QLinearGradient area_gradient(plot.topLeft(), plot.bottomLeft());
    area_gradient.setColorAt(0.0, QColor(65, 145, 255, 110));
    area_gradient.setColorAt(1.0, QColor(65, 145, 255, 8));
    painter.fillPath(fill, area_gradient);
    painter.setPen(QPen(QColor(91, 164, 255), 2.0));
    painter.drawPath(line);

    if (hover_index_ >= 0 && hover_index_ < static_cast<int>(profile_.samples.size())) {
        const ImageProfileSample& sample = profile_.samples[static_cast<std::size_t>(hover_index_)];
        const double x = plot.left() + plot.width() * sample.distance_pixels / profile_.pixel_length;
        const double y = plot.bottom() - plot.height() * sample.intensity / 255.0;
        painter.setPen(QPen(QColor(255, 202, 58, 180), 1.0, Qt::DashLine));
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        painter.setBrush(QColor(255, 202, 58));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(x, y), 4.5, 4.5);
        const QString text = tr("%1 %2 · %3")
            .arg(sample.distance_pixels * distance_scale_, 0, 'g', 5)
            .arg(distance_unit_)
            .arg(sample.intensity, 0, 'f', 1);
        const QRect text_bounds = painter.fontMetrics().boundingRect(text).adjusted(-7, -5, 7, 5);
        QRectF bubble(QPointF(x + 10.0, y - text_bounds.height() - 8.0), text_bounds.size());
        if (bubble.right() > width() - 6.0) bubble.moveRight(x - 10.0);
        painter.setPen(QColor(82, 98, 119));
        painter.setBrush(QColor(29, 39, 52, 240));
        painter.drawRoundedRect(bubble, 5.0, 5.0);
        painter.setPen(QColor(238, 243, 249));
        painter.drawText(bubble, Qt::AlignCenter, text);
    }
}

void ProfilePlotWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QRectF plot = plotRect();
    if (!profile_.IsValid() || !plot.contains(event->position())) {
        if (hover_index_ != -1) {
            hover_index_ = -1;
            update();
        }
        return;
    }
    const double ratio = std::clamp((event->position().x() - plot.left()) / plot.width(), 0.0, 1.0);
    hover_index_ = std::clamp(
        qRound(ratio * (profile_.samples.size() - 1)),
        0,
        static_cast<int>(profile_.samples.size()) - 1);
    update();
}

void ProfilePlotWidget::leaveEvent(QEvent*)
{
    hover_index_ = -1;
    update();
}

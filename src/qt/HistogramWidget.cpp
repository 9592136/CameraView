#include "HistogramWidget.h"

#include <QPainter>
#include <QPainterPath>

HistogramWidget::HistogramWidget(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(130);
}

void HistogramWidget::setHistogram(const HistogramData& data, QColor color)
{
    data_ = data;
    color_ = color;
    update();
}

void HistogramWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(28, 33, 40));
    const QRectF plot = QRectF(rect()).adjusted(8, 8, -8, -28);
    painter.setPen(QColor(73, 81, 91));
    painter.drawRect(plot);
    if (data_.max_count == 0) {
        painter.setPen(QColor(145, 154, 166));
        painter.drawText(plot, Qt::AlignCenter, tr("无直方图数据"));
        return;
    }

    QPainterPath path;
    path.moveTo(plot.left(), plot.bottom());
    for (int index = 0; index < HistogramData::kBinCount; ++index) {
        const double x = plot.left() + plot.width() * index / 255.0;
        const double ratio = data_.bins[static_cast<std::size_t>(index)] /
            static_cast<double>(data_.max_count);
        path.lineTo(x, plot.bottom() - plot.height() * ratio);
    }
    path.lineTo(plot.right(), plot.bottom());
    path.closeSubpath();
    QColor fill = color_;
    fill.setAlpha(90);
    painter.fillPath(path, fill);
    painter.setPen(QPen(color_, 1.4));
    painter.drawPath(path);

    painter.setPen(QColor(190, 197, 207));
    painter.drawText(
        QRectF(8, height() - 24, width() - 16, 20),
        Qt::AlignLeft | Qt::AlignVCenter,
        tr("最小 %1   最大 %2   均值 %3   中位数 %4")
            .arg(data_.stats.min_value)
            .arg(data_.stats.max_value)
            .arg(data_.stats.mean, 0, 'f', 1)
            .arg(data_.stats.median));
}

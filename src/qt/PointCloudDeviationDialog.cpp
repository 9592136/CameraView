#include "PointCloudDeviationDialog.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStringConverter>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

double GaussianExpectedCount(
    const PointCloudDeviationDistribution& distribution,
    double value)
{
    if (distribution.standard_deviation <= 1e-15 || distribution.bins.empty()) return 0.0;
    const double bin_width = distribution.bins.front().maximum -
        distribution.bins.front().minimum;
    const double normalized = (value - distribution.mean) /
        distribution.standard_deviation;
    return distribution.deviations.size() * bin_width /
        (distribution.standard_deviation * std::sqrt(2.0 * kPi)) *
        std::exp(-0.5 * normalized * normalized);
}

} // namespace

PointCloudDeviationPlotWidget::PointCloudDeviationPlotWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("PointCloudDeviationPlot"));
    setMinimumSize(760, 420);
}

void PointCloudDeviationPlotWidget::setDistribution(
    const PointCloudDeviationDistribution& distribution,
    const QString& unit)
{
    distribution_ = distribution;
    unit_ = unit;
    update();
}

void PointCloudDeviationPlotWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(10, 17, 25));
    const QRectF plot = QRectF(rect()).adjusted(74, 30, -30, -62);
    painter.setPen(QColor(52, 70, 90));
    painter.drawRect(plot);
    if (!distribution_.valid || distribution_.bins.empty()) {
        painter.setPen(QColor(150, 165, 185));
        painter.drawText(rect(), Qt::AlignCenter, tr("没有可显示的偏差数据"));
        return;
    }
    const double display_minimum = distribution_.standard_deviation > 1e-15
        ? std::min(distribution_.minimum,
            distribution_.mean - 3.5 * distribution_.standard_deviation)
        : distribution_.minimum;
    const double display_maximum = distribution_.standard_deviation > 1e-15
        ? std::max(distribution_.maximum,
            distribution_.mean + 3.5 * distribution_.standard_deviation)
        : distribution_.maximum;
    const double range = std::max(display_maximum - display_minimum, 1e-15);
    double maximum_count = std::max(1.0,
        GaussianExpectedCount(distribution_, distribution_.mean));
    for (const auto& bin : distribution_.bins) {
        maximum_count = std::max({maximum_count, static_cast<double>(bin.count),
            bin.gaussian_expected_count});
    }
    painter.setPen(QColor(75, 94, 116));
    for (int division = 0; division <= 5; ++division) {
        const double ratio = division / 5.0;
        const double x = plot.left() + ratio * plot.width();
        const double y = plot.bottom() - ratio * plot.height();
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.setPen(QColor(177, 190, 207));
        painter.drawText(QRectF(x - 48, plot.bottom() + 8, 96, 22), Qt::AlignCenter,
            QStringLiteral("%1").arg(display_minimum + range * ratio, 0, 'g', 5));
        painter.drawText(QRectF(4, y - 11, 62, 22), Qt::AlignRight | Qt::AlignVCenter,
            QStringLiteral("%1").arg(maximum_count * ratio, 0, 'g', 4));
        painter.setPen(QColor(75, 94, 116));
    }
    painter.setPen(QPen(QColor(77, 166, 255, 220), 1.0));
    painter.setBrush(QColor(77, 166, 255, 110));
    for (const auto& bin : distribution_.bins) {
        const double height = bin.count / maximum_count * plot.height();
        const double left = plot.left() +
            (bin.minimum - display_minimum) / range * plot.width();
        const double right = plot.left() +
            (bin.maximum - display_minimum) / range * plot.width();
        painter.drawRect(QRectF(left + 0.5, plot.bottom() - height,
            std::max(1.0, right - left - 1.0), height));
    }
    QPainterPath gaussian;
    constexpr int curve_samples = 240;
    for (int index = 0; index <= curve_samples; ++index) {
        const double ratio = index / static_cast<double>(curve_samples);
        const double value = display_minimum + ratio * range;
        const QPointF point(
            plot.left() + ratio * plot.width(),
            plot.bottom() - GaussianExpectedCount(distribution_, value) /
                maximum_count * plot.height());
        if (index == 0) gaussian.moveTo(point); else gaussian.lineTo(point);
    }
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 185, 75), 2.6));
    painter.drawPath(gaussian);
    if (distribution_.standard_deviation > 1e-15) {
        for (int sigma = -3; sigma <= 3; ++sigma) {
            const double value = distribution_.mean + sigma * distribution_.standard_deviation;
            const double x = plot.left() + (value - display_minimum) / range * plot.width();
            painter.setPen(QPen(sigma == 0 ? QColor(90, 225, 145) : QColor(185, 198, 215, 120),
                sigma == 0 ? 2.0 : 1.0, Qt::DashLine));
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
            painter.drawText(QPointF(x + 3.0, plot.top() + 15.0),
                sigma == 0 ? tr("均值") : tr("%1σ").arg(sigma));
        }
    }
    painter.setPen(QColor(190, 202, 218));
    painter.drawText(QRectF(plot.left(), height() - 30, plot.width(), 22),
        Qt::AlignCenter, tr("到拟合平面的有符号偏差 (%1)").arg(unit_));
    painter.save();
    painter.translate(18, plot.center().y());
    painter.rotate(-90);
    painter.drawText(QRectF(-plot.height() / 2, -12, plot.height(), 22),
        Qt::AlignCenter, tr("点数 / 理论频数"));
    painter.restore();
    painter.setPen(QPen(QColor(77, 166, 255), 5.0));
    painter.drawLine(plot.right() - 210, plot.top() + 16, plot.right() - 184, plot.top() + 16);
    painter.setPen(QColor(205, 215, 228));
    painter.drawText(QPointF(plot.right() - 178, plot.top() + 21), tr("实际直方图"));
    painter.setPen(QPen(QColor(255, 185, 75), 2.6));
    painter.drawLine(plot.right() - 100, plot.top() + 16, plot.right() - 74, plot.top() + 16);
    painter.setPen(QColor(205, 215, 228));
    painter.drawText(QPointF(plot.right() - 68, plot.top() + 21), tr("高斯拟合"));
}

PointCloudDeviationDialog::PointCloudDeviationDialog(
    PointCloudDeviationDistribution distribution,
    const QString& unit,
    QWidget* parent)
    : QDialog(parent), distribution_(std::move(distribution)), unit_(unit)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QStringLiteral("PointCloudDeviationDialog"));
    setWindowTitle(tr("点云偏差高斯分布"));
    resize(980, 680);
    auto* layout = new QVBoxLayout(this);
    summary_ = new QLabel(tr(
        "样本 %1 · 均值 %2 %3 · 标准差 σ %4 %3 · RMS %5 %3 · 峰谷值 %6 %3\n"
        "±1σ %7% · ±2σ %8% · ±3σ %9% · 偏度 %10 · 超额峰度 %11")
        .arg(distribution_.deviations.size())
        .arg(distribution_.mean, 0, 'g', 8).arg(unit_)
        .arg(distribution_.standard_deviation, 0, 'g', 8)
        .arg(distribution_.rms, 0, 'g', 8)
        .arg(distribution_.maximum - distribution_.minimum, 0, 'g', 8)
        .arg(distribution_.within_one_sigma_percent, 0, 'f', 2)
        .arg(distribution_.within_two_sigma_percent, 0, 'f', 2)
        .arg(distribution_.within_three_sigma_percent, 0, 'f', 2)
        .arg(distribution_.skewness, 0, 'g', 6)
        .arg(distribution_.excess_kurtosis, 0, 'g', 6));
    summary_->setObjectName(QStringLiteral("PointCloudDeviationSummary"));
    summary_->setWordWrap(true);
    layout->addWidget(summary_);
    plot_ = new PointCloudDeviationPlotWidget;
    plot_->setDistribution(distribution_, unit_);
    layout->addWidget(plot_, 1);
    auto* buttons = new QHBoxLayout;
    auto* export_button = new QPushButton(tr("导出 CSV…"));
    export_button->setObjectName(QStringLiteral("PointCloudDeviationExportButton"));
    auto* close_button = new QPushButton(tr("关闭"));
    buttons->addStretch();
    buttons->addWidget(export_button);
    buttons->addWidget(close_button);
    layout->addLayout(buttons);
    connect(export_button, &QPushButton::clicked, this, &PointCloudDeviationDialog::exportCsv);
    connect(close_button, &QPushButton::clicked, this, &QDialog::close);
}

void PointCloudDeviationDialog::exportCsv()
{
    QString path = QFileDialog::getSaveFileName(this, tr("导出点云偏差分布"),
        QStringLiteral("point-cloud-deviation-distribution.csv"), tr("CSV 文件 (*.csv)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".csv");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("导出失败"), file.errorString());
        return;
    }
    QTextStream output(&file);
    output.setEncoding(QStringConverter::Utf8);
    output << "metric,value,unit\n"
           << "sample_count," << distribution_.deviations.size() << ",count\n"
           << "mean," << distribution_.mean << ',' << unit_ << '\n'
           << "standard_deviation," << distribution_.standard_deviation << ',' << unit_ << '\n'
           << "rms," << distribution_.rms << ',' << unit_ << '\n'
           << "minimum," << distribution_.minimum << ',' << unit_ << '\n'
           << "maximum," << distribution_.maximum << ',' << unit_ << '\n'
           << "within_1_sigma_percent," << distribution_.within_one_sigma_percent << ",percent\n"
           << "within_2_sigma_percent," << distribution_.within_two_sigma_percent << ",percent\n"
           << "within_3_sigma_percent," << distribution_.within_three_sigma_percent << ",percent\n"
           << "skewness," << distribution_.skewness << ",dimensionless\n"
           << "excess_kurtosis," << distribution_.excess_kurtosis << ",dimensionless\n\n"
           << "bin_min,bin_max,bin_center,actual_count,gaussian_expected_count,unit\n";
    for (const auto& bin : distribution_.bins) {
        output << bin.minimum << ',' << bin.maximum << ',' << bin.center << ','
               << bin.count << ',' << bin.gaussian_expected_count << ',' << unit_ << '\n';
    }
    output << "\npoint_index,signed_deviation,unit\n";
    for (std::size_t index = 0; index < distribution_.deviations.size(); ++index) {
        output << index << ',' << distribution_.deviations[index] << ',' << unit_ << '\n';
    }
}

#include "PointCloudSectionDialog.h"

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

PointCloudSectionPlotWidget::PointCloudSectionPlotWidget(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("PointCloudSectionPlot"));
    setMinimumSize(720, 380);
}

void PointCloudSectionPlotWidget::setProfile(
    const PointCloudSectionProfile& profile,
    const QString& unit)
{
    profile_ = profile;
    unit_ = unit;
    update();
}

void PointCloudSectionPlotWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(10, 17, 25));
    const QRectF plot = QRectF(rect()).adjusted(70, 28, -28, -56);
    painter.setPen(QColor(52, 70, 90));
    painter.drawRect(plot);
    if (!profile_.valid || profile_.samples.empty()) {
        painter.setPen(QColor(150, 165, 185));
        painter.drawText(rect(), Qt::AlignCenter, tr("没有可显示的截面数据"));
        return;
    }
    const double height_range = std::max(
        profile_.maximum_height - profile_.minimum_height, 1e-12);
    painter.setPen(QColor(92, 110, 132));
    for (int division = 0; division <= 5; ++division) {
        const double ratio = division / 5.0;
        const double x = plot.left() + ratio * plot.width();
        const double y = plot.bottom() - ratio * plot.height();
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.setPen(QColor(175, 188, 205));
        painter.drawText(QRectF(x - 45, plot.bottom() + 8, 90, 22), Qt::AlignCenter,
            QStringLiteral("%1").arg(profile_.width * ratio, 0, 'g', 5));
        painter.drawText(QRectF(2, y - 11, 62, 22), Qt::AlignRight | Qt::AlignVCenter,
            QStringLiteral("%1").arg(profile_.minimum_height + height_range * ratio, 0, 'g', 5));
        painter.setPen(QColor(92, 110, 132));
    }
    QPainterPath path;
    for (std::size_t index = 0; index < profile_.samples.size(); ++index) {
        const auto& sample = profile_.samples[index];
        const QPointF point(
            plot.left() + sample.distance / profile_.width * plot.width(),
            plot.bottom() - (sample.height - profile_.minimum_height) / height_range * plot.height());
        if (index == 0) path.moveTo(point); else path.lineTo(point);
    }
    painter.setPen(QPen(QColor(77, 166, 255), 2.2));
    painter.drawPath(path);
    painter.setPen(QColor(190, 202, 218));
    painter.drawText(QRectF(plot.left(), height() - 28, plot.width(), 22),
        Qt::AlignCenter, tr("截面距离 (%1)").arg(unit_));
    painter.save();
    painter.translate(18, plot.center().y());
    painter.rotate(-90);
    painter.drawText(QRectF(-plot.height() / 2, -12, plot.height(), 22),
        Qt::AlignCenter, tr("高度 (%1)").arg(unit_));
    painter.restore();
}

PointCloudSectionDialog::PointCloudSectionDialog(
    PointCloudSectionProfile profile,
    const QString& unit,
    QWidget* parent)
    : QDialog(parent), profile_(std::move(profile)), unit_(unit)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QStringLiteral("PointCloudSectionDialog"));
    setWindowTitle(tr("点云任意截面分析"));
    resize(920, 620);
    auto* layout = new QVBoxLayout(this);
    summary_ = new QLabel(tr("截面宽度：%1 %2 · 阶梯差：%3 %2 · 凹槽深度：%4 %2 · 高度极差：%5 %2")
        .arg(profile_.width, 0, 'g', 8).arg(unit_)
        .arg(profile_.signed_step_height, 0, 'g', 8)
        .arg(profile_.groove_depth, 0, 'g', 8)
        .arg(profile_.maximum_height - profile_.minimum_height, 0, 'g', 8));
    summary_->setObjectName(QStringLiteral("PointCloudSectionSummary"));
    summary_->setWordWrap(true);
    layout->addWidget(summary_);
    plot_ = new PointCloudSectionPlotWidget;
    plot_->setProfile(profile_, unit_);
    layout->addWidget(plot_, 1);
    auto* buttons = new QHBoxLayout;
    auto* export_button = new QPushButton(tr("导出 CSV…"));
    export_button->setObjectName(QStringLiteral("PointCloudSectionExportButton"));
    auto* close_button = new QPushButton(tr("关闭"));
    buttons->addStretch();
    buttons->addWidget(export_button);
    buttons->addWidget(close_button);
    layout->addLayout(buttons);
    connect(export_button, &QPushButton::clicked, this, &PointCloudSectionDialog::exportCsv);
    connect(close_button, &QPushButton::clicked, this, &QDialog::close);
}

void PointCloudSectionDialog::exportCsv()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("导出点云截面"), QStringLiteral("point-cloud-section.csv"),
        tr("CSV 文件 (*.csv)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".csv");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("导出失败"), file.errorString());
        return;
    }
    QTextStream output(&file);
    output.setEncoding(QStringConverter::Utf8);
    output << "distance,height,source_count,unit\n";
    for (const auto& sample : profile_.samples) {
        output << sample.distance << ',' << sample.height << ',' << sample.source_count
               << ',' << unit_ << '\n';
    }
}

#include "PointCloudSectionDialog.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStringConverter>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

QColor profileColor(const PointCloudSectionProfile& profile)
{
    const auto rgb = profile.definition.color_rgb;
    return QColor(static_cast<int>((rgb >> 16U) & 0xffU),
        static_cast<int>((rgb >> 8U) & 0xffU), static_cast<int>(rgb & 0xffU));
}

QString featureName(PointCloudSectionFeatureType type)
{
    switch (type) {
    case PointCloudSectionFeatureType::Step: return QObject::tr("台阶");
    case PointCloudSectionFeatureType::Groove: return QObject::tr("沟槽");
    case PointCloudSectionFeatureType::Peak: return QObject::tr("峰值");
    }
    return {};
}

} // namespace

PointCloudSectionPlotWidget::PointCloudSectionPlotWidget(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("PointCloudSectionPlot"));
    setMinimumSize(320, 180);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void PointCloudSectionPlotWidget::setProfile(
    const PointCloudSectionProfile& profile, const QString& unit)
{
    setProfiles({profile}, profile.definition.id, unit);
}

void PointCloudSectionPlotWidget::setProfiles(
    const std::vector<PointCloudSectionProfile>& profiles,
    std::uint64_t active_profile_id,
    const QString& unit)
{
    profiles_ = profiles;
    active_profile_id_ = active_profile_id;
    const auto active = std::find_if(profiles_.begin(), profiles_.end(),
        [active_profile_id](const auto& value) { return value.definition.id == active_profile_id; });
    if (active != profiles_.end()) profile_ = *active;
    else if (!profiles_.empty()) profile_ = profiles_.front();
    else profile_ = {};
    unit_ = unit;
    resetViewRange();
    update();
}

void PointCloudSectionPlotWidget::setCursors(double first_distance, double second_distance)
{
    cursor_first_ = first_distance;
    cursor_second_ = second_distance;
    update();
}

QRectF PointCloudSectionPlotWidget::plotRect() const
{
    const bool compact = width() < 620 || height() < 230;
    return compact
        ? QRectF(rect()).adjusted(54, 18, -12, -40)
        : QRectF(rect()).adjusted(66, 24, -24, -48);
}

void PointCloudSectionPlotWidget::resetViewRange()
{
    view_minimum_ = 0.0;
    view_maximum_ = std::max(profile_.width, 1e-12);
    cursor_first_ = profile_.width * 0.25;
    cursor_second_ = profile_.width * 0.75;
}

double PointCloudSectionPlotWidget::distanceFromX(double x) const
{
    const QRectF plot = plotRect();
    const double ratio = std::clamp((x - plot.left()) / std::max(plot.width(), 1.0), 0.0, 1.0);
    return view_minimum_ + ratio * (view_maximum_ - view_minimum_);
}

void PointCloudSectionPlotWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(10, 17, 25));
    const QRectF plot = plotRect();
    painter.setPen(QColor(52, 70, 90));
    painter.drawRect(plot);
    if (!profile_.valid || profile_.samples.empty()) {
        painter.setPen(QColor(150, 165, 185));
        painter.drawText(rect(), Qt::AlignCenter, tr("绘制剖线后将在这里显示稳健轮廓"));
        return;
    }

    double minimum_height = std::numeric_limits<double>::infinity();
    double maximum_height = -std::numeric_limits<double>::infinity();
    for (const auto& current : profiles_) {
        if (!current.valid || !current.definition.visible) continue;
        for (const auto& sample : current.samples) {
            if (!sample.valid || sample.distance < view_minimum_ || sample.distance > view_maximum_) continue;
            minimum_height = std::min(minimum_height, sample.height);
            maximum_height = std::max(maximum_height, sample.height);
        }
    }
    if (!std::isfinite(minimum_height)) {
        minimum_height = profile_.minimum_height;
        maximum_height = profile_.maximum_height;
    }
    const double margin = std::max((maximum_height - minimum_height) * 0.08, 1e-12);
    minimum_height -= margin;
    maximum_height += margin;
    const double height_range = std::max(maximum_height - minimum_height, 1e-12);
    const auto x_of = [&](double distance) {
        return plot.left() + (distance - view_minimum_) /
            std::max(view_maximum_ - view_minimum_, 1e-12) * plot.width();
    };
    const auto y_of = [&](double height) {
        return plot.bottom() - (height - minimum_height) / height_range * plot.height();
    };

    painter.setPen(QColor(42, 58, 76));
    const int divisions = plot.width() < 620.0 ? 3 : 5;
    const int precision = divisions == 3 ? 4 : 5;
    for (int division = 0; division <= divisions; ++division) {
        const double ratio = static_cast<double>(division) / divisions;
        const double x = plot.left() + ratio * plot.width();
        const double y = plot.bottom() - ratio * plot.height();
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.setPen(QColor(160, 176, 196));
        painter.drawText(QRectF(x - 48, plot.bottom() + 5, 96, 20), Qt::AlignCenter,
            QString::number(view_minimum_ + (view_maximum_ - view_minimum_) * ratio, 'g', precision));
        painter.drawText(QRectF(2, y - 10, 58, 20), Qt::AlignRight | Qt::AlignVCenter,
            QString::number(minimum_height + height_range * ratio, 'g', precision));
        painter.setPen(QColor(42, 58, 76));
    }

    // Active-profile features are shown behind the curves.
    for (const auto& feature : profile_.features) {
        const double left = x_of(feature.start_distance);
        const double right = x_of(feature.end_distance);
        QColor fill = feature.type == PointCloudSectionFeatureType::Groove
            ? QColor(80, 150, 255, 35) : feature.type == PointCloudSectionFeatureType::Peak
            ? QColor(255, 170, 70, 35) : QColor(118, 214, 145, 30);
        painter.fillRect(QRectF(QPointF(left, plot.top()), QPointF(right, plot.bottom())).normalized(), fill);
        painter.setPen(fill.darker(120));
        painter.drawText(QRectF(std::min(left, right) + 3, plot.top() + 3,
            std::abs(right - left) - 6, 18), Qt::AlignLeft, featureName(feature.type));
    }

    for (const auto& current : profiles_) {
        if (!current.valid || !current.definition.visible) continue;
        const bool active = current.definition.id == active_profile_id_;
        QColor color = profileColor(current);
        color.setAlpha(active ? 255 : 115);
        if (active) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(color.red(), color.green(), color.blue(), 65));
            const std::size_t stride = std::max<std::size_t>(1, current.raw_points.size() / 12000);
            for (std::size_t index = 0; index < current.raw_points.size(); index += stride) {
                const auto& point = current.raw_points[index];
                if (point.distance < view_minimum_ || point.distance > view_maximum_) continue;
                painter.drawEllipse(QPointF(x_of(point.distance), y_of(point.height)), 1.2, 1.2);
            }
        }
        QPainterPath path;
        bool path_open = false;
        for (const auto& sample : current.samples) {
            if (!sample.valid || sample.distance < view_minimum_ || sample.distance > view_maximum_) {
                path_open = false;
                continue;
            }
            const QPointF point(x_of(sample.distance), y_of(sample.height));
            if (!path_open) { path.moveTo(point); path_open = true; }
            else path.lineTo(point);
        }
        painter.setPen(QPen(color, active ? 2.4 : 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
    }

    const auto draw_cursor = [&](double distance, const QString& name, const QColor& color) {
        const double x = x_of(distance);
        painter.setPen(QPen(color, 1.5, Qt::DashLine));
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(QPolygonF{QPointF(x - 5, plot.top()), QPointF(x + 5, plot.top()),
            QPointF(x, plot.top() + 7)});
        painter.setPen(color);
        painter.drawText(QPointF(x + 5, plot.top() + 17), name);
    };
    draw_cursor(cursor_first_, QStringLiteral("A"), QColor(255, 211, 74));
    draw_cursor(cursor_second_, QStringLiteral("B"), QColor(255, 122, 102));

    if (plot.contains(hover_position_)) {
        const double distance = distanceFromX(hover_position_.x());
        const double height = [&] {
            const auto upper = std::lower_bound(profile_.samples.begin(), profile_.samples.end(), distance,
                [](const auto& sample, double value) { return sample.distance < value; });
            return upper == profile_.samples.end() ? profile_.samples.back().height : upper->height;
        }();
        painter.setPen(QColor(215, 228, 244));
        painter.setBrush(QColor(18, 30, 44, 225));
        const QString tip = tr("距离 %1 %3\n高度 %2 %3")
            .arg(distance, 0, 'g', 7).arg(height, 0, 'g', 7).arg(unit_);
        QRectF tip_rect(hover_position_ + QPointF(12, -48), QSizeF(150, 42));
        if (tip_rect.right() > width() - 4) tip_rect.moveRight(width() - 4);
        painter.drawRoundedRect(tip_rect, 5, 5);
        painter.drawText(tip_rect.adjusted(7, 3, -5, -3), Qt::AlignLeft | Qt::AlignVCenter, tip);
    }

    painter.setPen(QColor(190, 202, 218));
    painter.drawText(QRectF(plot.left(), height() - 24, plot.width(), 20), Qt::AlignCenter,
        tr("剖线距离 (%1) · 滚轮缩放 · 双击复位").arg(unit_));
    painter.save();
    painter.translate(16, plot.center().y()); painter.rotate(-90);
    painter.drawText(QRectF(-plot.height() / 2, -10, plot.height(), 20), Qt::AlignCenter,
        tr("高度 (%1)").arg(unit_));
    painter.restore();
}

void PointCloudSectionPlotWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !profile_.valid) return;
    const auto x_for = [&](double distance) {
        return plotRect().left() + (distance - view_minimum_) /
            std::max(view_maximum_ - view_minimum_, 1e-12) * plotRect().width();
    };
    const double first_delta = std::abs(event->position().x() - x_for(cursor_first_));
    const double second_delta = std::abs(event->position().x() - x_for(cursor_second_));
    dragged_cursor_ = first_delta <= second_delta ? 1 : 2;
    if (std::min(first_delta, second_delta) > 12.0) {
        dragged_cursor_ = event->position().x() < (x_for(cursor_first_) + x_for(cursor_second_)) * 0.5 ? 1 : 2;
    }
    mouseMoveEvent(event);
}

void PointCloudSectionPlotWidget::mouseMoveEvent(QMouseEvent* event)
{
    hover_position_ = event->position();
    if (dragged_cursor_ != 0) {
        const double value = distanceFromX(event->position().x());
        if (dragged_cursor_ == 1) cursor_first_ = value; else cursor_second_ = value;
        emit cursorPositionsChanged(cursor_first_, cursor_second_);
    }
    update();
}

void PointCloudSectionPlotWidget::mouseReleaseEvent(QMouseEvent*) { dragged_cursor_ = 0; }

void PointCloudSectionPlotWidget::wheelEvent(QWheelEvent* event)
{
    if (!profile_.valid) return;
    const double center = distanceFromX(event->position().x());
    const double factor = event->angleDelta().y() > 0 ? 0.8 : 1.25;
    const double old_span = view_maximum_ - view_minimum_;
    const double span = std::clamp(old_span * factor, profile_.width / 50.0, profile_.width);
    const double ratio = (center - view_minimum_) / std::max(old_span, 1e-12);
    view_minimum_ = std::clamp(center - span * ratio, 0.0, profile_.width - span);
    view_maximum_ = view_minimum_ + span;
    update(); event->accept();
}

void PointCloudSectionPlotWidget::mouseDoubleClickEvent(QMouseEvent*) { resetViewRange(); update(); }

PointCloudSectionDialog::PointCloudSectionDialog(
    PointCloudSectionProfile profile, const QString& unit, QWidget* parent)
    : QDialog(parent), profile_(std::move(profile)), unit_(unit)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QStringLiteral("PointCloudSectionDialog"));
    setWindowTitle(tr("点云任意截面分析"));
    resize(920, 620);
    auto* layout = new QVBoxLayout(this);
    summary_ = new QLabel(tr("截面长度：%1 %2 · 阶梯差：%3 %2 · 凹槽深度：%4 %2 · 峰谷值：%5 %2")
        .arg(profile_.width, 0, 'g', 8).arg(unit_)
        .arg(profile_.signed_step_height, 0, 'g', 8)
        .arg(profile_.groove_depth, 0, 'g', 8)
        .arg(profile_.total_peak_to_valley, 0, 'g', 8));
    summary_->setObjectName(QStringLiteral("PointCloudSectionSummary"));
    summary_->setWordWrap(true); layout->addWidget(summary_);
    plot_ = new PointCloudSectionPlotWidget;
    plot_->setProfile(profile_, unit_); layout->addWidget(plot_, 1);
    auto* buttons = new QHBoxLayout;
    auto* export_button = new QPushButton(tr("导出 CSV…"));
    export_button->setObjectName(QStringLiteral("PointCloudSectionExportButton"));
    auto* close_button = new QPushButton(tr("关闭"));
    buttons->addStretch(); buttons->addWidget(export_button); buttons->addWidget(close_button);
    layout->addLayout(buttons);
    connect(export_button, &QPushButton::clicked, this, &PointCloudSectionDialog::exportCsv);
    connect(close_button, &QPushButton::clicked, this, &QDialog::close);
}

void PointCloudSectionDialog::exportCsv()
{
    QString path = QFileDialog::getSaveFileName(this, tr("导出点云截面"),
        QStringLiteral("point-cloud-section.csv"), tr("CSV 文件 (*.csv)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".csv");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("导出失败"), file.errorString()); return;
    }
    QTextStream output(&file); output.setEncoding(QStringConverter::Utf8);
    output << "distance,raw_height,filtered_height,mad,source_count,confidence,interpolated,unit\n";
    for (const auto& sample : profile_.samples) {
        output << sample.distance << ',' << sample.raw_height << ',' << sample.filtered_height
               << ',' << sample.mad << ',' << sample.source_count << ',' << sample.confidence
               << ',' << (sample.interpolated ? 1 : 0) << ',' << unit_ << '\n';
    }
}

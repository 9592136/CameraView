#include "ProfileAnalysisDialog.h"

#include "ProfilePlotWidget.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QStringConverter>
#include <QTextStream>
#include <QVBoxLayout>

ProfileAnalysisDialog::ProfileAnalysisDialog(
    ImageFrame frame,
    ImagePoint first,
    ImagePoint second,
    CalibrationProfile calibration,
    MeasurementUnit displayUnit,
    const QString& sourceName,
    QWidget* parent)
    : QDialog(parent),
      frame_(std::move(frame)),
      first_(first),
      second_(second),
      calibration_(calibration),
      display_unit_(displayUnit)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("剖线测量 · %1").arg(sourceName.isEmpty() ? tr("当前图像") : sourceName));
    resize(980, 650);
    setMinimumSize(720, 500);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);
    auto* header = new QHBoxLayout;
    header->addWidget(new QLabel(tr("强度通道")));
    channel_combo_ = new QComboBox;
    channel_combo_->setObjectName(QStringLiteral("ProfileChannelCombo"));
    channel_combo_->addItem(tr("亮度"), static_cast<int>(ImageProfileChannel::Luminance));
    channel_combo_->addItem(tr("红色"), static_cast<int>(ImageProfileChannel::Red));
    channel_combo_->addItem(tr("绿色"), static_cast<int>(ImageProfileChannel::Green));
    channel_combo_->addItem(tr("蓝色"), static_cast<int>(ImageProfileChannel::Blue));
    header->addWidget(channel_combo_);
    header->addStretch();
    root->addLayout(header);

    summary_label_ = new QLabel;
    summary_label_->setObjectName(QStringLiteral("ProfileSummary"));
    summary_label_->setWordWrap(true);
    root->addWidget(summary_label_);
    plot_ = new ProfilePlotWidget;
    root->addWidget(plot_, 1);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    auto* export_csv = new QPushButton(tr("导出 CSV…"));
    auto* export_plot = new QPushButton(tr("导出曲线图…"));
    export_plot->setProperty("role", QStringLiteral("primary"));
    auto* close_button = new QPushButton(tr("关闭"));
    buttons->addWidget(export_csv);
    buttons->addWidget(export_plot);
    buttons->addWidget(close_button);
    root->addLayout(buttons);

    connect(channel_combo_, qOverload<int>(&QComboBox::currentIndexChanged),
        this, &ProfileAnalysisDialog::recomputeProfile);
    connect(export_csv, &QPushButton::clicked, this, &ProfileAnalysisDialog::exportCsv);
    connect(export_plot, &QPushButton::clicked, this, &ProfileAnalysisDialog::exportPlot);
    connect(close_button, &QPushButton::clicked, this, &QDialog::close);
    recomputeProfile();
}

double ProfileAnalysisDialog::distanceScale() const
{
    if (!calibration_.IsCalibrated() || display_unit_ == MeasurementUnit::Pixels) return 1.0;
    return calibration_.PixelsToUnit(1.0, display_unit_);
}

QString ProfileAnalysisDialog::distanceUnitLabel() const
{
    if (!calibration_.IsCalibrated() || display_unit_ == MeasurementUnit::Pixels) return QStringLiteral("px");
    return QString::fromStdWString(CalibrationProfile::UnitLabel(display_unit_));
}

void ProfileAnalysisDialog::recomputeProfile()
{
    const ImageProfileChannel channel = static_cast<ImageProfileChannel>(channel_combo_->currentData().toInt());
    profile_ = ImageProfileSampler::Sample(frame_, first_, second_, channel);
    const QString channel_label = channel_combo_->currentText();
    const double length = profile_.pixel_length * distanceScale();
    summary_label_->setText(tr(
        "长度 %1 %2  ·  样本 %3  ·  最小 %4  ·  最大 %5  ·  平均 %6  ·  标准差 %7")
        .arg(length, 0, 'g', 7)
        .arg(distanceUnitLabel())
        .arg(profile_.samples.size())
        .arg(profile_.min_intensity, 0, 'f', 1)
        .arg(profile_.max_intensity, 0, 'f', 1)
        .arg(profile_.mean_intensity, 0, 'f', 1)
        .arg(profile_.standard_deviation, 0, 'f', 1));
    plot_->setProfile(profile_, distanceScale(), distanceUnitLabel(), channel_label);
}

void ProfileAnalysisDialog::exportCsv()
{
    if (!profile_.IsValid()) return;
    QString path = QFileDialog::getSaveFileName(
        this, tr("导出剖线数据"), QStringLiteral("image-profile.csv"), tr("CSV 文件 (*.csv)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".csv");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("导出失败"), file.errorString());
        return;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << "distance_" << distanceUnitLabel() << ",intensity\n";
    for (const ImageProfileSample& sample : profile_.samples) {
        stream << QString::number(sample.distance_pixels * distanceScale(), 'g', 12)
               << ',' << QString::number(sample.intensity, 'f', 6) << '\n';
    }
    if (!file.commit()) QMessageBox::warning(this, tr("导出失败"), file.errorString());
}

void ProfileAnalysisDialog::exportPlot()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("导出剖线曲线图"), QStringLiteral("image-profile.png"), tr("PNG 图像 (*.png)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".png");
    if (!plot_->grab().save(path, "PNG")) {
        QMessageBox::warning(this, tr("导出失败"), tr("无法保存剖线曲线图。"));
    }
}

#include "CalibrationDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

CalibrationDialog::CalibrationDialog(
    double pixelDistance,
    double initialRealLength,
    MeasurementUnit initialUnit,
    QWidget* parent)
    : QDialog(parent), pixel_distance_(pixelDistance)
{
    setWindowTitle(tr("两点标定"));
    setModal(true);
    setMinimumWidth(360);

    auto* layout = new QVBoxLayout(this);
    auto* hint = new QLabel(tr("已选定标尺的两个端点。请输入这段标尺对应的真实长度。"));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* form = new QFormLayout;
    auto* pixel_distance_label = new QLabel(
        tr("%1 px").arg(pixel_distance_, 0, 'f', 2));
    pixel_distance_label->setObjectName(QStringLiteral("CalibrationPixelDistance"));
    form->addRow(tr("像素距离"), pixel_distance_label);

    length_spin_ = new QDoubleSpinBox;
    length_spin_->setObjectName(QStringLiteral("CalibrationDialogLength"));
    length_spin_->setDecimals(6);
    length_spin_->setRange(0.000001, 1000000000.0);
    length_spin_->setSingleStep(1.0);
    length_spin_->setValue(std::max(0.000001, initialRealLength));
    length_spin_->setKeyboardTracking(false);
    form->addRow(tr("真实长度"), length_spin_);

    unit_combo_ = new QComboBox;
    unit_combo_->setObjectName(QStringLiteral("CalibrationDialogUnit"));
    unit_combo_->addItem(tr("µm"), static_cast<int>(MeasurementUnit::Micrometers));
    unit_combo_->addItem(tr("mm"), static_cast<int>(MeasurementUnit::Millimeters));
    const int initial_index = unit_combo_->findData(static_cast<int>(initialUnit));
    unit_combo_->setCurrentIndex(initial_index >= 0 ? initial_index : 0);
    form->addRow(tr("单位"), unit_combo_);
    layout->addLayout(form);

    scale_preview_label_ = new QLabel;
    scale_preview_label_->setObjectName(QStringLiteral("CalibrationScalePreview"));
    scale_preview_label_->setProperty("role", QStringLiteral("summary"));
    layout->addWidget(scale_preview_label_);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("应用标定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(length_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
        this, [this] { updatePreview(); });
    connect(unit_combo_, qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this] { updatePreview(); });
    updatePreview();
    length_spin_->setFocus();
    length_spin_->selectAll();
}

double CalibrationDialog::realLength() const
{
    return length_spin_->value();
}

MeasurementUnit CalibrationDialog::unit() const
{
    const QVariant data = unit_combo_->currentData();
    return static_cast<MeasurementUnit>(
        data.isValid() ? data.toInt() : static_cast<int>(MeasurementUnit::Micrometers));
}

CalibrationProfile CalibrationDialog::profile() const
{
    if (!std::isfinite(pixel_distance_) || pixel_distance_ < 1.0) {
        return CalibrationProfile::Uncalibrated();
    }
    return CalibrationProfile::FromTwoPointCalibration(
        ImagePoint{0.0, 0.0},
        ImagePoint{pixel_distance_, 0.0},
        realLength(),
        unit());
}

void CalibrationDialog::updatePreview()
{
    const CalibrationProfile candidate = profile();
    scale_preview_label_->setText(candidate.IsCalibrated()
        ? tr("换算比例：%1 µm / px").arg(candidate.MicronsPerPixel(), 0, 'g', 10)
        : tr("无法根据当前输入生成有效标定。"));
}

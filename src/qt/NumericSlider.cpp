#include "NumericSlider.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>

#include <algorithm>
#include <cmath>

NumericSlider::NumericSlider(QWidget* parent)
    : QWidget(parent),
      slider_(new QSlider(Qt::Horizontal, this)),
      value_label_(new QLabel(this))
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(slider_, 1);
    layout->addWidget(value_label_);

    slider_->setTracking(true);
    value_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value_label_->setMinimumWidth(64);
    value_label_->setProperty("role", QStringLiteral("sliderValue"));

    connect(slider_, &QSlider::valueChanged, this, [this] {
        updateValueLabel();
        emit valueChanged(value());
    });
    rebuildSliderRange();
}

void NumericSlider::setRange(double minimum, double maximum)
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum)) return;
    if (maximum < minimum) std::swap(minimum, maximum);
    const double old_value = value();
    minimum_ = minimum;
    maximum_ = maximum;
    rebuildSliderRange();
    setValue(std::clamp(old_value, minimum_, maximum_));
}

void NumericSlider::setSingleStep(double step)
{
    if (!std::isfinite(step) || step <= 0.0) return;
    const double old_value = value();
    single_step_ = step;
    rebuildSliderRange();
    setValue(old_value);
}

void NumericSlider::setDecimals(int decimals)
{
    decimals_ = std::clamp(decimals, 0, 8);
    updateValueLabel();
}

void NumericSlider::setSuffix(const QString& suffix)
{
    suffix_ = suffix;
    updateValueLabel();
}

void NumericSlider::setValue(double value)
{
    if (!std::isfinite(value)) return;
    const int position = positionFromValue(std::clamp(value, minimum_, maximum_));
    if (slider_->value() == position) {
        updateValueLabel();
        return;
    }
    slider_->setValue(position);
}

double NumericSlider::minimum() const
{
    return minimum_;
}

double NumericSlider::maximum() const
{
    return maximum_;
}

double NumericSlider::singleStep() const
{
    return single_step_;
}

double NumericSlider::value() const
{
    return valueFromPosition(slider_->value());
}

int NumericSlider::integerValue() const
{
    return static_cast<int>(std::lround(value()));
}

QSlider* NumericSlider::slider() const
{
    return slider_;
}

QLabel* NumericSlider::valueLabel() const
{
    return value_label_;
}

void NumericSlider::rebuildSliderRange()
{
    const QSignalBlocker blocker(slider_);
    const double span = maximum_ - minimum_;
    const int steps = span <= 0.0
        ? 0
        : std::max(1, static_cast<int>(std::lround(span / single_step_)));
    slider_->setRange(0, steps);
    slider_->setSingleStep(1);
    slider_->setPageStep(std::max(1, steps / 10));
    updateValueLabel();
}

double NumericSlider::valueFromPosition(int position) const
{
    if (slider_->maximum() <= 0 || maximum_ <= minimum_) return minimum_;
    if (position >= slider_->maximum()) return maximum_;
    return minimum_ + static_cast<double>(position) * single_step_;
}

int NumericSlider::positionFromValue(double value) const
{
    if (slider_->maximum() <= 0 || maximum_ <= minimum_) return 0;
    if (value >= maximum_) return slider_->maximum();
    return std::clamp(
        static_cast<int>(std::lround((value - minimum_) / single_step_)),
        slider_->minimum(), slider_->maximum());
}

void NumericSlider::updateValueLabel()
{
    QString text = decimals_ == 0
        ? QString::number(integerValue())
        : QString::number(value(), 'f', decimals_);
    text += suffix_;
    value_label_->setText(text);
    slider_->setAccessibleDescription(text);
}

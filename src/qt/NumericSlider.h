#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QSlider;

class NumericSlider final : public QWidget {
    Q_OBJECT

public:
    explicit NumericSlider(QWidget* parent = nullptr);

    void setRange(double minimum, double maximum);
    void setSingleStep(double step);
    void setDecimals(int decimals);
    void setSuffix(const QString& suffix);
    void setValue(double value);

    double minimum() const;
    double maximum() const;
    double singleStep() const;
    double value() const;
    int integerValue() const;
    QSlider* slider() const;
    QLabel* valueLabel() const;

signals:
    void valueChanged(double value);

private:
    void rebuildSliderRange();
    double valueFromPosition(int position) const;
    int positionFromValue(double value) const;
    void updateValueLabel();

    QSlider* slider_ = nullptr;
    QLabel* value_label_ = nullptr;
    double minimum_ = 0.0;
    double maximum_ = 100.0;
    double single_step_ = 1.0;
    int decimals_ = 0;
    QString suffix_;
};

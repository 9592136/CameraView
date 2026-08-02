#pragma once

#include "imaging/HistogramCalculator.h"

#include <QWidget>

class HistogramWidget final : public QWidget {
public:
    explicit HistogramWidget(QWidget* parent = nullptr);
    void setHistogram(const HistogramData& data, QColor color);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    HistogramData data_;
    QColor color_ = QColor(79, 172, 254);
};

#include "MeasurementToolButton.h"

#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QSizePolicy>

#include <cmath>

namespace {

class MeasurementIconEngine final : public QIconEngine {
public:
    explicit MeasurementIconEngine(MeasurementToolGlyph glyph) : glyph_(glyph) {}

    QIconEngine* clone() const override
    {
        return new MeasurementIconEngine(glyph_);
    }

    QString key() const override
    {
        return QStringLiteral("CameraViewMeasurementIcon");
    }

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        const QRectF bounds = QRectF(rect).adjusted(
            rect.width() * 0.12, rect.height() * 0.12,
            -rect.width() * 0.12, -rect.height() * 0.12);
        painter->translate(bounds.topLeft());
        painter->scale(bounds.width() / 32.0, bounds.height() / 32.0);

        QColor color = mode == QIcon::Disabled
            ? QColor(QStringLiteral("#59687a"))
            : QColor(QStringLiteral("#8fc4ff"));
        if (state == QIcon::On || mode == QIcon::Active || mode == QIcon::Selected) {
            color = QColor(QStringLiteral("#ffffff"));
        }
        QPen pen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);

        const auto node = [painter, color](const QPointF& point, qreal radius = 2.2) {
            painter->setBrush(color);
            painter->drawEllipse(point, radius, radius);
            painter->setBrush(Qt::NoBrush);
        };
        const auto arrow = [painter](const QPointF& tip, qreal direction) {
            constexpr qreal size = 3.6;
            painter->drawLine(tip, tip + QPointF(std::cos(direction + 0.55) * size,
                std::sin(direction + 0.55) * size));
            painter->drawLine(tip, tip + QPointF(std::cos(direction - 0.55) * size,
                std::sin(direction - 0.55) * size));
        };

        switch (glyph_) {
        case MeasurementToolGlyph::Calibration:
            painter->drawLine(QPointF(5, 22), QPointF(27, 10));
            node({5, 22});
            node({27, 10});
            painter->drawLine(QPointF(8, 26), QPointF(27, 26));
            for (int x = 8; x <= 27; x += 4) {
                painter->drawLine(QPointF(x, 24), QPointF(x, x % 8 == 0 ? 28 : 27));
            }
            break;
        case MeasurementToolGlyph::Point:
            painter->drawEllipse(QPointF(16, 16), 4.2, 4.2);
            painter->drawLine(QPointF(16, 3), QPointF(16, 10));
            painter->drawLine(QPointF(16, 22), QPointF(16, 29));
            painter->drawLine(QPointF(3, 16), QPointF(10, 16));
            painter->drawLine(QPointF(22, 16), QPointF(29, 16));
            node({16, 16}, 1.7);
            break;
        case MeasurementToolGlyph::Length:
            painter->drawLine(QPointF(5, 23), QPointF(27, 9));
            arrow({5, 23}, -0.57);
            arrow({27, 9}, 2.57);
            node({5, 23}, 1.8);
            node({27, 9}, 1.8);
            break;
        case MeasurementToolGlyph::Profile:
            painter->drawLine(QPointF(4, 25), QPointF(28, 7));
            node({4, 25}, 1.8);
            node({28, 7}, 1.8);
            {
                QPainterPath wave;
                wave.moveTo(5, 15);
                wave.cubicTo(9, 6, 12, 25, 16, 16);
                wave.cubicTo(20, 7, 23, 24, 27, 15);
                painter->drawPath(wave);
            }
            break;
        case MeasurementToolGlyph::Angle:
            painter->drawLine(QPointF(5, 25), QPointF(16, 14));
            painter->drawLine(QPointF(16, 14), QPointF(28, 23));
            node({5, 25}, 1.8);
            node({16, 14}, 2.0);
            node({28, 23}, 1.8);
            painter->drawArc(QRectF(11, 15, 12, 11), 35 * 16, 92 * 16);
            break;
        case MeasurementToolGlyph::Rectangle:
            painter->drawRect(QRectF(5, 7, 22, 18));
            painter->drawLine(QPointF(5, 28), QPointF(27, 28));
            painter->drawLine(QPointF(5, 26), QPointF(5, 30));
            painter->drawLine(QPointF(27, 26), QPointF(27, 30));
            break;
        case MeasurementToolGlyph::Polygon:
            {
                const QPolygonF polygon{{16, 3}, {28, 12}, {24, 27}, {9, 28}, {3, 14}};
                painter->drawPolygon(polygon);
                for (const QPointF& point : polygon) node(point, 1.7);
            }
            break;
        case MeasurementToolGlyph::Polyline:
            {
                const QPolygonF polyline{{3, 24}, {10, 10}, {18, 20}, {28, 6}};
                painter->drawPolyline(polyline);
                for (const QPointF& point : polyline) node(point, 1.8);
            }
            break;
        case MeasurementToolGlyph::Circle:
            painter->drawEllipse(QPointF(16, 16), 11, 11);
            painter->drawLine(QPointF(16, 16), QPointF(25, 10));
            node({16, 16}, 1.8);
            node({25, 10}, 1.8);
            break;
        case MeasurementToolGlyph::Ellipse:
            painter->drawEllipse(QRectF(3, 8, 26, 16));
            painter->drawLine(QPointF(3, 16), QPointF(29, 16));
            painter->drawLine(QPointF(16, 8), QPointF(16, 24));
            node({16, 16}, 1.6);
            break;
        case MeasurementToolGlyph::SmartCount:
            painter->drawRect(QRectF(4, 5, 24, 22));
            painter->drawEllipse(QRectF(8, 9, 7, 7));
            painter->drawEllipse(QRectF(18, 9, 6, 6));
            painter->drawEllipse(QRectF(12, 18, 7, 6));
            painter->drawLine(QPointF(2, 9), QPointF(2, 3));
            painter->drawLine(QPointF(2, 3), QPointF(8, 3));
            painter->drawLine(QPointF(30, 23), QPointF(30, 29));
            painter->drawLine(QPointF(30, 29), QPointF(24, 29));
            break;
        }
        painter->restore();
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
        QPixmap result(size);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        paint(&painter, result.rect(), mode, state);
        return result;
    }

private:
    MeasurementToolGlyph glyph_;
};

} // namespace

QIcon measurementToolIcon(MeasurementToolGlyph glyph)
{
    return QIcon(new MeasurementIconEngine(glyph));
}

MeasurementToolButton::MeasurementToolButton(
    MeasurementToolGlyph glyph,
    const QString& text,
    const QString& toolTip,
    QWidget* parent)
    : QToolButton(parent), glyph_(glyph)
{
    setText(text);
    setIcon(measurementToolIcon(glyph));
    setIconSize(QSize(30, 30));
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setCheckable(true);
    setAutoExclusive(true);
    setToolTip(toolTip);
    setAccessibleName(text);
    setAccessibleDescription(toolTip);
    setProperty("role", QStringLiteral("measurementTool"));
    setProperty("measurementGlyph", static_cast<int>(glyph));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumSize(84, 68);
}

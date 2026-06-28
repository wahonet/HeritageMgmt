#include "PieChartWidget.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QtMath>

namespace heritage {

PieChartWidget::PieChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(320, 220);
}

void PieChartWidget::setSlices(const QVector<SliceItem>& slices) {
    slices_ = slices;
    update();
}

void PieChartWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    static const QColor palette[] = {
        QColor(QStringLiteral("#2980b9")), QColor(QStringLiteral("#27ae60")),
        QColor(QStringLiteral("#c0392b")), QColor(QStringLiteral("#8e44ad")),
        QColor(QStringLiteral("#d35400")), QColor(QStringLiteral("#16a085")),
        QColor(QStringLiteral("#2c3e50")), QColor(QStringLiteral("#f39c12")),
    };

    const int side = qMin(width() - 150, height() - 20);
    const QRect pieRect(10, 10, qMax(80, side), qMax(80, side));

    double total = 0;
    for (const SliceItem& s : slices_)
        total += s.value;

    if (slices_.isEmpty() || total <= 0) {
        p.setPen(QColor(QStringLiteral("#888")));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("（无数据）"));
        return;
    }

    double angle = 90.0; // 从 12 点方向开始
    for (int i = 0; i < slices_.size(); ++i) {
        const double span = slices_[i].value / total * 360.0;
        QPainterPath path;
        path.moveTo(pieRect.center());
        path.arcTo(pieRect, angle, -span);
        path.closeSubpath();
        p.fillPath(path, palette[i % 8]);
        angle -= span;
    }
    p.setPen(QColor(QStringLiteral("#fff")));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(pieRect);

    // 图例
    QFont small = font();
    small.setPointSize(qMax(7, small.pointSize() - 1));
    p.setFont(small);
    const int legX = pieRect.right() + 16;
    int y = pieRect.top() + 6;
    for (int i = 0; i < slices_.size(); ++i) {
        p.setPen(Qt::NoPen);
        p.setBrush(palette[i % 8]);
        p.drawRect(legX, y, 12, 12);
        p.setPen(QColor(QStringLiteral("#333")));
        const QString pct = QString::number(slices_[i].value / total * 100, 'f', 0) + QStringLiteral("%");
        p.drawText(legX + 18, y + 11,
                   QStringLiteral("%1  %2（%3）").arg(slices_[i].label, pct, QString::number(slices_[i].value, 'f', 0)));
        y += 20;
    }
}

} // namespace heritage

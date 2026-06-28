#include "BarChartWidget.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPaintEvent>
#include <QtMath>

namespace heritage {

BarChartWidget::BarChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(320, 200);
}

void BarChartWidget::setBars(const QVector<BarItem>& bars, const QString& valueSuffix) {
    bars_ = bars;
    suffix_ = valueSuffix;
    update();
}

void BarChartWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRect r = rect().adjusted(10, 10, -10, -28);

    if (bars_.isEmpty()) {
        p.setPen(QColor(QStringLiteral("#888")));
        p.drawText(r, Qt::AlignCenter, QStringLiteral("（无数据）"));
        return;
    }

    double maxv = 0;
    for (const BarItem& b : bars_)
        maxv = qMax(maxv, b.value);
    if (maxv <= 0)
        maxv = 1;

    const int n = bars_.size();
    const int gap = 12;
    const int bw = n > 0 ? qMax(8, (r.width() - gap * (n + 1)) / n) : 0;
    // 调色板
    static const QColor palette[] = {
        QColor(QStringLiteral("#2980b9")), QColor(QStringLiteral("#27ae60")),
        QColor(QStringLiteral("#c0392b")), QColor(QStringLiteral("#8e44ad")),
        QColor(QStringLiteral("#d35400")), QColor(QStringLiteral("#16a085")),
        QColor(QStringLiteral("#2c3e50")), QColor(QStringLiteral("#f39c12")),
    };

    QFont small = font();
    small.setPointSize(qMax(7, small.pointSize() - 1));
    p.setFont(small);
    QFontMetrics fm(small);

    for (int i = 0; i < n; ++i) {
        const int x = r.left() + gap + i * (bw + gap);
        const int h = static_cast<int>(bars_[i].value / maxv * r.height());
        const QRect bar(x, r.bottom() - h, bw, h);
        p.setBrush(palette[i % 8]);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(bar, 3, 3);
        // 数值
        p.setPen(QColor(QStringLiteral("#333")));
        p.drawText(QRect(x, bar.top() - 16, bw, 15), Qt::AlignCenter,
                   QString::number(bars_[i].value, 'f', bars_[i].value >= 100 ? 0 : 1) + suffix_);
        // 标签（截断/换行）
        p.setPen(QColor(QStringLiteral("#555")));
        QString label = fm.elidedText(bars_[i].label, Qt::ElideRight, bw + gap);
        p.drawText(QRect(x - gap / 2, r.bottom() + 4, bw + gap, 20), Qt::AlignHCenter, label);
    }
}

} // namespace heritage

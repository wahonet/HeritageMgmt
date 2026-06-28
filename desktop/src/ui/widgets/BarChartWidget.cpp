#include "BarChartWidget.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QtMath>

namespace heritage {

// 文博暖棕主题协调色（赭石 / 竹青 / 赭黄 / 黛青 / 绛 / 茶褐 / 苔绿 / 石青）
static const QColor kPalette[] = {
    QColor(QStringLiteral("#a8623a")), QColor(QStringLiteral("#6e8b6b")),
    QColor(QStringLiteral("#c9a25a")), QColor(QStringLiteral("#4f6d7a")),
    QColor(QStringLiteral("#9e5b4d")), QColor(QStringLiteral("#7a6a55")),
    QColor(QStringLiteral("#8a9a5b")), QColor(QStringLiteral("#5a7a8c")),
};

BarChartWidget::BarChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(320, 210);
}

void BarChartWidget::setBars(const QVector<BarItem>& bars, const QString& valueSuffix) {
    bars_ = bars;
    suffix_ = valueSuffix;
    update();
}

void BarChartWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // 绘图区：上留数值、下留标签
    const QRect r = rect().adjusted(12, 18, -12, -30);

    if (bars_.isEmpty()) {
        p.setPen(QColor(QStringLiteral("#9b8f7c")));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("（暂无数据）"));
        return;
    }

    double maxv = 0;
    for (const BarItem& b : bars_)
        maxv = qMax(maxv, b.value);
    if (maxv <= 0)
        maxv = 1;

    // 横向基准网格线（4 等分），淡色
    p.setPen(QPen(QColor(QStringLiteral("#ece5d8")), 1));
    for (int g = 0; g <= 4; ++g) {
        const int gy = r.bottom() - r.height() * g / 4;
        p.drawLine(r.left(), gy, r.right(), gy);
    }
    // 基线
    p.setPen(QPen(QColor(QStringLiteral("#d8cfbf")), 1.5));
    p.drawLine(r.left(), r.bottom(), r.right(), r.bottom());

    const int n = bars_.size();
    const int gap = qMax(8, r.width() / (n * 4 + 1));
    const int bw = qMax(10, (r.width() - gap * (n + 1)) / n);

    QFont small = font();
    small.setPointSize(qMax(7, small.pointSize() - 1));
    p.setFont(small);
    QFontMetrics fm(small);

    for (int i = 0; i < n; ++i) {
        const int x = r.left() + gap + i * (bw + gap);
        const int h = qMax(2, static_cast<int>(bars_[i].value / maxv * r.height()));
        const QRect bar(x, r.bottom() - h, bw, h);

        const QColor base = kPalette[i % 8];
        QLinearGradient grad(bar.topLeft(), bar.bottomLeft());
        grad.setColorAt(0.0, base.lighter(118));
        grad.setColorAt(1.0, base);
        p.setBrush(grad);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(bar, 4, 4);

        // 数值
        p.setPen(QColor(QStringLiteral("#4a3a28")));
        p.drawText(QRect(x - gap / 2, bar.top() - 17, bw + gap, 15), Qt::AlignCenter,
                   QString::number(bars_[i].value, 'f', bars_[i].value >= 100 ? 0 : 1) + suffix_);
        // 标签
        p.setPen(QColor(QStringLiteral("#6f6456")));
        const QString label = fm.elidedText(bars_[i].label, Qt::ElideRight, bw + gap);
        p.drawText(QRect(x - gap / 2, r.bottom() + 6, bw + gap, 20), Qt::AlignHCenter, label);
    }
}

} // namespace heritage

#include "PieChartWidget.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QtMath>

namespace heritage {

// 与柱状图一致的文博暖棕协调色
static const QColor kPalette[] = {
    QColor(QStringLiteral("#a8623a")), QColor(QStringLiteral("#6e8b6b")),
    QColor(QStringLiteral("#c9a25a")), QColor(QStringLiteral("#4f6d7a")),
    QColor(QStringLiteral("#9e5b4d")), QColor(QStringLiteral("#7a6a55")),
    QColor(QStringLiteral("#8a9a5b")), QColor(QStringLiteral("#5a7a8c")),
};

PieChartWidget::PieChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(330, 230);
}

void PieChartWidget::setSlices(const QVector<SliceItem>& slices) {
    slices_ = slices;
    update();
}

void PieChartWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    double total = 0;
    for (const SliceItem& s : slices_)
        total += s.value;

    if (slices_.isEmpty() || total <= 0) {
        p.setPen(QColor(QStringLiteral("#9b8f7c")));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("（暂无数据）"));
        return;
    }

    // 给右侧图例预留空间
    const int legendW = 150;
    const int side = qMax(90, qMin(width() - legendW - 24, height() - 24));
    const QRect pieRect(16, (height() - side) / 2, side, side);

    // 扇区（白色描边分隔）
    double angle = 90.0; // 12 点起
    for (int i = 0; i < slices_.size(); ++i) {
        const double span = slices_[i].value / total * 360.0;
        QPainterPath path;
        path.moveTo(pieRect.center());
        path.arcTo(pieRect, angle, -span);
        path.closeSubpath();
        p.setPen(QPen(QColor(QStringLiteral("#ffffff")), 2));
        p.setBrush(kPalette[i % 8]);
        p.drawPath(path);
        angle -= span;
    }

    // 环形：中心挖空 + 总数
    const int holeD = static_cast<int>(side * 0.52);
    const QRect hole(pieRect.center().x() - holeD / 2, pieRect.center().y() - holeD / 2, holeD, holeD);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(QStringLiteral("#ffffff")));
    p.drawEllipse(hole);

    QFont big = font();
    big.setPointSize(big.pointSize() + 5);
    big.setBold(true);
    p.setFont(big);
    p.setPen(QColor(QStringLiteral("#6b4423")));
    p.drawText(hole, Qt::AlignCenter, QString::number(static_cast<int>(total)));
    QFont tiny = font();
    tiny.setPointSize(qMax(7, tiny.pointSize() - 1));
    p.setFont(tiny);
    p.setPen(QColor(QStringLiteral("#9b8f7c")));
    p.drawText(hole.adjusted(0, holeD / 2 - 6, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
               QStringLiteral("总计"));

    // 图例（圆角色块）
    QFont small = font();
    small.setPointSize(qMax(7, small.pointSize() - 1));
    p.setFont(small);
    QFontMetrics fm(small);
    const int legX = pieRect.right() + 18;
    const int rowH = 22;
    const int legendH = rowH * static_cast<int>(slices_.size());
    int y = pieRect.top() + qMax(0, (side - legendH) / 2);
    for (int i = 0; i < slices_.size(); ++i) {
        p.setPen(Qt::NoPen);
        p.setBrush(kPalette[i % 8]);
        p.drawRoundedRect(legX, y + 2, 12, 12, 3, 3);
        const QString pct = QString::number(slices_[i].value / total * 100, 'f', 0) + QStringLiteral("%");
        p.setPen(QColor(QStringLiteral("#4a3a28")));
        const QString name = fm.elidedText(slices_[i].label, Qt::ElideRight, legendW - 60);
        p.drawText(legX + 18, y + 12, name);
        // 百分比右对齐
        p.setPen(QColor(QStringLiteral("#a8623a")));
        p.drawText(QRect(legX, y, legendW - 18, rowH), Qt::AlignRight | Qt::AlignVCenter, pct);
        y += rowH;
    }
}

} // namespace heritage

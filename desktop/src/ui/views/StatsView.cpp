#include "StatsView.h"

#include "ui/widgets/BarChartWidget.h"
#include "ui/widgets/PieChartWidget.h"

#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

namespace heritage {

StatsView::StatsView(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);

    lblTotal_ = new QLabel(this);
    QFont f = lblTotal_->font();
    f.setPointSize(f.pointSize() + 2);
    f.setBold(true);
    lblTotal_->setFont(f);
    outer->addWidget(lblTotal_);

    auto* grid = new QGridLayout();
    grid->setSpacing(8);
    auto mkBox = [&](const QString& title, QWidget* inner) {
        auto* b = new QGroupBox(title, this);
        auto* l = new QVBoxLayout(b);
        l->addWidget(inner);
        return b;
    };
    byUnit_ = new BarChartWidget(this);
    byType_ = new BarChartWidget(this);
    byYear_ = new BarChartWidget(this);
    byStatus_ = new PieChartWidget(this);
    grid->addWidget(mkBox(QStringLiteral("中央资金(万元) 按单位"), byUnit_), 0, 0);
    grid->addWidget(mkBox(QStringLiteral("中央资金(万元) 按工程类型"), byType_), 0, 1);
    grid->addWidget(mkBox(QStringLiteral("中央资金(万元) 按年度"), byYear_), 1, 0);
    grid->addWidget(mkBox(QStringLiteral("工程数量 按状态"), byStatus_), 1, 1);
    outer->addLayout(grid, 1);
}

void StatsView::showStats(const StatsResult& r) {
    lblTotal_->setText(QStringLiteral("共 %1 个工程 ｜ 中央指标 %2 万元 ｜ 已支付 %3 万元 ｜ 待支付 %4 万元")
                           .arg(r.total.n)
                           .arg(r.total.funding, 0, 'f', 2)
                           .arg(r.total.paid, 0, 'f', 2)
                           .arg(r.total.pending, 0, 'f', 2));

    auto toBars = [](const QVector<StatGroup>& g) {
        QVector<BarItem> bars;
        for (const StatGroup& s : g)
            bars.append({s.k, s.funding});
        return bars;
    };
    byUnit_->setBars(toBars(r.byUnit));
    byType_->setBars(toBars(r.byType));
    byYear_->setBars(toBars(r.byYear));

    QVector<SliceItem> slices;
    for (const StatGroup& s : r.byStatus)
        slices.append({s.k.isEmpty() ? QStringLiteral("未定") : s.k, static_cast<double>(s.n)});
    byStatus_->setSlices(slices);
}

} // namespace heritage

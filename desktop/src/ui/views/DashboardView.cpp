#include "DashboardView.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

namespace heritage {

DashboardView::DashboardView(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void DashboardView::buildUi() {
    auto* outer = new QVBoxLayout(this);

    // 卡片行
    auto* cardsHost = new QWidget(this);
    auto* cardsLay = new QHBoxLayout(cardsHost);
    // 用 GroupBox 包数值：makeCard 返回内部 label，这里另持有
    auto* bTotal = new QGroupBox(QStringLiteral("工程总数"), cardsHost);
    lblTotal_ = new QLabel(QStringLiteral("—"), bTotal);
    auto* bDone = new QGroupBox(QStringLiteral("档案齐全"), cardsHost);
    lblDone_ = new QLabel(QStringLiteral("—"), bDone);
    auto* bMiss = new QGroupBox(QStringLiteral("有缺项"), cardsHost);
    lblMiss_ = new QLabel(QStringLiteral("—"), bMiss);
    auto* bFund = new QGroupBox(QStringLiteral("中央资金(万元)"), cardsHost);
    lblFund_ = new QLabel(QStringLiteral("—"), bFund);
    auto* bPaid = new QGroupBox(QStringLiteral("已支付(万元)"), cardsHost);
    lblPaid_ = new QLabel(QStringLiteral("—"), bPaid);
    for (const auto& pair : {QPair<QGroupBox*, QLabel*>{bTotal, lblTotal_},
                             QPair<QGroupBox*, QLabel*>{bDone, lblDone_},
                             QPair<QGroupBox*, QLabel*>{bMiss, lblMiss_},
                             QPair<QGroupBox*, QLabel*>{bFund, lblFund_},
                             QPair<QGroupBox*, QLabel*>{bPaid, lblPaid_}}) {
        QLabel* l = pair.second;
        l->setAlignment(Qt::AlignCenter);
        QFont f = l->font();
        f.setPointSize(f.pointSize() + 6);
        f.setBold(true);
        l->setFont(f);
        auto* lay = new QVBoxLayout(pair.first);
        lay->addWidget(l);
        cardsLay->addWidget(pair.first);
    }
    lblDone_->setStyleSheet(QStringLiteral("color: #27ae60;"));
    lblMiss_->setStyleSheet(QStringLiteral("color: #c0392b;"));
    outer->addWidget(cardsHost);

    // 缺项表
    auto* box = new QGroupBox(QStringLiteral("工程缺项清单（必备档案）"), this);
    auto* boxLay = new QVBoxLayout(box);
    table_ = new QTableWidget(box);
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels({QStringLiteral("单位"), QStringLiteral("工程"),
                                       QStringLiteral("类型"), QStringLiteral("状态"),
                                       QStringLiteral("文档数"), QStringLiteral("缺必备项")});
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    boxLay->addWidget(table_);
    outer->addWidget(box, 1);
}

void DashboardView::showDashboard(const DashboardData& d) {
    lblTotal_->setText(QString::number(d.totalProjects));
    lblDone_->setText(QString::number(d.done));
    lblMiss_->setText(QString::number(d.withMissing));
    lblFund_->setText(QString::number(d.centralFunding, 'f', 2));
    lblPaid_->setText(QString::number(d.totalPaid, 'f', 2));

    table_->setRowCount(d.rows.size());
    for (int i = 0; i < d.rows.size(); ++i) {
        const DashboardProjectRow& r = d.rows[i];
        const QStringList cells = {r.unitName, r.project.name,
                                   r.project.ptype, r.project.status,
                                   QString::number(r.docCount),
                                   r.missing.isEmpty() ? QStringLiteral("✔ 齐全") : r.missing.join(QStringLiteral("、"))};
        for (int c = 0; c < cells.size(); ++c) {
            auto* item = new QTableWidgetItem(cells[c]);
            if (c == 5)
                item->setForeground(r.missing.isEmpty() ? QBrush(QColor(QStringLiteral("#27ae60")))
                                                        : QBrush(QColor(QStringLiteral("#c0392b"))));
            table_->setItem(i, c, item);
        }
    }
}

} // namespace heritage

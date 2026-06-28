#include "RecycleView.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace heritage {

RecycleView::RecycleView(QWidget* parent) : QWidget(parent) {
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(14, 14, 14, 14);
    auto* box = new QGroupBox(QStringLiteral("回收站（已删除工程）"), this);
    auto* boxLay = new QVBoxLayout(box);

    auto* btnRow = new QHBoxLayout();
    auto* btnRestore = new QPushButton(QStringLiteral("↩ 恢复选中"), box);
    auto* btnPurge = new QPushButton(QStringLiteral("✕ 彻底删除"), box);
    btnPurge->setObjectName(QStringLiteral("btnDanger"));
    btnRow->addWidget(btnRestore);
    btnRow->addWidget(btnPurge);
    btnRow->addStretch(1);
    boxLay->addLayout(btnRow);

    table_ = new QTableWidget(box);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({QStringLiteral("单位"), QStringLiteral("工程"),
                                       QStringLiteral("类型"), QStringLiteral("状态"),
                                       QStringLiteral("归档目录")});
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    boxLay->addWidget(table_);
    lay->addWidget(box);

    connect(btnRestore, &QPushButton::clicked, this, &RecycleView::onRestore);
    connect(btnPurge, &QPushButton::clicked, this, &RecycleView::onPurge);
}

void RecycleView::showRecycled(const QVector<RecycledProject>& items) {
    table_->setRowCount(items.size());
    for (int i = 0; i < items.size(); ++i) {
        const RecycledProject& r = items[i];
        const QStringList cells = {r.unitName, r.name, r.ptype, r.status, r.folder};
        for (int c = 0; c < cells.size(); ++c) {
            auto* item = new QTableWidgetItem(cells[c]);
            item->setData(Qt::UserRole, r.id);
            table_->setItem(i, c, item);
        }
    }
}

void RecycleView::onRestore() {
    if (auto* it = table_->currentItem())
        emit restoreRequested(it->data(Qt::UserRole).toLongLong());
}

void RecycleView::onPurge() {
    if (!table_->currentItem())
        return;
    if (QMessageBox::question(this, QStringLiteral("彻底删除"),
            QStringLiteral("彻底删除该工程？此操作不可撤销（DB记录与文件均删除）。")) == QMessageBox::Yes)
        emit purgeRequested(table_->currentItem()->data(Qt::UserRole).toLongLong());
}

} // namespace heritage

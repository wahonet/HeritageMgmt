#include "LogsView.h"

#include <QGroupBox>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace heritage {

LogsView::LogsView(QWidget* parent) : QWidget(parent) {
    auto* lay = new QVBoxLayout(this);
    auto* box = new QGroupBox(QStringLiteral("操作日志"), this);
    auto* boxLay = new QVBoxLayout(box);
    table_ = new QTableWidget(box);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({QStringLiteral("时间"), QStringLiteral("操作"),
                                       QStringLiteral("对象"), QStringLiteral("明细")});
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    boxLay->addWidget(table_);
    lay->addWidget(box);
}

void LogsView::showLogs(const QVector<LogEntry>& logs) {
    table_->setRowCount(logs.size());
    for (int i = 0; i < logs.size(); ++i) {
        const LogEntry& l = logs[i];
        const QStringList cells = {l.ts, l.action, l.target, l.detail};
        for (int c = 0; c < cells.size(); ++c)
            table_->setItem(i, c, new QTableWidgetItem(cells[c]));
    }
}

} // namespace heritage

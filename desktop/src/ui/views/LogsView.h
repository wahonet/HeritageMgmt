#ifndef HERITAGE_UI_LOGS_VIEW_H
#define HERITAGE_UI_LOGS_VIEW_H

// 操作日志视图：表格列出 LogRepo.list（时间/操作/对象/明细）。

#include "core/domain/DomainTypes.h"

#include <QWidget>

class QTableWidget;

namespace heritage {

class LogsView : public QWidget {
    Q_OBJECT
public:
    explicit LogsView(QWidget* parent = nullptr);

public slots:
    void showLogs(const QVector<LogEntry>& logs);

private:
    QTableWidget* table_ = nullptr;
};

} // namespace heritage

#endif // HERITAGE_UI_LOGS_VIEW_H

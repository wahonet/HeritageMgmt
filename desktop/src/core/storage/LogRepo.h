#ifndef HERITAGE_STORAGE_LOG_REPO_H
#define HERITAGE_STORAGE_LOG_REPO_H

// 操作日志(logs)：对应 Go internal/store/log_repo.go 的 InsertLog/ListLogs。

#include "core/domain/DomainTypes.h"

#include <QSqlDatabase>
#include <QVector>

namespace heritage {

class LogRepo {
public:
    explicit LogRepo(QSqlDatabase db);

    void insert(const QString& action, const QString& target, const QString& detail);
    QVector<LogEntry> list(int limit = 200); // ORDER BY id DESC LIMIT ?

private:
    QSqlDatabase db_;
};

} // namespace heritage

#endif // HERITAGE_STORAGE_LOG_REPO_H

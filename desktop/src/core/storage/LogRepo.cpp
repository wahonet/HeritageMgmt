#include "LogRepo.h"

#include <QDateTime>
#include <QSqlQuery>

namespace heritage {

LogRepo::LogRepo(QSqlDatabase db) : db_(db) {}

void LogRepo::insert(const QString& action, const QString& target, const QString& detail) {
    QSqlQuery q(db_);
    if (!q.prepare(QStringLiteral("INSERT INTO logs(ts,action,target,detail) VALUES(?,?,?,?)")))
        return;
    q.addBindValue(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    q.addBindValue(action);
    q.addBindValue(target);
    q.addBindValue(detail);
    q.exec();
}

QVector<LogEntry> LogRepo::list(int limit) {
    QVector<LogEntry> out;
    QSqlQuery q(db_);
    if (!q.prepare(QStringLiteral("SELECT id,ts,action,target,detail FROM logs ORDER BY id DESC LIMIT ?")))
        return out;
    q.addBindValue(limit);
    if (!q.exec())
        return out;
    while (q.next()) {
        LogEntry l;
        l.id = q.value(0).toLongLong();
        l.ts = q.value(1).toString();
        l.action = q.value(2).toString();
        l.target = q.value(3).toString();
        l.detail = q.value(4).toString();
        out.append(l);
    }
    return out;
}

} // namespace heritage

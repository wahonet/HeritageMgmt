#include "ProjectRepo.h"

#include <QSqlError>
#include <QSqlQuery>

namespace heritage {

ProjectRepo::ProjectRepo(QSqlDatabase db) : db_(db) {}

QVector<Project> ProjectRepo::list(qint64 unitId, const QString& status, const QString& q) {
    QVector<Project> out;
    QString sql = QStringLiteral("SELECT %1 FROM projects WHERE COALESCE(deleted,0)=0")
                      .arg(projectColumns());
    if (unitId > 0)
        sql += QStringLiteral(" AND unit_id=?");
    if (!status.isEmpty())
        sql += QStringLiteral(" AND status=?");
    if (!q.isEmpty())
        sql += QStringLiteral(" AND (name LIKE ? OR approval_no LIKE ?)");
    sql += QStringLiteral(" ORDER BY unit_id,seq,id");

    QSqlQuery query(db_);
    if (!query.prepare(sql))
        return out;
    if (unitId > 0)
        query.addBindValue(unitId);
    if (!status.isEmpty())
        query.addBindValue(status);
    if (!q.isEmpty()) {
        query.addBindValue(QStringLiteral("%%1%").arg(q));
        query.addBindValue(QStringLiteral("%%1%").arg(q));
    }
    if (!query.exec())
        return out;
    while (query.next())
        out.append(scanProject(query));
    return out;
}

std::optional<Project> ProjectRepo::get(qint64 id) {
    QSqlQuery q(db_);
    if (!q.prepare(QStringLiteral("SELECT %1 FROM projects WHERE id=?").arg(projectColumns())))
        return std::nullopt;
    q.addBindValue(id);
    if (!q.exec() || !q.next())
        return std::nullopt;
    return scanProject(q);
}

int ProjectRepo::count() {
    QSqlQuery q(db_);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM projects")) || !q.next())
        return 0;
    return q.value(0).toInt();
}

QString ProjectRepo::name(qint64 id) {
    QSqlQuery q(db_);
    if (!q.prepare(QStringLiteral("SELECT name FROM projects WHERE id=?")))
        return {};
    q.addBindValue(id);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return {};
}

QSet<QString> ProjectRepo::docTypes(qint64 projectId) {
    QSet<QString> out;
    QSqlQuery q(db_);
    if (!q.prepare(QStringLiteral("SELECT DISTINCT doc_type FROM documents WHERE project_id=?")))
        return out;
    q.addBindValue(projectId);
    if (q.exec())
        while (q.next())
            out.insert(q.value(0).toString());
    return out;
}

} // namespace heritage

#include "ProjectRepo.h"

#include <QDateTime>
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

bool ProjectRepo::updateFields(qint64 id, const QString& sets, const QVariantList& vals) {
    QSqlQuery q(db_);
    if (!q.prepare(QStringLiteral("UPDATE projects SET ") + sets + QStringLiteral(" WHERE id=?")))
        return false;
    for (const QVariant& v : vals)
        q.addBindValue(v);
    q.addBindValue(id);
    return q.exec();
}

qint64 ProjectRepo::create(qint64 unitId, const QString& name, const QString& ptype, const QString& status) {
    QSqlQuery q(db_);
    if (!q.prepare(QStringLiteral(
            "INSERT INTO projects(unit_id,name,ptype,status,created) VALUES(?,?,?,?,?)")))
        return 0;
    q.addBindValue(unitId);
    q.addBindValue(name);
    q.addBindValue(ptype);
    q.addBindValue(status);
    q.addBindValue(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (!q.exec())
        return 0;
    return q.lastInsertId().toLongLong();
}

bool ProjectRepo::setFolder(qint64 id, const QString& folder) {
    QSqlQuery q(db_);
    if (!q.prepare(QStringLiteral("UPDATE projects SET folder=? WHERE id=?")))
        return false;
    q.addBindValue(folder);
    q.addBindValue(id);
    return q.exec();
}

bool ProjectRepo::softDelete(qint64 id) {
    QSqlQuery q(db_);
    if (!q.prepare(QStringLiteral("UPDATE projects SET deleted=1 WHERE id=?")))
        return false;
    q.addBindValue(id);
    return q.exec();
}

bool ProjectRepo::restore(qint64 id) {
    QSqlQuery q(db_);
    if (!q.prepare(QStringLiteral("UPDATE projects SET deleted=0 WHERE id=?")))
        return false;
    q.addBindValue(id);
    return q.exec();
}

bool ProjectRepo::purge(qint64 id) {
    QSqlQuery q(db_);
    if (!q.prepare(QStringLiteral("DELETE FROM documents WHERE project_id=?")))
        return false;
    q.addBindValue(id);
    if (!q.exec())
        return false;
    q.prepare(QStringLiteral("DELETE FROM projects WHERE id=?"));
    q.addBindValue(id);
    return q.exec();
}

QVector<RecycledProject> ProjectRepo::listRecycled() {
    QVector<RecycledProject> out;
    QSqlQuery q(db_);
    if (!q.exec(QStringLiteral(
            "SELECT p.id,p.name,p.folder,p.ptype,p.status,u.name FROM projects p "
            "LEFT JOIN units u ON u.id=p.unit_id WHERE COALESCE(p.deleted,0)=1 ORDER BY p.id DESC")))
        return out;
    while (q.next()) {
        RecycledProject r;
        r.id = q.value(0).toLongLong();
        r.name = q.value(1).toString();
        r.folder = q.value(2).toString();
        r.ptype = q.value(3).toString();
        r.status = q.value(4).toString();
        r.unitName = q.value(5).toString();
        if (r.unitName.isEmpty())
            r.unitName = QStringLiteral("(单位已删除)");
        out.append(r);
    }
    return out;
}

QVector<qint64> ProjectRepo::idsByUnit(qint64 unitId) {
    QVector<qint64> out;
    QSqlQuery q(db_);
    if (!q.prepare(QStringLiteral("SELECT id FROM projects WHERE unit_id=? AND COALESCE(deleted,0)=0")))
        return out;
    q.addBindValue(unitId);
    if (q.exec())
        while (q.next())
            out.append(q.value(0).toLongLong());
    return out;
}

} // namespace heritage

#include "UnitRepo.h"

#include <QSqlError>
#include <QSqlQuery>

namespace heritage {

UnitRepo::UnitRepo(QSqlDatabase db) : db_(db) {}

QVector<Unit> UnitRepo::list() {
    QVector<Unit> out;
    QSqlQuery q(db_);
    if (!q.exec(QStringLiteral("SELECT id,name,level,category,sort FROM units ORDER BY sort,id")))
        return out;
    while (q.next()) {
        Unit u;
        u.id = q.value(0).toLongLong();
        u.name = q.value(1).toString();
        u.level = q.value(2).toString();
        u.category = q.value(3).toString();
        u.sort = q.value(4).toInt();
        out.append(u);
    }
    return out;
}

QString UnitRepo::level(qint64 id) {
    QSqlQuery q(db_);
    if (!q.prepare(QStringLiteral("SELECT level FROM units WHERE id=?")))
        return {};
    q.addBindValue(id);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return {};
}

qint64 UnitRepo::createUnit(const QString& name, const QString& level, int sort) {
    QSqlQuery q(db_);
    if (!q.prepare(QStringLiteral("INSERT INTO units(name,level,sort) VALUES(?,?,?)")))
        return 0;
    q.addBindValue(name);
    q.addBindValue(level);
    q.addBindValue(sort);
    if (!q.exec())
        return 0;
    return q.lastInsertId().toLongLong();
}

void UnitRepo::deleteRecords(qint64 unitId) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("DELETE FROM documents WHERE project_id IN "
                             "(SELECT id FROM projects WHERE unit_id=?)"));
    q.addBindValue(unitId);
    q.exec();
    q.prepare(QStringLiteral("DELETE FROM projects WHERE unit_id=?"));
    q.addBindValue(unitId);
    q.exec();
    q.prepare(QStringLiteral("DELETE FROM units WHERE id=?"));
    q.addBindValue(unitId);
    q.exec();
}

} // namespace heritage

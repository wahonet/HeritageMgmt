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

} // namespace heritage

#ifndef HERITAGE_STORAGE_UNIT_REPO_H
#define HERITAGE_STORAGE_UNIT_REPO_H

// 文物单位(units)查询：对应 Go internal/store/unit_repo.go 的 ListUnits。
// M1 仅需 list；其余方法（UnitLevel/UnitStats/CreateUnit/Delete...）随里程碑补。

#include "core/domain/DomainTypes.h"

#include <QSqlDatabase>
#include <QVector>

namespace heritage {

class UnitRepo {
public:
    explicit UnitRepo(QSqlDatabase db);

    // SELECT id,name,level,category,sort FROM units ORDER BY sort,id
    QVector<Unit> list();

    // 某单位的保护级别（level），供 analysis 资质校验用。对应 Go UnitLevel。
    QString level(qint64 id);

    // 新建文物单位，返回新ID。对应 Go CreateUnit。
    qint64 createUnit(const QString& name, const QString& level, int sort);

private:
    QSqlDatabase db_;
};

} // namespace heritage

#endif // HERITAGE_STORAGE_UNIT_REPO_H

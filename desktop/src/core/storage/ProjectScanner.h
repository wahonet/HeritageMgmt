#ifndef HERITAGE_STORAGE_PROJECT_SCANNER_H
#define HERITAGE_STORAGE_PROJECT_SCANNER_H

// 行扫描辅助：对应 Go internal/store/scan.go 的 scanProject / projectCols。
// 固定列顺序，NULL 安全读取（Qt 用 QVariant::isNull()）。

#include "core/domain/DomainTypes.h"

#include <QString>

class QSqlQuery;

namespace heritage {

// projects 查询列顺序常量（与 scan.go 的 projectCols 逐字一致）。
// 任何 SELECT projects 的查询都应使用该列清单，再交由 scanProject 读取。
QString projectColumns();

// 从查询当前行（按 projectColumns 顺序）读取一个 Project。
Project scanProject(const QSqlQuery& q);

} // namespace heritage

#endif // HERITAGE_STORAGE_PROJECT_SCANNER_H

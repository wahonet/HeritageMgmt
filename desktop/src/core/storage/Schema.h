#ifndef HERITAGE_STORAGE_SCHEMA_H
#define HERITAGE_STORAGE_SCHEMA_H

// 表结构定义与旧库字段迁移：对应 Go internal/store/migrate.go。
// SQL 与 Go 版逐字一致，保证桌面版与 web 版读同一份数据库。

// 注意：QStringList 在 Qt6 中是 QList<QString> 的别名，不可前置声明为 class；
//       故在此直接 include。QSqlDatabase 为真正的类，可前置声明。
#include <QStringList>

class QSqlDatabase;

namespace heritage::schema {

// 完整建表语句列表（每条独立，便于逐条 exec）。
// Qt 的 QSqlQuery::exec 只执行单条语句，故拆成列表；Go 版由驱动按 ';' 拆分。
QStringList createStatements();

// 旧库字段迁移：对 projects 表 ALTER ADD COLUMN 缺失列。
// 幂等——用 PRAGMA table_info 检查已有列，只补缺失项。返回是否全部执行成功。
bool migrate(QSqlDatabase db);

// 清空 documents/projects/units 并重置自增序列（供批量导入在事务内调用）。
// 对应 Go store.ResetTables。
bool resetTables(QSqlDatabase db);

// 获取 projects 表现有列名集合（PRAGMA table_info）。
QStringList tableColumns(QSqlDatabase db, const QString& table);

} // namespace heritage::schema

#endif // HERITAGE_STORAGE_SCHEMA_H

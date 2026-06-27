#include "Schema.h"

#include <QPair>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace heritage::schema {

// 与 Go store/migrate.go 的 schema 常量逐字一致（拆为逐条语句）。
QStringList createStatements() {
    return {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS units ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE NOT NULL, "
            "level TEXT, category TEXT, intro TEXT, sort INTEGER DEFAULT 0)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS projects ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, unit_id INTEGER NOT NULL, seq INTEGER, "
            "name TEXT NOT NULL, ptype TEXT, approval_no TEXT, "
            "sign_date TEXT, complete_date TEXT, accept_date TEXT, contract_end TEXT, "
            "owner_unit TEXT, contract_no TEXT, contract_sign_date TEXT, "
            "central_funding REAL, eng_contract REAL, eng_paid REAL, "
            "sup_contract REAL, sup_paid REAL, des_contract REAL, des_paid REAL, "
            "expert_fee REAL, total_paid REAL, status TEXT, progress_note TEXT, "
            "source_dir TEXT, folder TEXT, created TEXT, "
            "construction_unit TEXT, construction_qual TEXT, "
            "design_unit TEXT, design_qual TEXT, "
            "supervision_unit TEXT, supervision_qual TEXT, "
            "FOREIGN KEY(unit_id) REFERENCES units(id))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS documents ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, project_id INTEGER NOT NULL, "
            "doc_type TEXT NOT NULL, doc_type_name TEXT, title TEXT, orig_name TEXT, "
            "file_path TEXT NOT NULL, file_ext TEXT, file_size INTEGER, uploaded TEXT, "
            "source TEXT DEFAULT 'import', "
            "FOREIGN KEY(project_id) REFERENCES projects(id) ON DELETE CASCADE)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS logs ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, ts TEXT, action TEXT, target TEXT, detail TEXT)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_doc_project ON documents(project_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_proj_unit ON projects(unit_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_logs_ts ON logs(ts)"),
    };
}

// 旧库迁移补字段（顺序与 Go migrate() 一致）。
// 注意：新建表已含全部列，故对全新库为 no-op；仅对历史库补齐。
static const QVector<QPair<QString, QString>> kMigrateColumns = {
    {QStringLiteral("construction_unit"), QStringLiteral("TEXT")},
    {QStringLiteral("construction_qual"), QStringLiteral("TEXT")},
    {QStringLiteral("design_unit"), QStringLiteral("TEXT")},
    {QStringLiteral("design_qual"), QStringLiteral("TEXT")},
    {QStringLiteral("supervision_unit"), QStringLiteral("TEXT")},
    {QStringLiteral("supervision_qual"), QStringLiteral("TEXT")},
    {QStringLiteral("contract_end"), QStringLiteral("TEXT")},
    {QStringLiteral("owner_unit"), QStringLiteral("TEXT")},
    {QStringLiteral("contract_no"), QStringLiteral("TEXT")},
    {QStringLiteral("contract_sign_date"), QStringLiteral("TEXT")},
    {QStringLiteral("deleted"), QStringLiteral("INTEGER DEFAULT 0")},
};

QStringList tableColumns(QSqlDatabase db, const QString& table) {
    QStringList cols;
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table)))
        return cols;
    while (q.next())
        cols << q.value(1).toString(); // 字段名在第 2 列
    return cols;
}

bool migrate(QSqlDatabase db) {
    const QStringList have = tableColumns(db, QStringLiteral("projects"));
    const QSet<QString> present(have.begin(), have.end());
    QSqlQuery q(db);
    for (const auto& col : kMigrateColumns) {
        if (present.contains(col.first))
            continue;
        const QString sql = QStringLiteral("ALTER TABLE projects ADD COLUMN %1 %2")
                                .arg(col.first, col.second);
        if (!q.exec(sql))
            return false;
    }
    return true;
}

} // namespace heritage::schema

#ifndef HERITAGE_STORAGE_DATABASE_H
#define HERITAGE_STORAGE_DATABASE_H

// 数据库连接与生命周期：对应 Go internal/store/db.go 的 Store。
// 持有 QSqlDatabase（驱动 QSQLITE），开启外键与 WAL，建表 + 旧库迁移。
// 连接名可指定；默认自动生成唯一名，便于单测多实例并行。

#include <QSqlDatabase>
#include <QString>

namespace heritage {

class Database {
public:
    // dbPath: 数据库文件路径。connectionName: 连接名（空则自动唯一）。
    explicit Database(const QString& dbPath,
                      const QString& connectionName = QString());
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // 打开并初始化（PRAGMA + 建表 + 迁移）。成功返回 true。
    bool open();
    bool isOpen() const;
    void close();

    QSqlDatabase connection() const;
    QString lastError() const;

private:
    QString path_;
    QString connName_;
    QSqlDatabase db_;
    bool added_ = false;  // 是否已 addDatabase（决定析构是否 removeDatabase）
    QString lastError_;
};

} // namespace heritage

#endif // HERITAGE_STORAGE_DATABASE_H

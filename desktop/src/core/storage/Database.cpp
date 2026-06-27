#include "Database.h"
#include "Schema.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace heritage {

// 生成唯一连接名，避免多实例（测试）冲突。
static QString uniqueConnectionName() {
    return QStringLiteral("heritage-%1").arg(QUuid::createUuid().toString(QUuid::Id128));
}

Database::Database(const QString& dbPath, const QString& connectionName)
    : path_(dbPath), connName_(connectionName.isEmpty() ? uniqueConnectionName() : connectionName) {}

Database::~Database() {
    close();
}

bool Database::open() {
    if (isOpen())
        return true;
    db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName_);
    added_ = true;
    db_.setDatabaseName(path_);
    if (!db_.open()) {
        lastError_ = db_.lastError().text();
        return false;
    }
    // 与 Go 版一致：开启外键级联 + WAL。
    QSqlQuery q1(db_);
    q1.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    QSqlQuery q2(db_);
    q2.exec(QStringLiteral("PRAGMA journal_mode = WAL"));

    for (const QString& sql : schema::createStatements()) {
        QSqlQuery q(db_);
        if (!q.exec(sql)) {
            lastError_ = q.lastError().text();
            return false;
        }
    }
    if (!schema::migrate(db_)) {
        lastError_ = QStringLiteral("projects table migrate failed");
        return false;
    }
    return true;
}

bool Database::isOpen() const {
    return db_.isOpen();
}

void Database::close() {
    if (db_.isOpen())
        db_.close();
    db_ = QSqlDatabase(); // 释放该连接的引用，便于 removeDatabase
    if (added_) {
        QSqlDatabase::removeDatabase(connName_);
        added_ = false;
    }
}

QSqlDatabase Database::connection() const {
    return db_;
}

QString Database::lastError() const {
    return lastError_;
}

} // namespace heritage

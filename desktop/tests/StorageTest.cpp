// 数据层单测：打开/建表/迁移/单位与工程查询/NULL 安全扫描。
// 对应 Go internal/store 的行为。退出码 0=通过，1=失败。

#include "core/storage/Database.h"
#include "core/storage/ProjectRepo.h"
#include "core/storage/Schema.h"
#include "core/storage/UnitRepo.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QSqlQuery>
#include <cstdio>
#include <cstdlib>

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__);      \
            return 1;                                                          \
        }                                                                      \
    } while (0)

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    // 唯一临时库，避免与其它实例/历史运行冲突。
    const QString dir = QDir::tempPath() + QStringLiteral("/heritage_test_%1")
                            .arg(QDateTime::currentMSecsSinceEpoch() % 100000);
    QDir().mkpath(dir);
    const QString dbPath = dir + QStringLiteral("/test.db");

    heritage::Database db(dbPath, QStringLiteral("storage_test_conn"));
    CHECK(db.open());
    CHECK(!db.lastError().size() || db.isOpen());

    // ---- 建表：四张表的关键列都应在 ----
    auto cols = heritage::schema::tableColumns(db.connection(), QStringLiteral("projects"));
    CHECK(cols.contains(QStringLiteral("deleted")));
    CHECK(cols.contains(QStringLiteral("central_funding")));
    CHECK(cols.contains(QStringLiteral("contract_end")));
    CHECK(heritage::schema::tableColumns(db.connection(), QStringLiteral("units")).contains(QStringLiteral("name")));
    CHECK(heritage::schema::tableColumns(db.connection(), QStringLiteral("documents")).contains(QStringLiteral("project_id")));
    CHECK(heritage::schema::tableColumns(db.connection(), QStringLiteral("logs")).contains(QStringLiteral("ts")));

    // ---- 迁移幂等：再跑一次 migrate 不应报错 ----
    CHECK(heritage::schema::migrate(db.connection()));

    // ---- 种子数据 ----
    QSqlQuery q(db.connection());
    CHECK(q.exec(QStringLiteral("INSERT INTO units(name,level,category,sort) "
                                "VALUES('测试单位甲','国保','古建筑',1)")));
    // 工程1：带金额、seq、状态
    CHECK(q.exec(QStringLiteral("INSERT INTO projects(unit_id,seq,name,ptype,status,central_funding,sign_date,created) "
                                "VALUES(1,1,'甲工程一','修缮工程','在建',123.45,'2024-01-01','2024-01-01 10:00:00')")));
    // 工程2：金额与 seq 均为 NULL（验证 NULL 安全）
    CHECK(q.exec(QStringLiteral("INSERT INTO projects(unit_id,seq,name,ptype,status,central_funding,created) "
                                "VALUES(1,NULL,'甲工程二','修缮工程','在建',NULL,'2024-01-02 10:00:00')")));

    // ---- 单位查询 ----
    heritage::UnitRepo units(db.connection());
    auto ulist = units.list();
    CHECK(ulist.size() == 1);
    CHECK(ulist[0].name == QStringLiteral("测试单位甲"));
    CHECK(ulist[0].level == QStringLiteral("国保"));

    // ---- 工程查询 ----
    heritage::ProjectRepo projects(db.connection());
    CHECK(projects.count() == 2);

    auto plist = projects.list(1, QString(), QString());
    CHECK(plist.size() == 2);
    // 按名查找，不依赖顺序（SQLite 升序排序时 NULL seq 排前，与 Go ORDER BY 一致）。
    const heritage::Project* withFunding = nullptr;
    const heritage::Project* nullProj = nullptr;
    for (const auto& p : plist) {
        if (p.name == QStringLiteral("甲工程一"))
            withFunding = &p;
        else if (p.name == QStringLiteral("甲工程二"))
            nullProj = &p;
    }
    CHECK(withFunding && nullProj);
    CHECK(withFunding->seq.has_value() && *withFunding->seq == 1);
    CHECK(withFunding->centralFunding.has_value());
    CHECK(*withFunding->centralFunding == 123.45);
    CHECK(!nullProj->seq.has_value());            // NULL
    CHECK(!nullProj->centralFunding.has_value()); // NULL

    // 状态过滤
    CHECK(projects.list(1, QStringLiteral("已竣工"), QString()).size() == 0);

    // get by id
    auto got = projects.get(1);
    CHECK(got.has_value());
    CHECK(got->name == QStringLiteral("甲工程一"));

    QDir(dir).removeRecursively();
    std::printf("storage_test: PASS\n");
    return 0;
}

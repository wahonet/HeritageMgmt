// stats 单测：按单位/类型/年份/状态聚合资金与支付。退出码 0=通过。

#include "core/config/AppConfig.h"
#include "core/storage/Database.h"
#include "core/storage/ProjectRepo.h"
#include "core/stats/StatsService.h"
#include "core/storage/UnitRepo.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QSqlQuery>
#include <cstdio>

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__);      \
            return 1;                                                          \
        }                                                                      \
    } while (0)

// 在结果里找 key 对应分组
static const heritage::StatGroup* find(const QVector<heritage::StatGroup>& v, const QString& k) {
    for (const auto& g : v)
        if (g.k == k)
            return &g;
    return nullptr;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    const QString dir = QDir::tempPath() + QStringLiteral("/heritage_st_%1").arg(QDateTime::currentMSecsSinceEpoch() % 100000);
    QDir().mkpath(dir);
    heritage::Database db(dir + QStringLiteral("/t.db"), QStringLiteral("stats_conn"));
    CHECK(db.open());

    QSqlQuery q(db.connection());
    CHECK(q.exec(QStringLiteral("INSERT INTO units(name,level,sort) VALUES('甲单位','国保',1)")));
    CHECK(q.exec(QStringLiteral("INSERT INTO units(name,level,sort) VALUES('乙单位','省保',2)")));
    // p1: 甲/修缮/在建/2024, 指标100 已付40
    CHECK(q.exec(QStringLiteral("INSERT INTO projects(unit_id,name,ptype,status,sign_date,central_funding,total_paid) "
                                "VALUES(1,'p1','修缮工程','在建','2024-01-01',100,40)")));
    // p2: 甲/修缮/已竣工/2023, 指标200 已付200
    CHECK(q.exec(QStringLiteral("INSERT INTO projects(unit_id,name,ptype,status,sign_date,central_funding,total_paid) "
                                "VALUES(1,'p2','修缮工程','已竣工','2023-06-01',200,200)")));
    // p3: 乙/安防/在建/2024, 指标300 已付0
    CHECK(q.exec(QStringLiteral("INSERT INTO projects(unit_id,name,ptype,status,sign_date,central_funding,total_paid) "
                                "VALUES(2,'p3','安防工程','在建','2024-05-01',300,0)")));

    heritage::ProjectRepo projects(db.connection());
    heritage::UnitRepo units(db.connection());
    heritage::StatsService svc(projects, units);

    const heritage::StatsResult r = svc.aggregate();
    CHECK(r.total.n == 3);
    CHECK(r.total.funding == 600.0);
    CHECK(r.total.paid == 240.0);
    CHECK(r.total.pending == 360.0);

    // byType：修缮工程(2,300)，安防工程(1,300)
    CHECK(r.byType.size() == 2);
    CHECK(find(r.byType, QStringLiteral("修缮工程")) && find(r.byType, QStringLiteral("修缮工程"))->n == 2);
    CHECK(find(r.byType, QStringLiteral("安防工程")) && find(r.byType, QStringLiteral("安防工程"))->n == 1);

    // byYear：升序 [2023(1), 2024(2)]
    CHECK(r.byYear.size() == 2);
    CHECK(r.byYear[0].k == QStringLiteral("2023") && r.byYear[0].n == 1);
    CHECK(r.byYear[1].k == QStringLiteral("2024") && r.byYear[1].n == 2);

    // byStatus：在建(2,400)，已竣工(1,200)
    CHECK(find(r.byStatus, QStringLiteral("在建")) && find(r.byStatus, QStringLiteral("在建"))->n == 2);
    CHECK(find(r.byStatus, QStringLiteral("已竣工")) && find(r.byStatus, QStringLiteral("已竣工"))->n == 1);

    // byUnit：甲单位(2,300)，乙单位(1,300)
    CHECK(find(r.byUnit, QStringLiteral("甲单位")) && find(r.byUnit, QStringLiteral("甲单位"))->funding == 300.0);
    CHECK(find(r.byUnit, QStringLiteral("乙单位")) && find(r.byUnit, QStringLiteral("乙单位"))->funding == 300.0);

    QDir(dir).removeRecursively();
    std::printf("stats_test: PASS\n");
    return 0;
}

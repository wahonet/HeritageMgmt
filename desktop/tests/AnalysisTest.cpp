// analysis 单测：缺项检测/完整度/阶段聚合/资质校验。退出码 0=通过。
// 用真实临时库 + 资源目录里的 doc_types/workflow/rules（集成式，顺带验 SQL 路径）。

#include "core/analysis/AnalysisService.h"
#include "core/config/AppConfig.h"
#include "core/config/ConfigLoader.h"
#include "core/storage/Database.h"
#include "core/storage/DocumentRepo.h"
#include "core/storage/ProjectRepo.h"
#include "core/storage/UnitRepo.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QSqlQuery>
#include <cstdio>

#ifndef HERITAGE_RESOURCE_DIR
#define HERITAGE_RESOURCE_DIR "."
#endif

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__);      \
            return 1;                                                          \
        }                                                                      \
    } while (0)

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    // 加载配置（doc_types/workflow/rules）
    heritage::AppConfig cfg;
    cfg.appBase = QStringLiteral(HERITAGE_RESOURCE_DIR);
    QString err;
    CHECK(heritage::config::loadDocWorkflow(cfg.appBase, cfg.docCfg, cfg.workflow, &err));
    cfg.rules = heritage::config::loadRules(cfg.appBase);
    CHECK(cfg.docCfg.types.size() == 12);

    // 临时库
    const QString dir = QDir::tempPath() + QStringLiteral("/heritage_an_%1").arg(QDateTime::currentMSecsSinceEpoch() % 100000);
    QDir().mkpath(dir);
    heritage::Database db(dir + QStringLiteral("/t.db"), QStringLiteral("analysis_conn"));
    CHECK(db.open());

    QSqlQuery q(db.connection());
    CHECK(q.exec(QStringLiteral("INSERT INTO units(name,level,category,sort) VALUES('测试单位','国保','古建筑',1)")));
    // 工程：在建；设了设计单位但无资质（应触发资质未填写告警）
    CHECK(q.exec(QStringLiteral("INSERT INTO projects(unit_id,name,ptype,status,sign_date,design_unit,created) "
                                "VALUES(1,'测试工程','修缮工程','在建','2024-03-15','某设计院','2024-03-15 10:00:00')")));
    // 文档：仅 approval + funding（2/5 必备）
    CHECK(q.exec(QStringLiteral("INSERT INTO documents(project_id,doc_type,doc_type_name,title,file_path) "
                                "VALUES(1,'approval','批复文','批复文.pdf','data/projects/P0001/approval/x.pdf')")));
    CHECK(q.exec(QStringLiteral("INSERT INTO documents(project_id,doc_type,doc_type_name,title,file_path) "
                                "VALUES(1,'funding','资金下达指标文','资金下达指标文.pdf','data/projects/P0001/funding/x.pdf')")));

    heritage::ProjectRepo projects(db.connection());
    heritage::DocumentRepo docs(db.connection());
    heritage::UnitRepo units(db.connection());
    heritage::AnalysisService svc(projects, docs, units, cfg);

    const auto d = svc.analyze(1);
    CHECK(d.has_value());
    CHECK(d->completeness == 40);                       // 2/5 必备 → 40
    CHECK(d->typeStatus.size() == 12);
    CHECK(d->stages.size() == 9);
    CHECK(d->missingRequired.contains(QStringLiteral("项目合同")));  // construction_contract 缺
    CHECK(d->missingRequired.contains(QStringLiteral("开工报告")));  // start_report 缺
    CHECK(d->missingRequired.contains(QStringLiteral("验收")));      // acceptance 缺
    CHECK(!d->missingRequired.contains(QStringLiteral("批复文")));   // approval 有
    CHECK(d->unitLevel == QStringLiteral("国保"));
    // 资质告警：设计单位已填但资质空 → "设计单位资质未填写"
    bool hasQualWarn = false;
    for (const QString& w : d->qualWarnings)
        if (w.contains(QStringLiteral("设计单位资质未填写")))
            hasQualWarn = true;
    CHECK(hasQualWarn);

    QDir(dir).removeRecursively();
    std::printf("analysis_test: PASS\n");
    return 0;
}

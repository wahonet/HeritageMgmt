// 批量导入集成单测：构造临时 Basicdata(子目录+文件+sample.xlsx) → ImportService.importAll
// → 验单位/工程/文档/财务匹配 + 财务字段落库。退出码 0=通过。

#include "core/config/AppConfig.h"
#include "core/config/ConfigLoader.h"
#include "core/import/ImportService.h"
#include "core/storage/Database.h"
#include "core/storage/DocumentRepo.h"
#include "core/storage/LogRepo.h"
#include "core/storage/ProjectRepo.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QTextStream>
#include <cstdio>

#ifndef HERITAGE_FIXTURE_DIR
#define HERITAGE_FIXTURE_DIR "."
#endif
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

static void writeFile(const QString& path, const char* txt) {
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(txt);
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    const QString tmp = QDir::tempPath() + QStringLiteral("/heritage_imp_%1").arg(QDateTime::currentMSecsSinceEpoch() % 100000);
    const QString bd = tmp + QStringLiteral("/basicdata");
    QDir().mkpath(bd);

    // 财务表
    CHECK(QFile::copy(QStringLiteral(HERITAGE_FIXTURE_DIR) + QStringLiteral("/sample.xlsx"),
                      bd + QStringLiteral("/sample.xlsx")));
    // 工程子目录 + 归档文件（文件名含序号前缀，测 classify/clean）
    QDir().mkpath(bd + QStringLiteral("/1、大雄宝殿修缮工程"));
    writeFile(bd + QStringLiteral("/1、大雄宝殿修缮工程/1、批复文.pdf"), "pdf");
    writeFile(bd + QStringLiteral("/1、大雄宝殿修缮工程/2、资金下达指标文.pdf"), "pdf");
    QDir().mkpath(bd + QStringLiteral("/2、千佛崖抢险加固工程"));
    writeFile(bd + QStringLiteral("/2、千佛崖抢险加固工程/杂项.txt"), "x");

    heritage::AppConfig cfg;
    cfg.appBase = QStringLiteral(HERITAGE_RESOURCE_DIR);
    cfg.dataDir = tmp + QStringLiteral("/data");
    cfg.projectsDir = cfg.dataDir + QStringLiteral("/projects");
    cfg.dbPath = cfg.dataDir + QStringLiteral("/heritage.db");
    QDir().mkpath(cfg.projectsDir);
    QString err;
    CHECK(heritage::config::loadDocWorkflow(cfg.appBase, cfg.docCfg, cfg.workflow, &err));
    cfg.rules = heritage::config::loadRules(cfg.appBase); // deriveStatus 需要 statusKeywords

    heritage::Database db(cfg.dbPath, QStringLiteral("import_conn"));
    CHECK(db.open());
    heritage::LogRepo logs(db.connection());

    heritage::ImportService imp(db.connection(), cfg, logs);
    const heritage::ImportStats st = imp.importAll(bd);
    if (!imp.lastError().isEmpty()) {
        std::printf("FAIL: import error: %s\n", qPrintable(imp.lastError()));
        return 1;
    }
    CHECK(st.units == 1);     // unitRules 空 → 全归"其他文物"
    CHECK(st.projects == 2);
    CHECK(st.docs == 3);      // 2 + 1
    CHECK(st.matched == 2);   // 两个工程名都在 xlsx 中命中

    // 财务字段落库：大雄宝殿修缮工程 central_funding=350.5、status=在建
    heritage::ProjectRepo projects(db.connection());
    heritage::Project found;
    bool ok = false;
    for (const heritage::Project& p : projects.list(0, QString(), QString())) {
        if (p.name == QStringLiteral("大雄宝殿修缮工程")) {
            found = p;
            ok = true;
            break;
        }
    }
    CHECK(ok);
    CHECK(found.centralFunding.has_value() && *found.centralFunding == 350.5);
    CHECK(found.status == QStringLiteral("在建"));
    CHECK(found.folder.startsWith(QStringLiteral("P000")));
    // 文档落库 + 文件已复制
    heritage::DocumentRepo docs(db.connection());
    CHECK(docs.count(found.id) == 2);
    CHECK(QFileInfo::exists(cfg.projectsDir + QStringLiteral("/") + found.folder +
                            QStringLiteral("/approval/1、批复文.pdf")));

    QDir(tmp).removeRecursively();
    std::printf("import_test: PASS\n");
    return 0;
}

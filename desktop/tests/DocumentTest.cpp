// 文档服务单测：上传(复制+插记录+日志)/删除单文档/删除分类。退出码 0=通过。

#include "core/config/AppConfig.h"
#include "core/config/ConfigLoader.h"
#include "core/documents/DocumentService.h"
#include "core/storage/Database.h"
#include "core/storage/DocumentRepo.h"
#include "core/storage/LogRepo.h"
#include "core/storage/ProjectRepo.h"
#include "core/storage/UnitRepo.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QTextStream>
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
    const QString tmp = QDir::tempPath() + QStringLiteral("/heritage_doc_%1").arg(QDateTime::currentMSecsSinceEpoch() % 100000);
    QDir().mkpath(tmp);

    heritage::AppConfig cfg;
    cfg.appBase = QStringLiteral(HERITAGE_RESOURCE_DIR);
    cfg.dataDir = tmp + QStringLiteral("/data");
    cfg.projectsDir = cfg.dataDir + QStringLiteral("/projects");
    cfg.dbPath = cfg.dataDir + QStringLiteral("/heritage.db");
    QDir().mkpath(cfg.projectsDir);
    QString err;
    CHECK(heritage::config::loadDocWorkflow(cfg.appBase, cfg.docCfg, cfg.workflow, &err));

    heritage::Database db(cfg.dbPath, QStringLiteral("doc_conn"));
    CHECK(db.open());

    QSqlQuery q(db.connection());
    CHECK(q.exec(QStringLiteral("INSERT INTO units(name,level,sort) VALUES('测试单位','国保',1)")));
    CHECK(q.exec(QStringLiteral("INSERT INTO projects(unit_id,name,folder,status,created) "
                                "VALUES(1,'测试工程','P0001','在建','2024-01-01 10:00:00')")));

    heritage::ProjectRepo projects(db.connection());
    heritage::DocumentRepo docs(db.connection());
    heritage::LogRepo logs(db.connection());
    heritage::DocumentService svc(projects, docs, logs, cfg);

    // 准备一个源文件
    const QString src = tmp + QStringLiteral("/源批复文.txt");
    { QFile f(src); f.open(QIODevice::WriteOnly); QTextStream(&f) << "hello"; }

    // 上传
    int n = svc.uploadFiles(1, QStringLiteral("approval"), {src});
    CHECK(n == 1);
    auto list = docs.list(1);
    CHECK(list.size() == 1);
    CHECK(list[0].docType == QStringLiteral("approval"));
    CHECK(list[0].docTypeName == QStringLiteral("批复文"));
    CHECK(list[0].source == QStringLiteral("upload"));
    const QString copied = svc.fullPath(list[0]);
    CHECK(QFileInfo::exists(copied));                       // 文件已复制进归档目录
    CHECK(copied.contains(QStringLiteral("P0001/approval/")));
    // 日志
    bool hasLog = false;
    for (const auto& l : logs.list(50))
        if (l.action == QStringLiteral("上传文档"))
            hasLog = true;
    CHECK(hasLog);

    const qint64 docId = list[0].id;

    // 重名上传 → 追加 _HHmmss，不应覆盖
    CHECK(svc.uploadFiles(1, QStringLiteral("approval"), {src}) == 1);
    CHECK(docs.list(1).size() == 2);

    // 删除单文档：文件与记录均消失
    svc.deleteDocument(docId);
    CHECK(!QFileInfo::exists(copied));
    bool gone = true;
    for (const auto& d : docs.list(1))
        if (d.id == docId)
            gone = false;
    CHECK(gone);

    // 删除分类：清空剩余 + 删目录
    CHECK(svc.deleteDocType(1, QStringLiteral("approval")) >= 1);
    CHECK(docs.list(1).isEmpty());
    CHECK(!QDir(cfg.projectsDir + QStringLiteral("/P0001/approval")).exists());

    QFile::remove(src);
    QDir(tmp).removeRecursively();
    std::printf("document_test: PASS\n");
    return 0;
}

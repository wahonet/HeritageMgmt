// 配置层单测：从磁盘加载 doc_types/workflow/rules、资源兜底、默认规则。
// 退出码 0=通过，1=失败。

#include "core/config/AppConfig.h"
#include "core/config/ConfigLoader.h"

#include <QCoreApplication>
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
    const QString resDir = QStringLiteral(HERITAGE_RESOURCE_DIR);

    // ---- doc_types + workflow 从磁盘加载 ----
    heritage::DocTypeCfg doc;
    heritage::Workflow wf;
    QString err;
    CHECK(heritage::config::loadDocWorkflow(resDir, doc, wf, &err));
    CHECK(doc.types.size() == 12);
    CHECK(doc.unknownCode == QStringLiteral("other"));
    CHECK(doc.unknownName == QStringLiteral("其他"));
    CHECK(doc.types[0].code == QStringLiteral("approval"));
    CHECK(doc.types[0].required == true);
    CHECK(doc.types[1].keywords.size() >= 1);
    CHECK(wf.stages.size() == 9);
    CHECK(wf.typeRules.size() == 11);
    CHECK(wf.stages[0].code == QStringLiteral("approve"));

    // ---- rules 从磁盘加载 ----
    heritage::Rules rules = heritage::config::loadRules(resDir);
    CHECK(rules.qualThresholds.contains(QStringLiteral("国保")));
    CHECK(rules.qualThresholds.value(QStringLiteral("国保")).design == QStringLiteral("甲级"));
    CHECK(rules.qualThresholds.value(QStringLiteral("国保")).construction == QStringLiteral("一级"));
    CHECK(rules.statusKeywords.contains(QStringLiteral("已竣工")));
    CHECK(rules.statusKeywords.value(QStringLiteral("已竣工")).contains(QStringLiteral("竣工")));

    // ---- 默认规则 ----
    heritage::Rules def = heritage::config::defaultRules();
    CHECK(def.qualThresholds.value(QStringLiteral("国保")).supervision == QStringLiteral("甲级"));

    // ---- 资源兜底：appBase 不存在时应回退 :/config/<name>（需链接 qrc）----
    const QByteArray fromRes = heritage::config::readFile(QStringLiteral("/no/such/base"),
                                                          QStringLiteral("doc_types.json"));
    CHECK(!fromRes.isEmpty());
    CHECK(fromRes.contains("unknown_code"));

    std::printf("config_test: PASS\n");
    return 0;
}

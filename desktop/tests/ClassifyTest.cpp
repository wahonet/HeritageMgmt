// classify 单测：搬运 Go internal/classify/classify_test.go 的用例。退出码 0=通过。

#include "core/classify/Classify.h"
#include "core/domain/ConfigTypes.h"

#include <QCoreApplication>
#include <QMap>
#include <cstdio>

using namespace heritage;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__);      \
            return 1;                                                          \
        }                                                                      \
    } while (0)

static QVector<DocType> testTypes() {
    return {
        {QStringLiteral("approval"), QStringLiteral("批复文"), {QStringLiteral("批复文")}, {}, true, {}},
        {QStringLiteral("funding"), QStringLiteral("资金下达指标文"),
         {QStringLiteral("资金下达指标文"), QStringLiteral("指标文")}, {}, true, {}},
        {QStringLiteral("construction_contract"), QStringLiteral("项目合同"),
         {QStringLiteral("项目合同"), QStringLiteral("施工合同")}, {}, true, {}},
        {QStringLiteral("start_report"), QStringLiteral("开工报告"), {QStringLiteral("开工报告")}, {}, true, {}},
        {QStringLiteral("acceptance"), QStringLiteral("验收"), {QStringLiteral("验收")}, {}, true, {}},
        {QStringLiteral("engineering_fee"), QStringLiteral("工程费"), {QStringLiteral("工程费")}, {}, false, {}},
        {QStringLiteral("design_fee"), QStringLiteral("设计费"), {QStringLiteral("设计费")}, {}, false, {}},
        {QStringLiteral("catalog"), QStringLiteral("目录"), {QStringLiteral("目录")}, {}, false, {}},
    };
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    const auto types = testTypes();

    // ---- ClassifyDoc ----
    struct Case { const char* fn; const char* want; };
    const Case cls[] = {
        {"1、批复文（某文物保[2019]45号）.pdf", "approval"},
        {"2、资金下达指标文（某财教指[2024]30号）.pdf", "funding"},
        {"3、项目合同（修缮工程合同）.pdf", "construction_contract"},
        {"4、开工报告（修缮工程开工报告）.pdf", "start_report"},
        {"5、验收（四方验评表）.pdf", "acceptance"},
        {"8、工程费.pdf", "engineering_fee"},
        {"9、设计费（修缮工程设计费）.pdf", "design_fee"},
        {"目录.docx", "catalog"},
        {"某个不匹配的文件.xyz", "other"},
    };
    for (const auto& c : cls) {
        const auto r = classify::classifyDoc(QString::fromUtf8(c.fn), types,
                                             QStringLiteral("other"), QStringLiteral("其他"));
        CHECK(r.code == QLatin1String(c.want));
    }

    // ---- ParseSeq ----
    auto seq = classify::parseSeq(QStringLiteral("3、某文物修缮工程"));
    CHECK(seq.has_value() && *seq == 3);
    CHECK(!classify::parseSeq(QStringLiteral("无序号工程")).has_value());

    // ---- DetectType ----
    QVector<TypeRule> trules = {
        {QStringLiteral("安防工程"), {QStringLiteral("安防")}},
        {QStringLiteral("消防工程"), {QStringLiteral("消防")}},
        {QStringLiteral("抢险加固工程"), {QStringLiteral("抢险")}},
        {QStringLiteral("本体保护工程"), {QStringLiteral("本体保护")}},
    };
    CHECK(classify::detectType(QStringLiteral("某文物安防升级改造工程"), trules) == QStringLiteral("安防工程"));
    CHECK(classify::detectType(QStringLiteral("某古建筑消防工程"), trules) == QStringLiteral("消防工程"));
    CHECK(classify::detectType(QStringLiteral("某古建筑抢险修缮工程"), trules) == QStringLiteral("抢险加固工程"));
    CHECK(classify::detectType(QStringLiteral("某遗址本体保护工程"), trules) == QStringLiteral("本体保护工程"));

    // ---- CleanProjectName ----
    CHECK(classify::cleanProjectName(QStringLiteral("3、某文物修缮工程")) == QStringLiteral("某文物修缮工程"));
    CHECK(classify::cleanProjectName(QStringLiteral("12、某消防工程")) == QStringLiteral("某消防工程"));
    CHECK(classify::cleanProjectName(QStringLiteral("无序号工程")) == QStringLiteral("无序号工程"));

    // ---- Similarity ----
    CHECK(classify::similarity(QStringLiteral("某文物修缮工程"), QStringLiteral("某文物修缮工程")) == 1.0);
    CHECK(classify::similarity({}, QStringLiteral("abc")) == 0.0);
    CHECK(classify::similarity(QStringLiteral("某文物修缮"), QStringLiteral("某文物修缮本体保护")) > 0.5);

    // ---- CleanTitle（保留扩展名）----
    CHECK(classify::cleanTitle(QStringLiteral("3、批复文.pdf")) == QStringLiteral("批复文.pdf"));

    // ---- MatchFinancial ----
    QVector<QMap<QString, QString>> rows = {
        {{QStringLiteral("_name"), QStringLiteral("某某工程")}},
        {{QStringLiteral("_name"), QStringLiteral("无关行")}},
    };
    auto m = classify::matchFinancial(QStringLiteral("某某工程"), rows);
    CHECK(m.index == 0 && m.score >= 0.6);

    std::printf("classify_test: PASS\n");
    return 0;
}

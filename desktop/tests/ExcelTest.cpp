// excelimport 单测：用真实 .xlsx(自写解析器读) 验 loadFinancials/finGet/parseFloat/
// trimDate/deriveStatus。退出码 0=通过。

#include "core/excelimport/ExcelImport.h"

#include <QCoreApplication>
#include <QHash>
#include <QStringList>
#include <cstdio>

#ifndef HERITAGE_FIXTURE_DIR
#define HERITAGE_FIXTURE_DIR "."
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

    const auto records = heritage::excel::loadFinancials(QStringLiteral(HERITAGE_FIXTURE_DIR));
    CHECK(records.size() == 3); // 小计行(非数字序号)被过滤

    CHECK(records[0].value(QStringLiteral("_name")) == QStringLiteral("大雄宝殿修缮工程"));
    CHECK(records[1].value(QStringLiteral("_name")) == QStringLiteral("千佛崖抢险加固"));
    CHECK(records[2].value(QStringLiteral("_name")) == QStringLiteral("天贶殿保护"));

    // finGet / parseFloat
    CHECK(heritage::excel::finGet(records[0], QStringLiteral("central_funding")) == QStringLiteral("350.5"));
    const auto cf = heritage::excel::parseFloat(
        heritage::excel::finGet(records[0], QStringLiteral("central_funding")));
    CHECK(cf.has_value() && *cf == 350.5);
    CHECK(heritage::excel::finGet(records[0], QStringLiteral("eng_contract")) == QStringLiteral("300"));
    CHECK(heritage::excel::finGet(records[0], QStringLiteral("progress_note")) == QStringLiteral("在建"));
    // candidate 回退：财政指标文下达金额 在，central_funding 直接命中
    CHECK(!heritage::excel::finGet(records[0], QStringLiteral("central_funding")).isEmpty());

    // deriveStatus（用默认 rules 关键词）
    QHash<QString, QStringList> kw = {
        {QStringLiteral("已竣工"), {QStringLiteral("验收"), QStringLiteral("竣工"), QStringLiteral("完工")}},
        {QStringLiteral("在建"), {QStringLiteral("在建"), QStringLiteral("施工")}},
    };
    CHECK(heritage::excel::deriveStatus(records[0], kw) == QStringLiteral("在建"));   // 在建
    CHECK(heritage::excel::deriveStatus(records[1], kw) == QStringLiteral("已竣工")); // 已竣工验收

    // trimDate
    CHECK(heritage::excel::trimDate(QStringLiteral("2024-03-15 00:00:00")) == QStringLiteral("2024-03-15"));
    CHECK(heritage::excel::trimDate(QStringLiteral("abc")).isEmpty() == false);

    std::printf("excel_test: PASS\n");
    return 0;
}

// 报告单测：生成 PDF 到临时文件，验非空 + %PDF 头 + LevelName。退出码 0=通过。
// QPdfWriter/QTextDocument 需 GUI 环境 → QGuiApplication(offscreen)。

#include "core/report/Report.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <cstdio>

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__);      \
            return 1;                                                          \
        }                                                                      \
    } while (0)

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    heritage::report::ReportData rd;
    rd.project.name = QStringLiteral("大雄宝殿修缮工程");
    rd.project.unitName = QStringLiteral("灵岩寺文物保护所");
    rd.project.ptype = QStringLiteral("修缮工程");
    rd.project.status = QStringLiteral("在建");
    rd.project.approvalNo = QStringLiteral("文物字〔2024〕12号");
    rd.project.constructionUnit = QStringLiteral("某施工公司");
    rd.project.constructionQual = QStringLiteral("一级");
    rd.project.centralFunding = 350.5;
    rd.project.totalPaid = 120.0;
    rd.project.signDate = QStringLiteral("2024-03-15");
    rd.unitLevel = QStringLiteral("国保");
    rd.completeness = 60;
    rd.missingRequired = {QStringLiteral("开工报告"), QStringLiteral("验收")};
    rd.analysis = QStringLiteral("（测试）该工程档案基本齐全，资金支付率偏低，建议加快拨付进度。");

    const QString tmp = QDir::tempPath() + QStringLiteral("/rep_%1.pdf").arg(
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddHHmmss")));
    QString err;
    CHECK(heritage::report::generateReport(rd, tmp, &err));
    QFile f(tmp);
    CHECK(f.open(QIODevice::ReadOnly));
    CHECK(f.size() > 500);
    CHECK(f.read(5).startsWith("%PDF"));

    // LevelName
    CHECK(heritage::report::levelName(QStringLiteral("国保")) == QStringLiteral("全国重点文物保护单位"));
    CHECK(heritage::report::levelName(QString()) == QStringLiteral("未定级文物点"));

    f.close();
    f.remove();
    std::printf("report_test: PASS\n");
    return 0;
}

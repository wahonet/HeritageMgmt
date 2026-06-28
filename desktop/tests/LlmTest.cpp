// LLM 单测（离线部分）：buildRequestBody / parseContent / configured / buildAnalysisPrompt。
// 不联网，仅测纯函数。退出码 0=通过。

#include "core/llm/Client.h"
#include "core/report/Analysis.h"
#include "core/report/Report.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__);      \
            return 1;                                                          \
        }                                                                      \
    } while (0)

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    using namespace heritage;
    // ---- buildRequestBody ----
    LlmConfig cfg;
    cfg.model = QStringLiteral("deepseek-chat");
    llm::Options opts;
    opts.jsonObject = true;
    opts.maxTokens = 100;
    opts.temperature = 0.2;
    const QByteArray body = llm::Client::buildRequestBody(
        cfg, {{QStringLiteral("user"), QStringLiteral("hi")}}, opts);
    const QJsonObject o = QJsonDocument::fromJson(body).object();
    CHECK(o.value(QStringLiteral("model")).toString() == QStringLiteral("deepseek-chat"));
    CHECK(o.value(QStringLiteral("messages")).toArray().size() == 1);
    CHECK(o.value(QStringLiteral("response_format")).toObject().value(QStringLiteral("type")).toString() ==
          QStringLiteral("json_object"));
    CHECK(o.value(QStringLiteral("max_tokens")).toInt() == 100);

    // ---- parseContent ----
    QString err;
    QString content = llm::Client::parseContent(
        QByteArrayLiteral(R"({"choices":[{"message":{"content":"  hello  "}}]})"), &err);
    CHECK(content == QStringLiteral("hello"));
    CHECK(err.isEmpty());

    content = llm::Client::parseContent(QByteArrayLiteral(R"({"error":{"message":"bad key"}})"), &err);
    CHECK(content.isEmpty());
    CHECK(err.contains(QStringLiteral("bad key")));

    content = llm::Client::parseContent(QByteArrayLiteral(R"({"choices":[]})"), &err);
    CHECK(content.isEmpty());
    CHECK(!err.isEmpty());

    // ---- configured ----
    cfg.apiKey.clear();
    CHECK(!llm::Client(cfg).configured());
    cfg.apiKey = QStringLiteral("sk-x");
    CHECK(llm::Client(cfg).configured());

    // ---- buildAnalysisPrompt ----
    report::ReportData rd;
    rd.project.name = QStringLiteral("大雄宝殿修缮工程");
    rd.project.centralFunding = 350.5;
    rd.project.totalPaid = 120.0;
    rd.completeness = 60;
    rd.missingRequired = {QStringLiteral("开工报告")};
    const QString prompt = report::buildAnalysisPrompt(rd);
    CHECK(prompt.contains(QStringLiteral("大雄宝殿修缮工程")));
    CHECK(prompt.contains(QStringLiteral("60%")));
    CHECK(prompt.contains(QStringLiteral("开工报告")));
    CHECK(prompt.contains(QStringLiteral("文物保护工程档案分析专家")));

    std::printf("llm_test: PASS\n");
    return 0;
}

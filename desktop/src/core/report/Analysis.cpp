#include "Analysis.h"

#include <QStringList>

namespace heritage::report {

namespace {
QString fptr(const std::optional<double>& v) { return v.has_value() ? QString::number(*v, 'f', 0) : QStringLiteral("0"); }
} // namespace

QString buildAnalysisPrompt(const ReportData& rd) {
    const Project& p = rd.project;
    QString payRate = QStringLiteral("—");
    if (p.centralFunding.has_value() && p.totalPaid.has_value() && *p.centralFunding > 0)
        payRate = QString::number(*p.totalPaid / *p.centralFunding * 100.0, 'f', 1) + QStringLiteral("%");

    const QString summary = QStringLiteral(
        "工程名称：%1\n"
        "文物单位：%2  级别：%3\n"
        "工程类型：%4  当前状态：%5\n"
        "批复文号：%6\n"
        "合同工期：%7 至 %8  实际完工：%9\n"
        "中央指标：%10万元  已支付：%11万元  支付率：%12\n"
        "建设单位：%13\n"
        "施工单位：%14（资质：%15）\n"
        "设计单位：%16（资质：%17）\n"
        "监理单位：%18（资质：%19）\n"
        "文档总数：%20  完整度：%21%\n"
        "缺项：%22\n"
        "资质校验：%23")
        .arg(p.name, p.unitName, levelName(rd.unitLevel), p.ptype, p.status, p.approvalNo,
             p.signDate, p.contractEnd, p.completeDate, fptr(p.centralFunding), fptr(p.totalPaid),
             payRate, p.ownerUnit, p.constructionUnit, p.constructionQual, p.designUnit,
             p.designQual, p.supervisionUnit, p.supervisionQual,
             QString::number(rd.documents.size()), QString::number(rd.completeness),
             rd.missingRequired.isEmpty() ? QStringLiteral("无") : rd.missingRequired.join(QStringLiteral("、")),
             rd.qualWarnings.isEmpty() ? QStringLiteral("无") : rd.qualWarnings.join(QStringLiteral("；")));

    return QStringLiteral(
        "你是文物保护工程档案分析专家。请根据以下工程数据，生成一份专业、正式的工程管理分析报告"
        "（约500-800字）。报告应包含：\n"
        "1. 工程概况评价\n2. 资金执行评价（支付率是否合理）\n3. 进度评价（工期是否逾期）\n"
        "4. 参建单位资质评价\n5. 档案完整性评价（缺项分析）\n6. 综合结论与建议\n"
        "语气正式客观，不用markdown，直接输出纯文本。\n\n工程数据：\n") + summary;
}

QString generateAnalysis(llm::Client& client, const ReportData& rd, int timeoutMs, QString* err) {
    if (err) err->clear();
    if (!client.configured())
        return QStringLiteral("（未配置大模型API密钥，分析报告功能不可用。）");
    llm::Options opts;
    opts.temperature = 0.3;
    opts.maxTokens = 2000;
    opts.timeoutMs = timeoutMs > 0 ? timeoutMs : 90000;
    return client.chat({{QStringLiteral("user"), buildAnalysisPrompt(rd)}}, opts, err);
}

} // namespace heritage::report

#include "AnalysisService.h"

#include <QHash>

namespace heritage {

AnalysisService::AnalysisService(ProjectRepo& projects, DocumentRepo& docs, UnitRepo& units,
                                 const AppConfig& cfg)
    : projects_(projects), docs_(docs), units_(units), cfg_(cfg) {}

std::optional<ProjectDetail> AnalysisService::analyze(qint64 projectId) {
    const auto projOpt = projects_.get(projectId);
    if (!projOpt)
        return std::nullopt;
    const Project& proj = *projOpt;

    const QVector<Document> docs = docs_.list(projectId);

    // 各分类文档数
    QHash<QString, int> counts;
    for (const Document& d : docs)
        counts[d.docType]++;

    // 类型状态
    QVector<TypeStatus> typeStatus;
    for (const DocType& t : cfg_.docCfg.types) {
        const int n = counts.value(t.code, 0);
        typeStatus.append({t.code, t.name, t.required, t.stage, n, n > 0});
    }

    // 必备缺项 / 完整度
    QStringList missingReq, missingOpt;
    int requiredTotal = 0, requiredHave = 0;
    for (const TypeStatus& ts : typeStatus) {
        if (ts.required) {
            requiredTotal++;
            if (ts.count == 0)
                missingReq.append(ts.name);
            else
                requiredHave++;
        }
    }
    if (proj.status == QStringLiteral("已竣工")) {
        for (const TypeStatus& ts : typeStatus)
            if (!ts.required && ts.count == 0 && ts.code != QStringLiteral("catalog"))
                missingOpt.append(ts.name);
    }
    int completeness = 100;
    if (requiredTotal > 0)
        completeness = requiredHave * 100 / requiredTotal;

    // 阶段聚合
    QHash<QString, TypeStatus> tsByCode;
    for (const TypeStatus& ts : typeStatus)
        tsByCode.insert(ts.code, ts);
    QVector<StageOut> stages;
    for (const Stage& s : cfg_.workflow.stages) {
        QVector<Document> stDocs;
        for (const Document& d : docs)
            for (const QString& c : s.docs)
                if (d.docType == c) {
                    stDocs.append(d);
                    break;
                }
        QVector<TypeStatus> stTypes;
        for (const QString& c : s.docs) {
            auto it = tsByCode.constFind(c);
            if (it != tsByCode.cend())
                stTypes.append(*it);
        }
        stages.append({s.code, s.name, stDocs.size(), stTypes, stDocs});
    }

    ProjectDetail d;
    d.project = proj;
    d.unitLevel = units_.level(proj.unitId);
    d.documents = docs;
    d.typeStatus = typeStatus;
    d.stages = stages;
    d.missingRequired = missingReq;
    d.missingOptional = missingOpt;
    d.completeness = completeness;
    d.qualWarnings = qualWarnings(proj);
    return d;
}

QStringList AnalysisService::qualWarnings(const Project& p) const {
    QStringList warns;
    const QString level = units_.level(p.unitId);
    if (level.isEmpty())
        return warns;
    // 未配置的级别 → 空 QualThreshold（仅校验"未填写"，不校验等级阈值）
    const QualThreshold req = cfg_.rules.qualThresholds.value(level);
    struct Check { QString label, qual, unit, req; };
    const Check checks[] = {
        {QStringLiteral("设计单位资质"), p.designQual, p.designUnit, req.design},
        {QStringLiteral("施工单位资质"), p.constructionQual, p.constructionUnit, req.construction},
        {QStringLiteral("监理单位资质"), p.supervisionQual, p.supervisionUnit, req.supervision},
    };
    for (const Check& c : checks) {
        if (c.qual.isEmpty() && !c.unit.isEmpty()) {
            warns.append(c.label + QStringLiteral("未填写"));
        } else if (!c.qual.isEmpty() && !c.req.isEmpty() && !c.qual.contains(c.req)) {
            warns.append(c.label + QStringLiteral("为「") + c.qual + QStringLiteral("」，") +
                         level + QStringLiteral("工程建议") + c.req + QStringLiteral("及以上"));
        }
    }
    return warns;
}

} // namespace heritage

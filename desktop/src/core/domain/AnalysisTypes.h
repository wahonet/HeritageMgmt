#ifndef HERITAGE_DOMAIN_ANALYSIS_TYPES_H
#define HERITAGE_DOMAIN_ANALYSIS_TYPES_H

// 分析结果类型：对应 Go internal/domain/analysis.go。
// TypeStatus（缺项检测）/ StageOut（阶段聚合）/ ProjectDetail（工程完整分析）。

#include "core/domain/DomainTypes.h"

#include <QStringList>
#include <QVector>

namespace heritage {

// 某文档类型在工程中的存在状态（缺项检测用）
struct TypeStatus {
    QString code;
    QString name;
    bool required = false;
    QString stage;
    int count = 0;
    bool has = false;
};

// 工作流阶段的聚合输出（含该阶段类型与文档）
struct StageOut {
    QString code;
    QString name;
    int docCount = 0;
    QVector<TypeStatus> types;
    QVector<Document> documents;
};

// 单个工程的完整分析结果
struct ProjectDetail {
    Project project;
    QString unitLevel;
    QVector<Document> documents;
    QVector<TypeStatus> typeStatus;
    QVector<StageOut> stages;
    QStringList missingRequired;
    QStringList missingOptional;
    int completeness = 100;
    QStringList qualWarnings;
};

} // namespace heritage

#endif // HERITAGE_DOMAIN_ANALYSIS_TYPES_H

#ifndef HERITAGE_CONFIG_TYPES_H
#define HERITAGE_CONFIG_TYPES_H

// 配置层数据类型：对应 Go internal/domain 中的配置结构与 config.Rules。
// 由 config/ConfigLoader 从 JSON 解析填充。
#include <QString>
#include <QStringList>
#include <QHash>
#include <QVector>

namespace heritage {

// 文档分类定义（doc_types.json -> types[]）
struct DocType {
    QString code;
    QString name;
    QStringList keywords;
    QString stage;
    bool required = false;
    QString desc;
};

// 文档分类配置集合（doc_types.json 顶层）
struct DocTypeCfg {
    QVector<DocType> types;
    QString unknownCode;
    QString unknownName;
};

// 工作流阶段（workflow.json -> stages[]）
struct Stage {
    QString code;
    QString name;
    QStringList docs;
};

// 文物单位识别规则（workflow.json -> units.rules[]）
struct UnitRule {
    QString unit;
    QString level;
    QString category;
    QStringList keywords;
};

// 工程类型识别规则（workflow.json -> project_types.rules[]）
struct TypeRule {
    QString type;
    QStringList keywords;
};

// 工作流配置（workflow.json 顶层）
struct Workflow {
    QVector<Stage> stages;
    QVector<UnitRule> unitRules;
    QVector<TypeRule> typeRules;
};

// 某保护级别对 设计/施工/监理 资质的最低要求（rules.json -> qual_thresholds[level]）
struct QualThreshold {
    QString design;
    QString construction;
    QString supervision;
};

// 业务规则集合（rules.json）
struct Rules {
    QHash<QString, QualThreshold> qualThresholds;
    QHash<QString, QStringList> statusKeywords;
};

} // namespace heritage

#endif // HERITAGE_CONFIG_TYPES_H

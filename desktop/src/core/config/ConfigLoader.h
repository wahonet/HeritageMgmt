#ifndef HERITAGE_CONFIG_CONFIG_LOADER_H
#define HERITAGE_CONFIG_CONFIG_LOADER_H

// 配置加载：对应 Go internal/config/config.go + rules.go。
// 语义：磁盘 <appBase>/config/<name> 优先，否则用编译进资源的默认配置（:/config/<name>）。
// rules 缺失/非法回退默认（与 Go LoadRules 一致）。

#include "core/domain/ConfigTypes.h"

#include <QByteArray>
#include <QString>

namespace heritage::config {

// 读取 config/<name>：磁盘优先，资源兜底。返回空 QByteArray 表示两者都读不到。
QByteArray readFile(const QString& appBase, const QString& name);

// 解析 doc_types.json + workflow.json。失败返回 false 并写 err。
bool loadDocWorkflow(const QString& appBase, DocTypeCfg& doc, Workflow& wf, QString* err);

// 解析 rules.json；缺失/非法回退 defaultRules()。
Rules loadRules(const QString& appBase);

// 默认规则（与 Go defaultRules 一致：国保资质阈值 + 状态关键词）。
Rules defaultRules();

} // namespace heritage::config

#endif // HERITAGE_CONFIG_CONFIG_LOADER_H

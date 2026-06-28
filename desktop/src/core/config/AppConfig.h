#ifndef HERITAGE_CONFIG_APP_CONFIG_H
#define HERITAGE_CONFIG_APP_CONFIG_H

// 应用配置容器：对应 Go internal/config/config.go 的 Config + NewConfig/ResolvePaths。
// 持有路径与已加载的 文档/工作流/规则 配置。由 main 在启动时构造并注入。

#include "core/domain/ConfigTypes.h"

#include <QString>
#include <optional>

namespace heritage {

struct AppConfig {
    QString appBase;     // 可执行文件同级目录（config/ 与 data/ 均相对此）
    QString dataDir;     // appBase/data
    QString projectsDir; // appBase/data/projects
    QString dbPath;      // appBase/data/heritage.db

    DocTypeCfg docCfg;
    Workflow workflow;
    Rules rules;
    LlmConfig llm;

    // 解析路径（exe 同级优先，回退 cwd）+ 创建 data 目录 + 加载配置。
    // 失败返回 std::nullopt 并写 err。
    static std::optional<AppConfig> resolve(QString* err = nullptr);
};

} // namespace heritage

#endif // HERITAGE_CONFIG_APP_CONFIG_H

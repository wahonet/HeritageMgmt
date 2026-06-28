#ifndef HERITAGE_ANALYSIS_ANALYSIS_SERVICE_H
#define HERITAGE_ANALYSIS_ANALYSIS_SERVICE_H

// 工程分析服务：缺项检测 / 完整度 / 阶段聚合 / 资质校验。
// 对应 Go internal/service/project_service.go 的 Analyze + qualWarnings。

#include "core/config/AppConfig.h"
#include "core/domain/AnalysisTypes.h"
#include "core/storage/DocumentRepo.h"
#include "core/storage/ProjectRepo.h"
#include "core/storage/UnitRepo.h"

#include <optional>

namespace heritage {

class AnalysisService {
public:
    AnalysisService(ProjectRepo& projects, DocumentRepo& docs, UnitRepo& units, const AppConfig& cfg);

    // 汇总单个工程的缺项检测、阶段聚合与资质校验。工程不存在返回 nullopt。
    std::optional<ProjectDetail> analyze(qint64 projectId);

private:
    // 依据 rules.qualThresholds[level] 校验参建单位资质。
    QStringList qualWarnings(const Project& p) const;

    ProjectRepo& projects_;
    DocumentRepo& docs_;
    UnitRepo& units_;
    const AppConfig& cfg_;
};

} // namespace heritage

#endif // HERITAGE_ANALYSIS_ANALYSIS_SERVICE_H

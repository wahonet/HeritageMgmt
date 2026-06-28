#ifndef HERITAGE_STATS_STATS_SERVICE_H
#define HERITAGE_STATS_STATS_SERVICE_H

// 统计聚合服务：按 单位/类型/年份/状态 聚合资金与支付。
// 对应 Go internal/service/stats_service.go 的 Aggregate。

#include "core/domain/StatTypes.h"
#include "core/storage/ProjectRepo.h"
#include "core/storage/UnitRepo.h"

namespace heritage {

class StatsService {
public:
    StatsService(ProjectRepo& projects, UnitRepo& units);

    StatsResult aggregate();

private:
    ProjectRepo& projects_;
    UnitRepo& units_;
};

} // namespace heritage

#endif // HERITAGE_STATS_STATS_SERVICE_H

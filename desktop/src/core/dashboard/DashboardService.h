#ifndef HERITAGE_DASHBOARD_DASHBOARD_SERVICE_H
#define HERITAGE_DASHBOARD_DASHBOARD_SERVICE_H

// 看板聚合服务：汇总指标 + 每工程必备缺项。对应 Go ProjectService.Dashboard。
// 注：Go 原版用 ProjectName(unitID) 取单位名（实为 bug，会取到错的工程名/空），
//     本实现改用 UnitRepo 取正确单位名——功能正确优先于复刻 bug。

#include "core/config/AppConfig.h"
#include "core/domain/DomainTypes.h"
#include "core/storage/DocumentRepo.h"
#include "core/storage/ProjectRepo.h"
#include "core/storage/UnitRepo.h"

#include <QStringList>
#include <QVector>

namespace heritage {

struct DashboardProjectRow {
    Project project;
    QString unitName;
    int docCount = 0;
    QStringList missing;      // 必备缺项名称
    QStringList missingCodes; // 必备缺项 code
};

struct DashboardData {
    int totalProjects = 0;
    int done = 0;          // 无必备缺项的工程数
    int withMissing = 0;
    double centralFunding = 0;
    double totalPaid = 0;
    QVector<DashboardProjectRow> rows;
};

class DashboardService {
public:
    DashboardService(ProjectRepo& projects, DocumentRepo& docs, UnitRepo& units, const AppConfig& cfg);

    DashboardData dashboard();

private:
    ProjectRepo& projects_;
    DocumentRepo& docs_;
    UnitRepo& units_;
    const AppConfig& cfg_;
};

} // namespace heritage

#endif // HERITAGE_DASHBOARD_DASHBOARD_SERVICE_H

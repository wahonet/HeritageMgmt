#ifndef HERITAGE_RECYCLE_RECYCLE_SERVICE_H
#define HERITAGE_RECYCLE_RECYCLE_SERVICE_H

// 回收站：工程软删/恢复/彻底删、单位级联删（协调 DB 记录与磁盘文件）。
// 对应 Go internal/service/recycle_service.go。归档目录在 软删↔恢复 时于
// projectsDir/<folder> 与 dataDir/recycle/<folder> 之间搬移。

#include "core/config/AppConfig.h"
#include "core/domain/DomainTypes.h"
#include "core/storage/LogRepo.h"
#include "core/storage/ProjectRepo.h"
#include "core/storage/UnitRepo.h"

namespace heritage {

class RecycleService {
public:
    RecycleService(ProjectRepo& projects, UnitRepo& units, LogRepo& logs, const AppConfig& cfg);

    // 软删除工程并移文件入回收站。
    bool recycleProject(const Project& p);
    // 从回收站恢复工程（取消软删 + 文件搬回）。
    void restoreProject(const RecycledProject& r);
    // 彻底删除工程（DB 记录 + 回收站文件）。
    void purgeProject(const RecycledProject& r);
    // 删除单位：其下工程文件入回收站，再删单位/工程/文档 DB 记录。返回受影响工程数。
    int deleteUnitCascade(qint64 unitId);

private:
    void recycleFolderToBin(const QString& folder); // 移 projectsDir/<folder> → dataDir/recycle/<folder>

    ProjectRepo& projects_;
    UnitRepo& units_;
    LogRepo& logs_;
    const AppConfig& cfg_;
};

} // namespace heritage

#endif // HERITAGE_RECYCLE_RECYCLE_SERVICE_H

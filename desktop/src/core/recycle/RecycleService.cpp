#include "RecycleService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace heritage {

RecycleService::RecycleService(ProjectRepo& projects, UnitRepo& units, LogRepo& logs,
                               const AppConfig& cfg)
    : projects_(projects), units_(units), logs_(logs), cfg_(cfg) {}

void RecycleService::recycleFolderToBin(const QString& folder) {
    if (folder.isEmpty())
        return;
    const QString src = cfg_.projectsDir + QStringLiteral("/") + folder;
    if (!QFileInfo::exists(src))
        return;
    const QString bin = cfg_.dataDir + QStringLiteral("/recycle");
    QDir().mkpath(bin);
    // 目标若已存在先移除（避免 rename 失败）
    const QString dst = bin + QStringLiteral("/") + folder;
    if (QFileInfo::exists(dst))
        QDir(dst).removeRecursively();
    QDir().rename(src, dst);
}

bool RecycleService::recycleProject(const Project& p) {
    if (!projects_.softDelete(p.id))
        return false;
    recycleFolderToBin(p.folder);
    logs_.insert(QStringLiteral("删除工程"), QStringLiteral("工程#%1 %2").arg(p.id).arg(p.name), p.folder);
    return true;
}

void RecycleService::restoreProject(const RecycledProject& r) {
    projects_.restore(r.id);
    const QString recyclePath = cfg_.dataDir + QStringLiteral("/recycle/") + r.folder;
    if (QFileInfo::exists(recyclePath))
        QDir().rename(recyclePath, cfg_.projectsDir + QStringLiteral("/") + r.folder);
    logs_.insert(QStringLiteral("恢复工程"), QStringLiteral("工程#%1 %2").arg(r.id).arg(r.name), r.folder);
}

void RecycleService::purgeProject(const RecycledProject& r) {
    projects_.purge(r.id);
    QDir(cfg_.dataDir + QStringLiteral("/recycle/") + r.folder).removeRecursively();
    logs_.insert(QStringLiteral("彻底删除"), QStringLiteral("工程#%1 %2").arg(r.id).arg(r.name), r.folder);
}

int RecycleService::deleteUnitCascade(qint64 unitId) {
    const QVector<qint64> pids = projects_.idsByUnit(unitId);
    for (qint64 pid : pids) {
        const auto proj = projects_.get(pid);
        if (proj)
            recycleFolderToBin(proj->folder);
    }
    units_.deleteRecords(unitId);
    logs_.insert(QStringLiteral("删除单位"), QStringLiteral("单位#%1").arg(unitId),
                 QStringLiteral("级联%1个工程").arg(pids.size()));
    return pids.size();
}

} // namespace heritage

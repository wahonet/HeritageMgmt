#ifndef HERITAGE_STORAGE_PROJECT_REPO_H
#define HERITAGE_STORAGE_PROJECT_REPO_H

// 工程(projects)查询：对应 Go internal/store/project_repo.go。
// M1 仅需 list/get/count；其余方法随里程碑补。

#include "core/domain/DomainTypes.h"
#include "core/storage/ProjectScanner.h"

#include <QSqlDatabase>
#include <QVector>
#include <optional>

namespace heritage {

class ProjectRepo {
public:
    explicit ProjectRepo(QSqlDatabase db);

    // 列出未软删工程；unitId<=0 不按单位过滤；status/q 为空则不过滤。
    // 与 Go ListProjects 一致：WHERE COALESCE(deleted,0)=0 [AND unit_id=?] [AND status=?]
    //   [AND (name LIKE ? OR approval_no LIKE ?)] ORDER BY unit_id,seq,id
    QVector<Project> list(qint64 unitId, const QString& status, const QString& q);

    // 按 id 取单个工程（含全部列）。
    std::optional<Project> get(qint64 id);

    // 工程总数（含已软删，对齐 Go CountProjects）。
    int count();

private:
    QSqlDatabase db_;
};

} // namespace heritage

#endif // HERITAGE_STORAGE_PROJECT_REPO_H

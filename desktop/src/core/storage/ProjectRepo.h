#ifndef HERITAGE_STORAGE_PROJECT_REPO_H
#define HERITAGE_STORAGE_PROJECT_REPO_H

// 工程(projects)查询：对应 Go internal/store/project_repo.go。
// M1 仅需 list/get/count；其余方法随里程碑补。

#include "core/domain/DomainTypes.h"
#include "core/storage/ProjectScanner.h"

#include <QSet>
#include <QSqlDatabase>
#include <QVariantList>
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

    // 工程名（按 id）。对应 Go ProjectName。
    QString name(qint64 id);

    // 某工程已有的文档类型集合。对应 Go ProjectDocTypes。
    QSet<QString> docTypes(qint64 projectId);

    // 按字段更新工程。sets 为 "a=?,b=?"，vals 为对应值（不含 id，函数内追加）。对应 Go UpdateProjectFields。
    bool updateFields(qint64 id, const QString& sets, const QVariantList& vals);
    // 新建工程，返回新ID。对应 Go CreateProject。
    qint64 create(qint64 unitId, const QString& name, const QString& ptype, const QString& status);
    // 设置工程归档目录名。对应 Go SetProjectFolder。
    bool setFolder(qint64 id, const QString& folder);

private:
    QSqlDatabase db_;
};

} // namespace heritage

#endif // HERITAGE_STORAGE_PROJECT_REPO_H

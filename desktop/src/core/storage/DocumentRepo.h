#ifndef HERITAGE_STORAGE_DOCUMENT_REPO_H
#define HERITAGE_STORAGE_DOCUMENT_REPO_H

// 文档(documents)查询：对应 Go internal/store/document_repo.go 的 ListDocuments/CountDocs。

#include "core/domain/DomainTypes.h"

#include <QSqlDatabase>
#include <QVector>
#include <optional>

namespace heritage {

class DocumentRepo {
public:
    explicit DocumentRepo(QSqlDatabase db);

    QVector<Document> list(qint64 projectId); // WHERE project_id=? ORDER BY id
    int count(qint64 projectId);              // COUNT(*)

    // 按 id 取单个文档；不存在返回 nullopt。对应 Go DocByID。
    std::optional<Document> byId(qint64 id);
    // 插入一条文档记录。对应 Go InsertDocument。
    bool insert(const Document& d);
    // 删除一条文档记录（不删文件）。对应 Go DeleteDocument。
    void remove(qint64 id);
    // 删除某工程某分类全部文档记录，返回删除条数。对应 Go DeleteDocsByType。
    int removeByType(qint64 projectId, const QString& docType);

private:
    QSqlDatabase db_;
};

} // namespace heritage

#endif // HERITAGE_STORAGE_DOCUMENT_REPO_H

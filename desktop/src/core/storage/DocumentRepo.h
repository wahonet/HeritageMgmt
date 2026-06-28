#ifndef HERITAGE_STORAGE_DOCUMENT_REPO_H
#define HERITAGE_STORAGE_DOCUMENT_REPO_H

// 文档(documents)查询：对应 Go internal/store/document_repo.go 的 ListDocuments/CountDocs。

#include "core/domain/DomainTypes.h"

#include <QSqlDatabase>
#include <QVector>

namespace heritage {

class DocumentRepo {
public:
    explicit DocumentRepo(QSqlDatabase db);

    QVector<Document> list(qint64 projectId); // WHERE project_id=? ORDER BY id
    int count(qint64 projectId);              // COUNT(*)

private:
    QSqlDatabase db_;
};

} // namespace heritage

#endif // HERITAGE_STORAGE_DOCUMENT_REPO_H

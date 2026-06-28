#include "DocumentRepo.h"

#include <QSqlQuery>

namespace heritage {

DocumentRepo::DocumentRepo(QSqlDatabase db) : db_(db) {}

QVector<Document> DocumentRepo::list(qint64 projectId) {
    QVector<Document> out;
    QSqlQuery q(db_);
    if (!q.prepare(QStringLiteral(
            "SELECT id,project_id,doc_type,doc_type_name,title,orig_name,"
            "file_path,file_ext,file_size,uploaded,source FROM documents WHERE project_id=? ORDER BY id")))
        return out;
    q.addBindValue(projectId);
    if (!q.exec())
        return out;
    while (q.next()) {
        Document d;
        d.id = q.value(0).toLongLong();
        d.projectId = q.value(1).toLongLong();
        d.docType = q.value(2).toString();
        d.docTypeName = q.value(3).toString();
        d.title = q.value(4).toString();
        d.origName = q.value(5).toString();
        d.filePath = q.value(6).toString();
        d.fileExt = q.value(7).toString();
        d.fileSize = q.value(8).toLongLong();
        d.uploaded = q.value(9).toString();
        d.source = q.value(10).toString();
        out.append(d);
    }
    return out;
}

int DocumentRepo::count(qint64 projectId) {
    QSqlQuery q(db_);
    if (!q.prepare(QStringLiteral("SELECT COUNT(*) FROM documents WHERE project_id=?")))
        return 0;
    q.addBindValue(projectId);
    if (!q.exec() || !q.next())
        return 0;
    return q.value(0).toInt();
}

} // namespace heritage

#ifndef HERITAGE_DOCUMENTS_DOCUMENT_SERVICE_H
#define HERITAGE_DOCUMENTS_DOCUMENT_SERVICE_H

// 文档服务：上传(复制文件进归档目录+插记录+日志)/删除单文档/删除分类/磁盘路径。
// 对应 Go httpapi/document_handlers.go 的 handleUpload/handleDeleteDoc/handleDeleteDocType。
// 文件预览(系统打开)属 UI 层，用 fullPath() 得到路径后由 UI 调 QDesktopServices。

#include "core/config/AppConfig.h"
#include "core/domain/DomainTypes.h"
#include "core/storage/DocumentRepo.h"
#include "core/storage/LogRepo.h"
#include "core/storage/ProjectRepo.h"

#include <QString>
#include <QStringList>

namespace heritage {

class DocumentService {
public:
    DocumentService(ProjectRepo& projects, DocumentRepo& docs, LogRepo& logs, const AppConfig& cfg);

    // 上传：把每个本地源文件复制进 <projectsDir>/<folder>/<docType>/（重名追加 _HHmmss），
    //       插文档记录并记日志。返回成功上传数。
    int uploadFiles(qint64 projectId, const QString& docType, const QStringList& sourceFiles);

    // 文档在磁盘上的完整路径（projectsDir + rel，rel 用 / 分隔）。
    QString fullPath(const Document& d) const;

    // 删除单个文档（删文件 + DB 记录 + 日志）。
    void deleteDocument(qint64 id);

    // 删除某工程某分类全部文档（DB 记录 + 整个分类目录 + 日志），返回删除条数。
    int deleteDocType(qint64 projectId, const QString& docType);

private:
    QString docTypeName(const QString& code) const; // 找不到返回 "其他"

    ProjectRepo& projects_;
    DocumentRepo& docs_;
    LogRepo& logs_;
    const AppConfig& cfg_;
};

} // namespace heritage

#endif // HERITAGE_DOCUMENTS_DOCUMENT_SERVICE_H

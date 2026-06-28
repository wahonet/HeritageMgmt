#include "DocumentService.h"

#include "core/classify/Classify.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace heritage {

DocumentService::DocumentService(ProjectRepo& projects, DocumentRepo& docs, LogRepo& logs,
                                 const AppConfig& cfg)
    : projects_(projects), docs_(docs), logs_(logs), cfg_(cfg) {}

QString DocumentService::docTypeName(const QString& code) const {
    for (const DocType& t : cfg_.docCfg.types)
        if (t.code == code)
            return t.name;
    return QStringLiteral("其他");
}

int DocumentService::uploadFiles(qint64 projectId, const QString& docType,
                                 const QStringList& sourceFiles) {
    const auto proj = projects_.get(projectId);
    if (!proj || docType.isEmpty())
        return 0;
    const QString tname = docTypeName(docType);
    const QString destDir = cfg_.projectsDir + QStringLiteral("/") + proj->folder +
                            QStringLiteral("/") + docType;
    QDir().mkpath(destDir);

    const QString now = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    int count = 0;
    for (const QString& src : sourceFiles) {
        const QFileInfo sfi(src);
        const QString fname = sfi.fileName();
        if (fname.isEmpty())
            continue;
        // 重名追加 _HHmmss（与 Go 一致）
        QString dst = destDir + QStringLiteral("/") + fname;
        if (QFileInfo::exists(dst)) {
            const QString base = sfi.completeBaseName();
            const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("HHmmss"));
            dst = destDir + QStringLiteral("/") + base + QStringLiteral("_") + stamp +
                  (sfi.suffix().isEmpty() ? QString() : QStringLiteral(".") + sfi.suffix());
        }
        if (!QFile::copy(src, dst))
            continue;

        // rel：相对 projectsDir，正斜杠
        QString rel = QDir(cfg_.projectsDir).relativeFilePath(dst);
        rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
        const QString ext = sfi.suffix().toLower();
        const qint64 size = QFileInfo(dst).size();

        Document d;
        d.projectId = projectId;
        d.docType = docType;
        d.docTypeName = tname;
        d.title = classify::cleanTitle(fname);
        d.origName = fname;
        d.filePath = rel;
        d.fileExt = ext;
        d.fileSize = size;
        d.uploaded = now;
        d.source = QStringLiteral("upload");
        docs_.insert(d);
        ++count;
    }
    if (count > 0)
        logs_.insert(QStringLiteral("上传文档"),
                     QStringLiteral("工程#%1 %2").arg(projectId).arg(tname),
                     QStringLiteral("%1个文件").arg(count));
    return count;
}

QString DocumentService::fullPath(const Document& d) const {
    return cfg_.projectsDir + QStringLiteral("/") + d.filePath;
}

void DocumentService::deleteDocument(qint64 id) {
    const auto doc = docs_.byId(id);
    if (!doc)
        return;
    QFile::remove(fullPath(*doc));
    docs_.remove(id);
    logs_.insert(QStringLiteral("删除文档"),
                 QStringLiteral("工程#%1 %2").arg(doc->projectId).arg(doc->docTypeName),
                 doc->origName);
}

int DocumentService::deleteDocType(qint64 projectId, const QString& docType) {
    const auto proj = projects_.get(projectId);
    const int deleted = docs_.removeByType(projectId, docType);
    if (proj) {
        const QString folder = cfg_.projectsDir + QStringLiteral("/") + proj->folder +
                               QStringLiteral("/") + docType;
        QDir(folder).removeRecursively();
    }
    const QString pname = proj ? proj->name : QString();
    logs_.insert(QStringLiteral("删除分类"),
                 QStringLiteral("工程#%1 %2 / %3").arg(projectId).arg(pname).arg(docType),
                 QStringLiteral("删除%1个文件").arg(deleted));
    return deleted;
}

} // namespace heritage

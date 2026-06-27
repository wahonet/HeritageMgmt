#ifndef HERITAGE_DOMAIN_TYPES_H
#define HERITAGE_DOMAIN_TYPES_H

// 核心数据类型：对应 Go internal/domain 中的实体。
// 零依赖（仅 Qt 基础类型），可被 storage / config / ui / 业务层自由引用。
#include <QString>
#include <QtGlobal>
#include <optional>

namespace heritage {

// 文物保护单位（units 表）
struct Unit {
    qint64 id = 0;
    QString name;
    QString level;
    QString category;
    int sort = 0;
};

// 工程（projects 表）。金额单位：万元，存 REAL；NULL 表示未填 → std::optional。
// seq 可空；字符串字段在 Go 版以 NullString→"" 读取，此处一律 QString（空串等同 NULL）。
struct Project {
    qint64 id = 0;
    qint64 unitId = 0;
    std::optional<qint64> seq;
    QString name;
    QString ptype;
    QString approvalNo;
    QString signDate;
    QString completeDate;
    QString acceptDate;
    QString contractEnd;
    QString ownerUnit;
    QString contractNo;
    QString contractSignDate;
    std::optional<double> centralFunding;
    std::optional<double> engContract;
    std::optional<double> engPaid;
    std::optional<double> supContract;
    std::optional<double> supPaid;
    std::optional<double> desContract;
    std::optional<double> desPaid;
    std::optional<double> expertFee;
    std::optional<double> totalPaid;
    QString status;
    QString progressNote;
    QString sourceDir;
    QString folder;
    QString created;
    // 参建单位与资质
    QString constructionUnit;
    QString constructionQual;
    QString designUnit;
    QString designQual;
    QString supervisionUnit;
    QString supervisionQual;
    // 附带展示字段（JOIN/聚合得出，非表列）
    QString unitName;
    int docCount = 0;
};

// 文档（documents 表）
struct Document {
    qint64 id = 0;
    qint64 projectId = 0;
    QString docType;
    QString docTypeName;
    QString title;
    QString origName;
    QString filePath;
    QString fileExt;
    qint64 fileSize = 0;
    QString uploaded;
    QString source;
};

// 操作日志（logs 表）
struct LogEntry {
    qint64 id = 0;
    QString ts;
    QString action;
    QString target;
    QString detail;
};

} // namespace heritage

#endif // HERITAGE_DOMAIN_TYPES_H

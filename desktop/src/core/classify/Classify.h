#ifndef HERITAGE_CLASSIFY_CLASSIFY_H
#define HERITAGE_CLASSIFY_CLASSIFY_H

// 纯函数：文档/单位/类型识别、序号解析、名称清理、相似度。对应 Go internal/classify。
// 全部以配置(rules/types)作参，不读全局/DB/文件系统，便于单测。

#include "core/domain/ConfigTypes.h"

#include <QMap>
#include <QString>
#include <QVector>
#include <optional>
#include <utility>

namespace heritage::classify {

// 从文件夹名提取前缀序号（如 "3、xxx" → 3）；无则 nullopt。
std::optional<qint64> parseSeq(const QString& folder);

// 去工程名序号前缀。
QString cleanProjectName(const QString& folder);

// 去文件名序号前缀（保留扩展名）。
QString cleanTitle(const QString& filename);

struct UnitDetect {
    QString unit;
    QString level;
    QString category;
};
// 按关键词识别文物单位；未命中返回 {"其他文物","",""}。
UnitDetect detectUnit(const QString& name, const QVector<UnitRule>& rules);

// 按关键词识别工程类型；未命中返回 "其他工程"。
QString detectType(const QString& name, const QVector<TypeRule>& rules);

struct DocClass {
    QString code;
    QString name;
};
// 按文件名关键词归类文档；未命中返回 (unknownCode, unknownName)。
DocClass classifyDoc(const QString& filename, const QVector<DocType>& types,
                     const QString& unknownCode, const QString& unknownName);

// 归一化名称：仅保留中文(一-龥)/字母/数字，用于相似度匹配。
QString normName(const QString& s);

// 相似度（LCS，按 UTF-8 字节计，与 Go 逐字一致）。相同=1.0，含空串=0。
double similarity(const QString& a, const QString& b);

// 在财务行中为某工程名找最相似的一条。finRows 每条的 "_name" 字段为项目名称。
// 返回 {命中行下标, 分数}；未达阈值(0.6) index=-1。
struct FinancialMatch {
    int index = -1;
    double score = 0.0;
};
FinancialMatch matchFinancial(const QString& name, const QVector<QMap<QString, QString>>& finRows);

} // namespace heritage::classify

#endif // HERITAGE_CLASSIFY_CLASSIFY_H

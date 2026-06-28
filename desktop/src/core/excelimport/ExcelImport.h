#ifndef HERITAGE_EXCELIMPORT_EXCEL_IMPORT_H
#define HERITAGE_EXCELIMPORT_EXCEL_IMPORT_H

// 财务 Excel 导入：对应 Go internal/excelimport/financials.go。
// 用自写 XlsxReader 读 .xlsx，按表头建行记录；fieldMap/FinGet/ParseFloat/TrimDate/DeriveStatus。

#include <QHash>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

namespace heritage::excel {

// 读 basicdataDir 下首个 .xlsx，返回以表头为键的行记录；每条 _name=项目名称(第二列)。
QVector<QMap<QString, QString>> loadFinancials(const QString& basicdataDir);

// 按 DB 字段名从财务记录取值（依次尝试 fieldMap 候选表头）
QString finGet(const QMap<QString, QString>& fin, const QString& field);

// 解析金额，空/非法返回 nullopt
std::optional<double> parseFloat(const QString& s);

// 规范化日期：形如 "2021-12-10 00:00:00" 取前 10 位
QString trimDate(const QString& s);

// 推导工程状态：progress_note 关键词(已竣工>在建) → 有开工日期→在建 → 前期。
// statusKeywords 由调用方注入(来自 config/rules)，保持本模块无配置依赖。
QString deriveStatus(const QMap<QString, QString>& fin,
                     const QHash<QString, QStringList>& statusKeywords);

} // namespace heritage::excel

#endif // HERITAGE_EXCELIMPORT_EXCEL_IMPORT_H

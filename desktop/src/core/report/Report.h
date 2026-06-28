#ifndef HERITAGE_REPORT_REPORT_H
#define HERITAGE_REPORT_REPORT_H

// PDF 报告：对应 Go internal/reporting/pdf.go。
// 用 QPdfWriter + QTextDocument(HTML) 排版（替代 gopdf）；中文字体优先随程序/系统。

#include "core/domain/AnalysisTypes.h"
#include "core/domain/DomainTypes.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace heritage::report {

struct ReportData {
    Project project;
    QString unitLevel;
    int completeness = 100;
    QStringList missingRequired;
    QVector<Document> documents;
    QStringList qualWarnings;
    QString analysis; // 智能分析文本（可为空）
};

// 保护级别全称。对应 Go LevelName。
QString levelName(const QString& level);

// 查找可用中文字体（应用自带 fonts/ 优先，其次系统常见路径）；无则返回空。
QString findChineseFont(const QString& appBase);

// 渲染报告 PDF 到 outPath。成功返回 true。字体缺失时不阻断（用默认/系统字体）。
bool generateReport(const ReportData& rd, const QString& outPath, QString* err = nullptr);

} // namespace heritage::report

#endif // HERITAGE_REPORT_REPORT_H

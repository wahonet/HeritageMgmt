#ifndef HERITAGE_REPORT_ANALYSIS_H
#define HERITAGE_REPORT_ANALYSIS_H

// 智能分析文本生成：基于报告数据调大模型生成约 500-800 字分析。
// 对应 Go reporting/analysis.go。prompt 构建抽为纯函数供单测；未配置密钥返回占位（无错误）。

#include "core/llm/Client.h"
#include "core/report/Report.h"

#include <QString>

namespace heritage::report {

// 构建发给大模型的提示词（含工程数据摘要）。纯函数，可单测。
QString buildAnalysisPrompt(const ReportData& rd);

// 生成分析文本。client 未配置密钥时返回占位说明（*err 清空）；失败写 *err。
QString generateAnalysis(llm::Client& client, const ReportData& rd, int timeoutMs, QString* err);

} // namespace heritage::report

#endif // HERITAGE_REPORT_ANALYSIS_H

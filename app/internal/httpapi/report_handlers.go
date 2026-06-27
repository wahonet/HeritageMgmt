package httpapi

// 工程报告PDF导出的 HTTP 处理：取分析数据 → 组装 reporting.ReportData →
// 调 reporting 生成分析文本与PDF。排版/分析逻辑见 internal/reporting。
// LLM 客户端由 Server 持有并注入（reporting.GenerateAnalysis 收 LLMClient 接口）。

import (
	"fmt"
	"net/http"
	"strconv"
	"strings"
	"time"

	"heritage-mgmt/internal/llm"
	"heritage-mgmt/internal/reporting"
)

// handleReportPDF 生成工程报告PDF
func (s *Server) handleReportPDF(w http.ResponseWriter, r *http.Request) {
	pid, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	d := s.proj.Analyze(pid)
	if d == nil {
		http.NotFound(w, r)
		return
	}
	proj := d.Project
	rd := reporting.ReportData{
		Project:         d.Project,
		UnitLevel:       d.UnitLevel,
		Completeness:    d.Completeness,
		MissingRequired: d.MissingRequired,
		Documents:       d.Documents,
		QualWarnings:    d.QualWarnings,
	}

	analysis, err := reporting.GenerateAnalysis(s.llm, rd, llm.Timeout(s.cfg.LLM, 120*time.Second))
	if err != nil {
		analysis = "分析报告生成失败：" + err.Error()
	}
	rd.Analysis = analysis

	pdf, err := reporting.Generate(rd, s.cfg.AppBase)
	if err != nil {
		writeJSON(w, map[string]interface{}{"error": err.Error()})
		return
	}

	filename := fmt.Sprintf("报告_%s_%s.pdf", strings.ReplaceAll(proj.Name, "/", "_"), time.Now().Format("20060102"))
	w.Header().Set("Content-Type", "application/pdf")
	w.Header().Set("Content-Disposition", "attachment; filename*=UTF-8''"+urlEncode(filename))
	w.Write(pdf)
	s.logs.InsertLog("导出报告", fmt.Sprintf("工程#%d %s", pid, proj.Name), filename)
}

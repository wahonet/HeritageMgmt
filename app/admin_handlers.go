package main

// HTTP 处理：配置/单位列表/重新导入/操作日志/OCR 识别。均为 *Server 方法。

import (
	"fmt"
	"net/http"
	"strconv"

	"heritage-mgmt/internal/domain"
)

func (s *Server) handleConfig(w http.ResponseWriter, r *http.Request) {
	writeJSON(w, map[string]interface{}{
		"doc_types": s.cfg.DocCfg.Types,
		"workflow":  s.cfg.WfCfg.Stages,
	})
}

func (s *Server) handleUnits(w http.ResponseWriter, r *http.Request) {
	out, err := s.proj.ListUnits()
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}
	writeJSON(w, out)
}

func (s *Server) handleImport(w http.ResponseWriter, r *http.Request) {
	stats, err := s.imp.ImportAll(false)
	if err != nil {
		writeErr(w, err.Error())
		return
	}
	s.logs.InsertLog("重新导入", "全部数据", fmt.Sprintf("工程%d/文档%d", stats.Projects, stats.Docs))
	writeJSON(w, map[string]interface{}{"ok": true, "stats": stats})
}

func (s *Server) handleLogs(w http.ResponseWriter, r *http.Request) {
	limit := 300
	if l := r.URL.Query().Get("limit"); l != "" {
		if v, err := strconv.Atoi(l); err == nil {
			limit = v
		}
	}
	logs, _ := s.logs.ListLogs(limit)
	if logs == nil {
		logs = []domain.LogEntry{}
	}
	writeJSON(w, logs)
}

func (s *Server) handleOCRScan(w http.ResponseWriter, r *http.Request) {
	pid, _ := strconv.ParseInt(r.URL.Query().Get("project_id"), 10, 64)
	if pid == 0 {
		writeErr(w, "缺少 project_id")
		return
	}
	fields, err := s.ocrSvc.ScanContracts(pid)
	if err != nil {
		writeErr(w, err.Error())
		return
	}
	n := s.ocrSvc.ApplyFields(pid, fields)
	s.logs.InsertLog("OCR识别", fmt.Sprintf("工程#%d %s", pid, s.projects.ProjectName(pid)), fmt.Sprintf("回填%d个字段", n))
	writeJSON(w, map[string]interface{}{"ok": true, "fields": fields, "applied": n})
}

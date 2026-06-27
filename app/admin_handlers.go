package main

// HTTP 处理：配置/单位列表/重新导入/操作日志/OCR 识别。

import (
	"fmt"
	"net/http"
	"strconv"

	"heritage-mgmt/internal/domain"
)

func handleConfig(w http.ResponseWriter, r *http.Request) {
	writeJSON(w, map[string]interface{}{
		"doc_types": docCfg.Types,
		"workflow":  wfCfg.Stages,
	})
}

func handleUnits(w http.ResponseWriter, r *http.Request) {
	units, _ := ListUnits()
	out := []unitListItem{}
	for _, u := range units {
		pc, dc, cf := UnitStats(u.ID)
		out = append(out, unitListItem{Unit: u, ProjectCount: pc, DocCount: dc, CentralFunding: cf})
	}
	writeJSON(w, out)
}

func handleImport(w http.ResponseWriter, r *http.Request) {
	stats, err := ImportAll(absBasicdata, false)
	if err != nil {
		writeErr(w, err.Error())
		return
	}
	InsertLog("重新导入", "全部数据", fmt.Sprintf("工程%d/文档%d", stats.Projects, stats.Docs))
	writeJSON(w, map[string]interface{}{"ok": true, "stats": stats})
}

func handleLogs(w http.ResponseWriter, r *http.Request) {
	limit := 300
	if l := r.URL.Query().Get("limit"); l != "" {
		if v, err := strconv.Atoi(l); err == nil {
			limit = v
		}
	}
	logs, _ := ListLogs(limit)
	if logs == nil {
		logs = []domain.LogEntry{}
	}
	writeJSON(w, logs)
}

func handleOCRScan(w http.ResponseWriter, r *http.Request) {
	pid, _ := strconv.ParseInt(r.URL.Query().Get("project_id"), 10, 64)
	if pid == 0 {
		writeErr(w, "缺少 project_id")
		return
	}
	fields, err := scanProjectContracts(pid)
	if err != nil {
		writeErr(w, err.Error())
		return
	}
	n := applyExtractedFields(pid, fields)
	InsertLog("OCR识别", fmt.Sprintf("工程#%d %s", pid, ProjectName(pid)), fmt.Sprintf("回填%d个字段", n))
	writeJSON(w, map[string]interface{}{"ok": true, "fields": fields, "applied": n})
}

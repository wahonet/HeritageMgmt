package main

// HTTP 处理：删除/回收站生命周期（删工程/删单位/回收站列表/恢复/彻底删除）。

import (
	"fmt"
	"net/http"
	"strconv"

	"heritage-mgmt/internal/domain"
)

// ---- 删除工程(软删除,放入回收站) ----
func handleDeleteProject(w http.ResponseWriter, r *http.Request) {
	pid, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	proj, err := GetProject(pid)
	if err != nil || proj == nil {
		writeErr(w, "工程不存在")
		return
	}
	if err := RecycleProject(proj); err != nil {
		writeErr(w, err.Error())
		return
	}
	InsertLog("删除工程", fmt.Sprintf("工程#%d %s", pid, proj.Name), "已放入回收站")
	writeJSON(w, map[string]interface{}{"ok": true})
}

// ---- 删除文物单位(连同工程一起放入回收站) ----
func handleDeleteUnit(w http.ResponseWriter, r *http.Request) {
	uid, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	name := UnitName(uid)
	if name == "" {
		writeErr(w, "单位不存在")
		return
	}
	projCount := DeleteUnitCascade(uid)
	detail := ""
	if projCount > 0 {
		detail = fmt.Sprintf("同时删除%d个工程(已放入回收站)", projCount)
	}
	InsertLog("删除单位", name, detail)
	writeJSON(w, map[string]interface{}{"ok": true, "projects_deleted": projCount})
}

// ---- 回收站 ----
func handleRecycle(w http.ResponseWriter, r *http.Request) {
	items, _ := ListRecycled()
	if items == nil {
		items = []domain.RecycledProject{}
	}
	writeJSON(w, items)
}

// ---- 恢复工程 ----
func handleRestoreProject(w http.ResponseWriter, r *http.Request) {
	pid, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	proj, err := GetProject(pid)
	if err != nil || proj == nil {
		writeErr(w, "工程不存在")
		return
	}
	RestoreProject(proj)
	InsertLog("恢复工程", fmt.Sprintf("工程#%d %s", pid, proj.Name), "从回收站恢复")
	writeJSON(w, map[string]interface{}{"ok": true})
}

// ---- 彻底删除工程 ----
func handlePurgeProject(w http.ResponseWriter, r *http.Request) {
	pid, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	proj, err := GetProject(pid)
	if err != nil || proj == nil {
		writeErr(w, "工程不存在")
		return
	}
	PurgeProject(proj)
	InsertLog("彻底删除", fmt.Sprintf("工程#%d %s", pid, proj.Name), "不可恢复")
	writeJSON(w, map[string]interface{}{"ok": true})
}

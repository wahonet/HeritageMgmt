package main

// HTTP 处理：工程相关（列表/详情/更新/看板/文件树/新建）。均为 *Server 方法。

import (
	"encoding/json"
	"fmt"
	"net/http"
	"strconv"
	"strings"
)

func (s *Server) handleProjects(w http.ResponseWriter, r *http.Request) {
	unitID, _ := strconv.ParseInt(r.URL.Query().Get("unit_id"), 10, 64)
	status := r.URL.Query().Get("status")
	q := strings.TrimSpace(r.URL.Query().Get("q"))
	out, err := s.proj.List(unitID, status, q)
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}
	writeJSON(w, out)
}

func (s *Server) handleProject(w http.ResponseWriter, r *http.Request) {
	id, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	d := s.proj.Analyze(id)
	if d == nil {
		http.NotFound(w, r)
		return
	}
	writeJSON(w, d)
}

func (s *Server) handleUpdateProject(w http.ResponseWriter, r *http.Request) {
	id, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	var body map[string]interface{}
	json.NewDecoder(r.Body).Decode(&body)
	logParts, err := s.proj.Update(id, body)
	if err != nil {
		writeErr(w, err.Error())
		return
	}
	s.logs.InsertLog("编辑工程", fmt.Sprintf("工程#%d %s", id, s.projects.ProjectName(id)), strings.Join(logParts, ", "))
	writeJSON(w, map[string]interface{}{"ok": true})
}

func (s *Server) handleDashboard(w http.ResponseWriter, r *http.Request) {
	writeJSON(w, s.proj.Dashboard())
}

// ---- 文件结构树 ----
func (s *Server) handleProjectTree(w http.ResponseWriter, r *http.Request) {
	pid, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	writeJSON(w, s.proj.Tree(pid))
}

// ---- 新建工程 ----
func (s *Server) handleCreateProject(w http.ResponseWriter, r *http.Request) {
	var body struct {
		Name    string `json:"name"`
		UnitID  int64  `json:"unit_id"`
		NewUnit string `json:"new_unit"`
		Level   string `json:"level"`
		Ptype   string `json:"ptype"`
		Status  string `json:"status"`
	}
	json.NewDecoder(r.Body).Decode(&body)
	pid, folder, err := s.proj.CreateWizard(CreateProjectInput{
		Name: body.Name, UnitID: body.UnitID, NewUnit: body.NewUnit,
		Level: body.Level, Ptype: body.Ptype, Status: body.Status,
	})
	if err != nil {
		writeErr(w, err.Error())
		return
	}
	s.logs.InsertLog("新建工程", fmt.Sprintf("工程#%d %s", pid, body.Name), "通过添加项目向导")
	writeJSON(w, map[string]interface{}{"ok": true, "id": pid, "folder": folder})
}

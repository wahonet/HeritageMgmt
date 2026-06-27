package main

// HTTP 处理：工程相关（列表/详情/更新/看板/文件树/新建）。

import (
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"heritage-mgmt/internal/domain"
)

func handleProjects(w http.ResponseWriter, r *http.Request) {
	unitID, _ := strconv.ParseInt(r.URL.Query().Get("unit_id"), 10, 64)
	status := r.URL.Query().Get("status")
	q := strings.TrimSpace(r.URL.Query().Get("q"))
	projects, err := ListProjects(unitID, status, q)
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}
	out := []projectListItem{}
	for _, p := range projects {
		item := projectListItem{
			Project:         p,
			UnitName:        UnitName(p.UnitID),
			DocCount:        CountDocs(p.ID),
			MissingRequired: []string{},
		}
		if d := analyzeProject(p.ID); d != nil {
			item.Completeness = d.Completeness
			item.MissingRequired = d.MissingRequired
		}
		out = append(out, item)
	}
	writeJSON(w, out)
}

func handleProject(w http.ResponseWriter, r *http.Request) {
	id, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	d := analyzeProject(id)
	if d == nil {
		http.NotFound(w, r)
		return
	}
	writeJSON(w, d)
}

func handleUpdateProject(w http.ResponseWriter, r *http.Request) {
	id, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	var body map[string]interface{}
	json.NewDecoder(r.Body).Decode(&body)
	allowed := []string{"approval_no", "sign_date", "complete_date", "accept_date", "contract_end",
		"central_funding", "eng_contract", "eng_paid", "sup_contract", "sup_paid",
		"des_contract", "des_paid", "expert_fee", "total_paid", "status", "progress_note", "ptype",
		"construction_unit", "construction_qual", "design_unit", "design_qual",
		"supervision_unit", "supervision_qual", "owner_unit", "contract_no", "contract_sign_date", "name"}
	var sets, logParts []string
	var args []interface{}
	for _, k := range allowed {
		if v, ok := body[k]; ok {
			sets = append(sets, k+"=?")
			logParts = append(logParts, fmt.Sprintf("%s=%v", k, v))
			if s, isStr := v.(string); isStr && s == "" {
				args = append(args, nil)
			} else {
				args = append(args, v)
			}
		}
	}
	if len(sets) == 0 {
		writeErr(w, "无可更新字段")
		return
	}
	if err := UpdateProjectFields(id, strings.Join(sets, ","), args); err != nil {
		writeErr(w, err.Error())
		return
	}
	InsertLog("编辑工程", fmt.Sprintf("工程#%d %s", id, ProjectName(id)), strings.Join(logParts, ", "))
	writeJSON(w, map[string]interface{}{"ok": true})
}

func handleDashboard(w http.ResponseWriter, r *http.Request) {
	required := []string{}
	reqNames := map[string]string{}
	for _, t := range docCfg.Types {
		if t.Required {
			required = append(required, t.Code)
			reqNames[t.Code] = t.Name
		}
	}
	projects, _ := ListProjects(0, "", "")
	type pItem struct {
		domain.Project
		UnitName    string   `json:"unit_name"`
		DocCount    int      `json:"doc_count"`
		Missing     []string `json:"missing"`
		MissingCode []string `json:"missing_codes"`
	}
	var rows []pItem
	var tProj, tDone, tMiss int
	var tFund, tPaid float64
	for _, p := range projects {
		un := UnitName(p.UnitID)
		p.UnitName = un
		p.DocCount = CountDocs(p.ID)
		have := ProjectDocTypes(p.ID)
		var miss, missCode []string
		for _, c := range required {
			if !have[c] {
				miss = append(miss, reqNames[c])
				missCode = append(missCode, c)
			}
		}
		if miss == nil {
			miss = []string{}
		}
		if missCode == nil {
			missCode = []string{}
		}
		rows = append(rows, pItem{Project: p, UnitName: un, DocCount: p.DocCount, Missing: miss, MissingCode: missCode})
		tProj++
		if len(miss) == 0 {
			tDone++
		} else {
			tMiss++
		}
		if p.CentralFunding != nil {
			tFund += *p.CentralFunding
		}
		if p.TotalPaid != nil {
			tPaid += *p.TotalPaid
		}
	}
	writeJSON(w, map[string]interface{}{
		"totals": map[string]interface{}{
			"projects": tProj, "done": tDone, "with_missing": tMiss,
			"central_funding": tFund, "total_paid": tPaid,
		},
		"projects": rows,
	})
}

// ---- 文件结构树 ----
func handleProjectTree(w http.ResponseWriter, r *http.Request) {
	pid, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	proj, err := GetProject(pid)
	if err != nil || proj == nil {
		writeJSON(w, []interface{}{})
		return
	}
	root := filepath.Join(projectsDir, proj.Folder)
	type fNode struct {
		Name string `json:"name"`
		Size int64  `json:"size"`
		Ext  string `json:"ext"`
	}
	type dNode struct {
		Code     string  `json:"code"`
		Label    string  `json:"label"`
		Stage    string  `json:"stage"`
		Required bool    `json:"required"`
		Files    []fNode `json:"files"`
	}
	// 建立 code->docType 映射
	typeMap := map[string]domain.DocType{}
	for _, t := range docCfg.Types {
		typeMap[t.Code] = t
	}
	var nodes []dNode
	entries, _ := os.ReadDir(root)
	for _, e := range entries {
		if !e.IsDir() {
			continue
		}
		code := e.Name()
		dt, known := typeMap[code]
		label := code
		stage := ""
		required := false
		if known {
			label = dt.Name
			stage = dt.Stage
			required = dt.Required
		}
		dn := dNode{Code: code, Label: label, Stage: stage, Required: required}
		subEntries, _ := os.ReadDir(filepath.Join(root, code))
		for _, f := range subEntries {
			if f.IsDir() {
				continue
			}
			fi, _ := f.Info()
			ext := strings.ToLower(strings.TrimPrefix(filepath.Ext(f.Name()), "."))
			dn.Files = append(dn.Files, fNode{Name: f.Name(), Size: fi.Size(), Ext: ext})
		}
		nodes = append(nodes, dn)
	}
	if nodes == nil {
		nodes = []dNode{}
	}
	writeJSON(w, nodes)
}

// ---- 新建工程 ----
func handleCreateProject(w http.ResponseWriter, r *http.Request) {
	var body struct {
		Name    string `json:"name"`
		UnitID  int64  `json:"unit_id"`
		NewUnit string `json:"new_unit"`
		Level   string `json:"level"`
		Ptype   string `json:"ptype"`
		Status  string `json:"status"`
	}
	json.NewDecoder(r.Body).Decode(&body)
	pid, folder, err := CreateProjectWizard(CreateProjectInput{
		Name: body.Name, UnitID: body.UnitID, NewUnit: body.NewUnit,
		Level: body.Level, Ptype: body.Ptype, Status: body.Status,
	})
	if err != nil {
		writeErr(w, err.Error())
		return
	}
	InsertLog("新建工程", fmt.Sprintf("工程#%d %s", pid, body.Name), "通过添加项目向导")
	writeJSON(w, map[string]interface{}{"ok": true, "id": pid, "folder": folder})
}

package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"
)

func writeJSON(w http.ResponseWriter, v interface{}) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	json.NewEncoder(w).Encode(v)
}
func handleConfig(w http.ResponseWriter, r *http.Request) {
	writeJSON(w, map[string]interface{}{
		"doc_types": docCfg.Types,
		"workflow":  wfCfg.Stages,
	})
}
func handleUnits(w http.ResponseWriter, r *http.Request) {
	units, _ := ListUnits()
	out := []map[string]interface{}{}
	for _, u := range units {
		var pc, dc int
		var cf float64
		db.QueryRow("SELECT COUNT(*) FROM projects WHERE unit_id=?", u.ID).Scan(&pc)
		db.QueryRow("SELECT COUNT(*) FROM documents d JOIN projects p ON p.id=d.project_id WHERE p.unit_id=?", u.ID).Scan(&dc)
		db.QueryRow("SELECT COALESCE(SUM(central_funding),0) FROM projects WHERE unit_id=?", u.ID).Scan(&cf)
		out = append(out, map[string]interface{}{
			"id": u.ID, "name": u.Name, "level": u.Level, "category": u.Category,
			"sort": u.Sort, "project_count": pc, "doc_count": dc, "central_funding": cf,
		})
	}
	writeJSON(w, out)
}
func handleProjects(w http.ResponseWriter, r *http.Request) {
	unitID, _ := strconv.ParseInt(r.URL.Query().Get("unit_id"), 10, 64)
	status := r.URL.Query().Get("status")
	q := strings.TrimSpace(r.URL.Query().Get("q"))
	projects, err := ListProjects(unitID, status, q)
	if err != nil {
		http.Error(w, err.Error(), 500)
		return
	}
	out := []map[string]interface{}{}
	for _, p := range projects {
		var un string
		db.QueryRow("SELECT name FROM units WHERE id=?", p.UnitID).Scan(&un)
		p.UnitName = un
		p.DocCount = CountDocs(p.ID)
		d := analyzeProject(p.ID)
		miss, _ := d["missing_required"].([]string)
		// 扁平结构：工程字段 + 附带字段（与前端约定一致）
		pb, _ := json.Marshal(p)
		m := map[string]interface{}{}
		json.Unmarshal(pb, &m)
		m["unit_name"] = un
		m["doc_count"] = p.DocCount
		m["completeness"] = d["completeness"]
		m["missing_required"] = miss
		out = append(out, m)
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
		writeJSON(w, map[string]interface{}{"ok": false, "error": "无可更新字段"})
		return
	}
	args = append(args, id)
	if _, err := db.Exec("UPDATE projects SET "+strings.Join(sets, ",")+" WHERE id=?", args...); err != nil {
		writeJSON(w, map[string]interface{}{"ok": false, "error": err.Error()})
		return
	}
	var pname string
	db.QueryRow("SELECT name FROM projects WHERE id=?", id).Scan(&pname)
	InsertLog("编辑工程", fmt.Sprintf("工程#%d %s", id, pname), strings.Join(logParts, ", "))
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
		Project
		UnitName    string   `json:"unit_name"`
		DocCount    int      `json:"doc_count"`
		Missing     []string `json:"missing"`
		MissingCode []string `json:"missing_codes"`
	}
	var rows []pItem
	var tProj, tDone, tMiss int
	var tFund, tPaid float64
	for _, p := range projects {
		var un string
		db.QueryRow("SELECT name FROM units WHERE id=?", p.UnitID).Scan(&un)
		p.UnitName = un
		p.DocCount = CountDocs(p.ID)
		// 已有doc_type
		have := map[string]bool{}
		rs, _ := db.Query("SELECT DISTINCT doc_type FROM documents WHERE project_id=?", p.ID)
		for rs.Next() {
			var c string
			rs.Scan(&c)
			have[c] = true
		}
		rs.Close()
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
func handleFile(w http.ResponseWriter, r *http.Request) {
	id, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	var rel, orig, ext string
	err := db.QueryRow("SELECT file_path,orig_name,file_ext FROM documents WHERE id=?", id).Scan(&rel, &orig, &ext)
	if err != nil {
		http.NotFound(w, r)
		return
	}
	full := filepath.Join(projectsDir, filepath.FromSlash(rel))
	if _, err := os.Stat(full); err != nil {
		http.NotFound(w, r)
		return
	}
	inline := map[string]bool{"pdf": true, "jpg": true, "jpeg": true, "png": true, "gif": true, "bmp": true, "webp": true}
	if !inline[strings.ToLower(ext)] {
		w.Header().Set("Content-Disposition", "attachment; filename*=UTF-8''"+urlEncode(orig))
	}
	http.ServeFile(w, r, full)
}
func handleUpload(w http.ResponseWriter, r *http.Request) {
	if err := r.ParseMultipartForm(200 << 20); err != nil {
		writeJSON(w, map[string]interface{}{"ok": false, "error": err.Error()})
		return
	}
	pid, _ := strconv.ParseInt(r.FormValue("project_id"), 10, 64)
	docType := strings.TrimSpace(r.FormValue("doc_type"))
	if pid == 0 || docType == "" {
		writeJSON(w, map[string]interface{}{"ok": false, "error": "缺少 project_id 或 doc_type"})
		return
	}
	proj, err := GetProject(pid)
	if err != nil || proj == nil {
		writeJSON(w, map[string]interface{}{"ok": false, "error": "工程不存在"})
		return
	}
	tname := "其他"
	for _, t := range docCfg.Types {
		if t.Code == docType {
			tname = t.Name
			break
		}
	}
	destDir := filepath.Join(projectsDir, proj.Folder, docType)
	os.MkdirAll(destDir, 0755)
	uploaded := 0
	for _, fh := range r.MultipartForm.File["file"] {
		src, err := fh.Open()
		if err != nil {
			continue
		}
		fname := fh.Filename
		dst := filepath.Join(destDir, fname)
		if _, err := os.Stat(dst); err == nil {
			base := strings.TrimSuffix(fname, filepath.Ext(fname))
			dst = filepath.Join(destDir, base+"_"+time.Now().Format("150405")+filepath.Ext(fname))
		}
		out, err := os.Create(dst)
		if err != nil {
			src.Close()
			continue
		}
		io.Copy(out, src)
		out.Close()
		src.Close()
		rel, _ := filepath.Rel(projectsDir, dst)
		rel = filepath.ToSlash(rel)
		ext := strings.ToLower(strings.TrimPrefix(filepath.Ext(fname), "."))
		fi, _ := os.Stat(dst)
		var size int64
		if fi != nil {
			size = fi.Size()
		}
		db.Exec(`INSERT INTO documents(project_id,doc_type,doc_type_name,title,orig_name,file_path,file_ext,file_size,uploaded,source) VALUES(?,?,?,?,?,?,?,?,?,?)`,
			pid, docType, tname, cleanTitle(fname), fname, rel, ext, size, time.Now().Format("2006-01-02 15:04:05"), "upload")
		uploaded++
	}
	if uploaded > 0 {
		InsertLog("上传文档", fmt.Sprintf("工程#%d %s", pid, tname), fmt.Sprintf("%d个文件", uploaded))
	}
	writeJSON(w, map[string]interface{}{"ok": true, "count": uploaded})
}
func handleDeleteDoc(w http.ResponseWriter, r *http.Request) {
	id, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	var rel, orig, tname string
	var pid int64
	err := db.QueryRow("SELECT file_path,orig_name,doc_type_name,project_id FROM documents WHERE id=?", id).Scan(&rel, &orig, &tname, &pid)
	if err != nil {
		writeJSON(w, map[string]interface{}{"ok": false, "error": "文档不存在"})
		return
	}
	os.Remove(filepath.Join(projectsDir, filepath.FromSlash(rel)))
	db.Exec("DELETE FROM documents WHERE id=?", id)
	InsertLog("删除文档", fmt.Sprintf("工程#%d %s", pid, tname), orig)
	writeJSON(w, map[string]interface{}{"ok": true})
}
func handleImport(w http.ResponseWriter, r *http.Request) {
	stats, err := ImportAll(absBasicdata, false)
	if err != nil {
		writeJSON(w, map[string]interface{}{"ok": false, "error": err.Error()})
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
		logs = []LogEntry{}
	}
	writeJSON(w, logs)
}
func handleOCRScan(w http.ResponseWriter, r *http.Request) {
	pid, _ := strconv.ParseInt(r.URL.Query().Get("project_id"), 10, 64)
	if pid == 0 {
		writeJSON(w, map[string]interface{}{"ok": false, "error": "缺少 project_id"})
		return
	}
	fields, err := scanProjectContracts(pid)
	if err != nil {
		writeJSON(w, map[string]interface{}{"ok": false, "error": err.Error()})
		return
	}
	n := applyExtractedFields(pid, fields)
	var pname string
	db.QueryRow("SELECT name FROM projects WHERE id=?", pid).Scan(&pname)
	InsertLog("OCR识别", fmt.Sprintf("工程#%d %s", pid, pname), fmt.Sprintf("回填%d个字段", n))
	writeJSON(w, map[string]interface{}{"ok": true, "fields": fields, "applied": n})
}

// ---- 删除文物单位(连同工程一起放入回收站) ----
func handleDeleteUnit(w http.ResponseWriter, r *http.Request) {
	uid, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	var name string
	db.QueryRow("SELECT name FROM units WHERE id=?", uid).Scan(&name)
	if name == "" {
		writeJSON(w, map[string]interface{}{"ok": false, "error": "单位不存在"})
		return
	}
	// 软删除该单位下所有工程
	var pids []int64
	rows, _ := db.Query("SELECT id FROM projects WHERE unit_id=? AND COALESCE(deleted,0)=0", uid)
	for rows.Next() {
		var pid int64
		rows.Scan(&pid)
		pids = append(pids, pid)
	}
	rows.Close()
	for _, pid := range pids {
		MoveProjectToRecycle(pid) // 文件移入回收站(保留)
	}
	// 硬删除工程DB记录(文件已在回收站),解除FK约束
	db.Exec("DELETE FROM documents WHERE project_id IN (SELECT id FROM projects WHERE unit_id=?)", uid)
	db.Exec("DELETE FROM projects WHERE unit_id=?", uid)
	// 删除单位
	db.Exec("DELETE FROM units WHERE id=?", uid)
	projCount := len(pids)
	detail := ""
	if projCount > 0 {
		detail = fmt.Sprintf("同时删除%d个工程(已放入回收站)", projCount)
	}
	InsertLog("删除单位", name, detail)
	writeJSON(w, map[string]interface{}{"ok": true, "projects_deleted": projCount})
}

// ---- 删除某工程下某分类的全部文件 ----
func handleDeleteDocType(w http.ResponseWriter, r *http.Request) {
	pid, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	docType := r.PathValue("docType")
	if docType == "" {
		writeJSON(w, map[string]interface{}{"ok": false, "error": "缺少分类"})
		return
	}
	proj, err := GetProject(pid)
	if err != nil || proj == nil {
		writeJSON(w, map[string]interface{}{"ok": false, "error": "工程不存在"})
		return
	}
	// 删DB记录
	res, _ := db.Exec("DELETE FROM documents WHERE project_id=? AND doc_type=?", pid, docType)
	deleted, _ := res.RowsAffected()
	// 删磁盘文件夹
	folder := filepath.Join(projectsDir, proj.Folder, docType)
	os.RemoveAll(folder)
	pname := proj.Name
	InsertLog("删除分类", fmt.Sprintf("工程#%d %s / %s", pid, pname, docType), fmt.Sprintf("删除%d个文件", deleted))
	writeJSON(w, map[string]interface{}{"ok": true, "deleted": deleted})
}

// ---- 删除工程(软删除,放入回收站) ----
func handleDeleteProject(w http.ResponseWriter, r *http.Request) {
	pid, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	proj, err := GetProject(pid)
	if err != nil || proj == nil {
		writeJSON(w, map[string]interface{}{"ok": false, "error": "工程不存在"})
		return
	}
	if err := SoftDeleteProject(pid); err != nil {
		writeJSON(w, map[string]interface{}{"ok": false, "error": err.Error()})
		return
	}
	MoveProjectToRecycle(pid)
	InsertLog("删除工程", fmt.Sprintf("工程#%d %s", pid, proj.Name), "已放入回收站")
	writeJSON(w, map[string]interface{}{"ok": true})
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
	typeMap := map[string]DocType{}
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
	if strings.TrimSpace(body.Name) == "" {
		writeJSON(w, map[string]interface{}{"ok": false, "error": "工程名称不能为空"})
		return
	}
	unitID := body.UnitID
	if strings.TrimSpace(body.NewUnit) != "" {
		res, err := db.Exec("INSERT INTO units(name,level,sort) VALUES(?,?,?)", strings.TrimSpace(body.NewUnit), body.Level, 99)
		if err != nil {
			writeJSON(w, map[string]interface{}{"ok": false, "error": "创建单位失败: " + err.Error()})
			return
		}
		unitID, _ = res.LastInsertId()
	}
	if unitID == 0 {
		writeJSON(w, map[string]interface{}{"ok": false, "error": "请选择或新建文物单位"})
		return
	}
	status := body.Status
	if status == "" {
		status = "前期"
	}
	res, err := db.Exec(`INSERT INTO projects(unit_id,name,ptype,status,created) VALUES(?,?,?,?,?)`,
		unitID, body.Name, body.Ptype, status, time.Now().Format("2006-01-02 15:04:05"))
	if err != nil {
		writeJSON(w, map[string]interface{}{"ok": false, "error": err.Error()})
		return
	}
	pid, _ := res.LastInsertId()
	folder := fmt.Sprintf("P%04d", pid)
	db.Exec("UPDATE projects SET folder=? WHERE id=?", folder, pid)
	os.MkdirAll(filepath.Join(projectsDir, folder), 0755)
	InsertLog("新建工程", fmt.Sprintf("工程#%d %s", pid, body.Name), "通过添加项目向导")
	writeJSON(w, map[string]interface{}{"ok": true, "id": pid, "folder": folder})
}

package main

// 业务编排：工程聚合服务。合并原 analyze.go（缺项检测/资质校验）与 project_service.go
// （新建向导/列表富化/看板/字段更新/文件树）。依赖注入 ProjectRepository/UnitRepository/
// DocumentRepository 接口与 *Config，不再访问任何包级全局。

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"heritage-mgmt/internal/domain"
)

// ProjectService 编排工程相关的业务逻辑（依赖仓储接口 + 配置，可注入 mock）。
type ProjectService struct {
	projects ProjectRepository
	units    UnitRepository
	docs     DocumentRepository
	cfg      *Config
}

// CreateProjectInput 新建工程向导的输入
type CreateProjectInput struct {
	Name    string
	UnitID  int64
	NewUnit string
	Level   string
	Ptype   string
	Status  string
}

// Analyze 汇总单个工程的缺项检测、阶段聚合与资质校验，返回强类型结果（原 analyzeProject）。
func (svc *ProjectService) Analyze(pid int64) *domain.ProjectDetail {
	proj, err := svc.projects.GetProject(pid)
	if err != nil || proj == nil {
		return nil
	}
	docs, _ := svc.docs.ListDocuments(pid)
	counts := map[string]int{}
	for _, d := range docs {
		counts[d.DocType]++
	}
	// 类型状态
	var typeStatus []domain.TypeStatus
	for _, t := range svc.cfg.DocCfg.Types {
		n := counts[t.Code]
		typeStatus = append(typeStatus, domain.TypeStatus{
			Code: t.Code, Name: t.Name, Required: t.Required,
			Stage: t.Stage, Count: n, Has: n > 0,
		})
	}
	// 必备缺项
	var missingReq, missingOpt []string
	requiredTotal, requiredHave := 0, 0
	for _, ts := range typeStatus {
		if ts.Required {
			requiredTotal++
			if ts.Count == 0 {
				missingReq = append(missingReq, ts.Name)
			} else {
				requiredHave++
			}
		}
	}
	if proj.Status == "已竣工" {
		for _, ts := range typeStatus {
			if !ts.Required && ts.Count == 0 && ts.Code != "catalog" {
				missingOpt = append(missingOpt, ts.Name)
			}
		}
	}
	completeness := 100
	if requiredTotal > 0 {
		completeness = requiredHave * 100 / requiredTotal
	}
	// 阶段
	var stages []domain.StageOut
	tsByCode := map[string]domain.TypeStatus{}
	for _, ts := range typeStatus {
		tsByCode[ts.Code] = ts
	}
	for _, s := range svc.cfg.WfCfg.Stages {
		var stTypes []domain.TypeStatus
		var stDocs []domain.Document
		for _, d := range docs {
			for _, c := range s.Docs {
				if d.DocType == c {
					stDocs = append(stDocs, d)
					break
				}
			}
		}
		for _, c := range s.Docs {
			if ts, ok := tsByCode[c]; ok {
				stTypes = append(stTypes, ts)
			}
		}
		stages = append(stages, domain.StageOut{
			Code: s.Code, Name: s.Name, DocCount: len(stDocs),
			Types: stTypes, Documents: stDocs,
		})
	}
	// 规范化为非 nil 切片，保证 JSON 输出为 [] 而非 null
	if missingReq == nil {
		missingReq = []string{}
	}
	if missingOpt == nil {
		missingOpt = []string{}
	}
	qw := svc.qualWarnings(proj)
	if qw == nil {
		qw = []string{}
	}
	return &domain.ProjectDetail{
		Project:         proj,
		UnitLevel:       svc.units.UnitLevel(proj.UnitID),
		Documents:       docs,
		TypeStatus:      typeStatus,
		Stages:          stages,
		MissingRequired: missingReq,
		MissingOptional: missingOpt,
		Completeness:    completeness,
		QualWarnings:    qw,
	}
}

// qualRules: 各保护级别对 设计/施工/监理 资质的最低要求（""=不限）。
// TODO(Step 10): 迁移至 config/rules.json，实现配置驱动。
var qualRules = map[string]struct{ design, construction, supervision string }{
	"国保": {design: "甲级", construction: "一级", supervision: "甲级"},
}

func (svc *ProjectService) qualWarnings(p *domain.Project) []string {
	var warns []string
	level := svc.units.UnitLevel(p.UnitID)
	if level == "" {
		return warns
	}
	// 未配置的级别返回零值结构体（全 ""），即仅校验"未填写"，不校验资质等级阈值
	req := qualRules[level]
	checks := []struct{ label, qual, unit, req string }{
		{"设计单位资质", p.DesignQual, p.DesignUnit, req.design},
		{"施工单位资质", p.ConstructionQual, p.ConstructionUnit, req.construction},
		{"监理单位资质", p.SupervisionQual, p.SupervisionUnit, req.supervision},
	}
	for _, c := range checks {
		switch {
		case c.qual == "" && c.unit != "":
			warns = append(warns, c.label+"未填写")
		case c.qual != "" && c.req != "" && !strings.Contains(c.qual, c.req):
			warns = append(warns, c.label+"为「"+c.qual+"」，"+level+"工程建议"+c.req+"及以上")
		}
	}
	return warns
}

// List 返回工程列表，每项富化单位名/文档数/完整度/必备缺项（原 handleProjects 主体）。
func (svc *ProjectService) List(unitID int64, status, q string) ([]projectListItem, error) {
	projects, err := svc.projects.ListProjects(unitID, status, q)
	if err != nil {
		return nil, err
	}
	out := []projectListItem{}
	for _, p := range projects {
		item := projectListItem{
			Project:         p,
			UnitName:        svc.projects.ProjectName(p.UnitID),
			DocCount:        svc.docs.CountDocs(p.ID),
			MissingRequired: []string{},
		}
		if d := svc.Analyze(p.ID); d != nil {
			item.Completeness = d.Completeness
			item.MissingRequired = d.MissingRequired
		}
		out = append(out, item)
	}
	return out, nil
}

// ListUnits 返回文物单位列表，每项富化工程数/文档数/中央指标（原 handleUnits 主体）。
func (svc *ProjectService) ListUnits() ([]unitListItem, error) {
	units, err := svc.units.ListUnits()
	if err != nil {
		return nil, err
	}
	out := []unitListItem{}
	for _, u := range units {
		pc, dc, cf := svc.units.UnitStats(u.ID)
		out = append(out, unitListItem{Unit: u, ProjectCount: pc, DocCount: dc, CentralFunding: cf})
	}
	return out, nil
}

// dashboardRow 看板每行的响应结构（含缺项名与缺项 code）。
type dashboardRow struct {
	domain.Project
	UnitName    string   `json:"unit_name"`
	DocCount    int      `json:"doc_count"`
	Missing     []string `json:"missing"`
	MissingCode []string `json:"missing_codes"`
}

// Dashboard 聚合看板数据：汇总指标 + 每工程的必备缺项（原 handleDashboard 主体）。
func (svc *ProjectService) Dashboard() map[string]interface{} {
	required := []string{}
	reqNames := map[string]string{}
	for _, t := range svc.cfg.DocCfg.Types {
		if t.Required {
			required = append(required, t.Code)
			reqNames[t.Code] = t.Name
		}
	}
	projects, _ := svc.projects.ListProjects(0, "", "")
	var rows []dashboardRow
	var tProj, tDone, tMiss int
	var tFund, tPaid float64
	for _, p := range projects {
		un := svc.projects.ProjectName(p.UnitID)
		p.UnitName = un
		p.DocCount = svc.docs.CountDocs(p.ID)
		have := svc.projects.ProjectDocTypes(p.ID)
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
		rows = append(rows, dashboardRow{Project: p, UnitName: un, DocCount: p.DocCount, Missing: miss, MissingCode: missCode})
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
	return map[string]interface{}{
		"totals": map[string]interface{}{
			"projects": tProj, "done": tDone, "with_missing": tMiss,
			"central_funding": tFund, "total_paid": tPaid,
		},
		"projects": rows,
	}
}

// fileNode / dirNode 文件结构树的响应节点。
type fileNode struct {
	Name string `json:"name"`
	Size int64  `json:"size"`
	Ext  string `json:"ext"`
}
type dirNode struct {
	Code     string     `json:"code"`
	Label    string     `json:"label"`
	Stage    string     `json:"stage"`
	Required bool       `json:"required"`
	Files    []fileNode `json:"files"`
}

// Tree 构造工程归档目录的文件结构树（原 handleProjectTree 主体）。
func (svc *ProjectService) Tree(pid int64) interface{} {
	proj, err := svc.projects.GetProject(pid)
	if err != nil || proj == nil {
		return []interface{}{}
	}
	root := filepath.Join(svc.cfg.ProjectsDir, proj.Folder)
	typeMap := map[string]domain.DocType{}
	for _, t := range svc.cfg.DocCfg.Types {
		typeMap[t.Code] = t
	}
	var nodes []dirNode
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
		dn := dirNode{Code: code, Label: label, Stage: stage, Required: required}
		subEntries, _ := os.ReadDir(filepath.Join(root, code))
		for _, f := range subEntries {
			if f.IsDir() {
				continue
			}
			fi, _ := f.Info()
			ext := strings.ToLower(strings.TrimPrefix(filepath.Ext(f.Name()), "."))
			dn.Files = append(dn.Files, fileNode{Name: f.Name(), Size: fi.Size(), Ext: ext})
		}
		nodes = append(nodes, dn)
	}
	if nodes == nil {
		nodes = []dirNode{}
	}
	return nodes
}

// updateAllowed 允许通过编辑接口更新的字段白名单。
var updateAllowed = []string{"approval_no", "sign_date", "complete_date", "accept_date", "contract_end",
	"central_funding", "eng_contract", "eng_paid", "sup_contract", "sup_paid",
	"des_contract", "des_paid", "expert_fee", "total_paid", "status", "progress_note", "ptype",
	"construction_unit", "construction_qual", "design_unit", "design_qual",
	"supervision_unit", "supervision_qual", "owner_unit", "contract_no", "contract_sign_date", "name"}

// Update 按白名单字段更新工程，返回用于日志的字段变更描述（原 handleUpdateProject 主体）。
func (svc *ProjectService) Update(id int64, body map[string]interface{}) ([]string, error) {
	var sets, logParts []string
	var args []interface{}
	for _, k := range updateAllowed {
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
		return nil, fmt.Errorf("无可更新字段")
	}
	if err := svc.projects.UpdateProjectFields(id, strings.Join(sets, ","), args); err != nil {
		return nil, err
	}
	return logParts, nil
}

// CreateWizard 新建工程（可选先新建单位），创建归档目录，返回工程ID与目录名。
func (svc *ProjectService) CreateWizard(in CreateProjectInput) (int64, string, error) {
	name := strings.TrimSpace(in.Name)
	if name == "" {
		return 0, "", fmt.Errorf("工程名称不能为空")
	}
	unitID := in.UnitID
	if nu := strings.TrimSpace(in.NewUnit); nu != "" {
		id, err := svc.units.CreateUnit(nu, in.Level, 99)
		if err != nil {
			return 0, "", fmt.Errorf("创建单位失败: %v", err)
		}
		unitID = id
	}
	if unitID == 0 {
		return 0, "", fmt.Errorf("请选择或新建文物单位")
	}
	status := in.Status
	if status == "" {
		status = "前期"
	}
	pid, err := svc.projects.CreateProject(unitID, name, in.Ptype, status)
	if err != nil {
		return 0, "", err
	}
	folder := fmt.Sprintf("P%04d", pid)
	svc.projects.SetProjectFolder(pid, folder)
	os.MkdirAll(filepath.Join(svc.cfg.ProjectsDir, folder), 0755)
	return pid, folder, nil
}

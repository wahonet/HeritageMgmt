package main

import "strings"

type TypeStatus struct {
	Code     string `json:"code"`
	Name     string `json:"name"`
	Required bool   `json:"required"`
	Stage    string `json:"stage"`
	Count    int    `json:"count"`
	Has      bool   `json:"has"`
}
type StageOut struct {
	Code      string       `json:"code"`
	Name      string       `json:"name"`
	DocCount  int          `json:"doc_count"`
	Types     []TypeStatus `json:"types"`
	Documents []Document   `json:"documents"`
}

func analyzeProject(pid int64) map[string]interface{} {
	proj, err := GetProject(pid)
	if err != nil || proj == nil {
		return nil
	}
	docs, _ := ListDocuments(pid)
	counts := map[string]int{}
	for _, d := range docs {
		counts[d.DocType]++
	}
	// 类型状态
	var typeStatus []TypeStatus
	for _, t := range docCfg.Types {
		n := counts[t.Code]
		typeStatus = append(typeStatus, TypeStatus{
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
	var stages []StageOut
	tsByCode := map[string]TypeStatus{}
	for _, ts := range typeStatus {
		tsByCode[ts.Code] = ts
	}
	for _, s := range wfCfg.Stages {
		var stTypes []TypeStatus
		var stDocs []Document
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
		stages = append(stages, StageOut{
			Code: s.Code, Name: s.Name, DocCount: len(stDocs),
			Types: stTypes, Documents: stDocs,
		})
	}
	if missingReq == nil {
		missingReq = []string{}
	}
	if missingOpt == nil {
		missingOpt = []string{}
	}
	if missingReq == nil {
		missingReq = []string{}
	}
	if missingOpt == nil {
		missingOpt = []string{}
	}
	qw := qualWarnings(proj)
	if qw == nil {
		qw = []string{}
	}
	return map[string]interface{}{
		"project":          proj,
		"unit_level":       unitLevel(proj.UnitID),
		"documents":        docs,
		"type_status":      typeStatus,
		"stages":           stages,
		"missing_required": missingReq,
		"missing_optional": missingOpt,
		"completeness":     completeness,
		"qual_warnings":    qw,
	}
}
func qualWarnings(p *Project) []string {
	var warns []string
	level := unitLevel(p.UnitID)
	if level == "" {
		return warns
	}
	type chk struct {
		label, qual, unit, req string
	}
	var checks []chk
	if level == "国保" {
		checks = []chk{
			{"设计单位资质", p.DesignQual, p.DesignUnit, "甲级"},
			{"施工单位资质", p.ConstructionQual, p.ConstructionUnit, "一级"},
			{"监理单位资质", p.SupervisionQual, p.SupervisionUnit, "甲级"},
		}
	} else {
		checks = []chk{
			{"设计单位资质", p.DesignQual, p.DesignUnit, ""},
			{"施工单位资质", p.ConstructionQual, p.ConstructionUnit, ""},
			{"监理单位资质", p.SupervisionQual, p.SupervisionUnit, ""},
		}
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

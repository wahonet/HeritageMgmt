package main

import (
	"strings"

	"heritage-mgmt/internal/domain"
)

// TypeStatus/StageOut/ProjectDetail 已迁移至 internal/domain

// analyzeProject 汇总单个工程的缺项检测、阶段聚合与资质校验，返回强类型结果。
func analyzeProject(pid int64) *domain.ProjectDetail {
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
	var typeStatus []domain.TypeStatus
	for _, t := range docCfg.Types {
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
	for _, s := range wfCfg.Stages {
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
	qw := qualWarnings(proj)
	if qw == nil {
		qw = []string{}
	}
	return &domain.ProjectDetail{
		Project:         proj,
		UnitLevel:       unitLevel(proj.UnitID),
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

func qualWarnings(p *domain.Project) []string {
	var warns []string
	level := unitLevel(p.UnitID)
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

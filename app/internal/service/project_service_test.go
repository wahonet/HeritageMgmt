package service

// ProjectService 单测：用 fakeStore 注入，验证缺项检测/完整度/资质校验（无 DB/无磁盘）。

import (
	"strings"
	"testing"

	"heritage-mgmt/internal/config"
	"heritage-mgmt/internal/domain"
)

func testCfg() *config.Config {
	return &config.Config{
		DocCfg: domain.DocTypeCfg{
			Types: []domain.DocType{
				{Code: "approval", Name: "批复文", Required: true, Stage: "approve"},
				{Code: "funding", Name: "资金下达", Required: false, Stage: "funding"},
			},
		},
		WfCfg: domain.Workflow{Stages: []domain.Stage{
			{Code: "approve", Name: "方案审批", Docs: []string{"approval"}},
			{Code: "funding", Name: "资金下达", Docs: []string{"funding"}},
		}},
		Rules: config.Rules{QualThresholds: map[string]config.QualThreshold{
			"国保": {Design: "甲级", Construction: "一级", Supervision: "甲级"},
		}},
	}
}

// 必备项齐全 → 完整度 100%、无缺项
func TestAnalyzeComplete(t *testing.T) {
	fs := &fakeStore{
		proj:  &domain.Project{ID: 1, UnitID: 7, Status: "在建"},
		level: "省保",
		docs:  []domain.Document{{DocType: "approval"}},
	}
	svc := &ProjectService{projects: fs, units: fs, docs: fs, cfg: testCfg()}
	d := svc.Analyze(1)
	if d == nil {
		t.Fatal("Analyze 返回 nil")
	}
	if d.Completeness != 100 {
		t.Errorf("完整度 = %d, want 100", d.Completeness)
	}
	if len(d.MissingRequired) != 0 {
		t.Errorf("必备缺项 = %v, want 空", d.MissingRequired)
	}
}

// 必备项缺失 → 完整度 0、缺项含"批复文"
func TestAnalyzeMissingRequired(t *testing.T) {
	fs := &fakeStore{
		proj:  &domain.Project{ID: 1, UnitID: 7, Status: "在建"},
		level: "",
		docs:  nil,
	}
	svc := &ProjectService{projects: fs, units: fs, docs: fs, cfg: testCfg()}
	d := svc.Analyze(1)
	if d.Completeness != 0 {
		t.Errorf("完整度 = %d, want 0", d.Completeness)
	}
	found := false
	for _, m := range d.MissingRequired {
		if m == "批复文" {
			found = true
		}
	}
	if !found {
		t.Errorf("缺项应含'批复文', got %v", d.MissingRequired)
	}
}

// 工程不存在 → nil
func TestAnalyzeMissingProject(t *testing.T) {
	fs := &fakeStore{proj: nil}
	svc := &ProjectService{projects: fs, units: fs, docs: fs, cfg: testCfg()}
	if svc.Analyze(999) != nil {
		t.Error("工程不存在应返回 nil")
	}
}

// 资质校验：国保+设计乙级 应告警；甲级 不应触发等级告警（规则来自 config.Rules）
func TestQualWarningsThreshold(t *testing.T) {
	svc := &ProjectService{units: &fakeStore{level: "国保"}, cfg: testCfg()}
	w := svc.qualWarnings(&domain.Project{ID: 1, UnitID: 7, DesignUnit: "某设计院", DesignQual: "乙级"})
	if len(w) == 0 {
		t.Error("国保设计资质为'乙级'应触发等级告警")
	}
	w2 := svc.qualWarnings(&domain.Project{ID: 1, UnitID: 7, DesignUnit: "某设计院", DesignQual: "甲级"})
	for _, msg := range w2 {
		if strings.Contains(msg, "建议") {
			t.Errorf("甲级满足要求不应触发等级告警: %s", msg)
		}
	}
}

// 未填写资质（有单位无资质）应告警"未填写"
func TestQualWarningsMissing(t *testing.T) {
	svc := &ProjectService{units: &fakeStore{level: "省保"}, cfg: testCfg()}
	w := svc.qualWarnings(&domain.Project{ID: 1, UnitID: 7, DesignUnit: "某设计院", DesignQual: ""})
	if len(w) == 0 {
		t.Error("有设计单位但无资质应告警'未填写'")
	}
}

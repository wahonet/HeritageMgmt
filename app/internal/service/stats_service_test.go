package service

// StatsService 单测：用 fakeStore 注入，验证聚合统计（无 DB）。

import (
	"testing"

	"heritage-mgmt/internal/domain"
)

func TestStatsAggregate(t *testing.T) {
	cf := 100.0
	tp := 40.0
	fs := &fakeStore{
		projs: []domain.Project{
			{ID: 1, UnitID: 1, Ptype: "修缮工程", SignDate: "2021-05-01", Status: "在建", CentralFunding: &cf, TotalPaid: &tp},
			{ID: 2, UnitID: 1, Ptype: "安防工程", SignDate: "2022-06-01", Status: "已竣工", CentralFunding: &cf, TotalPaid: &tp},
		},
		units: []domain.Unit{{ID: 1, Name: "测试单位", Level: "省保"}},
	}
	svc := &StatsService{projects: fs, units: fs}
	r := svc.Aggregate()

	tot, ok := r["total"].(domain.StatGroup)
	if !ok {
		t.Fatalf("total 类型断言失败")
	}
	if tot.N != 2 {
		t.Errorf("total.N = %d, want 2", tot.N)
	}
	if tot.Funding != 200 {
		t.Errorf("total.Funding = %v, want 200", tot.Funding)
	}
	if tot.Paid != 80 {
		t.Errorf("total.Paid = %v, want 80", tot.Paid)
	}
	if tot.Pending != 120 {
		t.Errorf("total.Pending = %v, want 120", tot.Pending)
	}
	byType, ok := r["by_type"].([]domain.StatGroup)
	if !ok || len(byType) != 2 {
		t.Errorf("by_type 应有2个类型分组, got %v", r["by_type"])
	}
}

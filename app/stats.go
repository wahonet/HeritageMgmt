package main

import (
	"net/http"
	"sort"

	"heritage-mgmt/internal/domain"
)

// gs 统计聚合类型已迁移至 internal/domain.StatGroup

func handleStats(w http.ResponseWriter, r *http.Request) {
	projects, _ := ListProjects(0, "", "")
	uname := map[int64]string{}
	units, _ := ListUnits()
	for _, u := range units {
		uname[u.ID] = u.Name
	}
	fv := func(v *float64) float64 {
		if v != nil {
			return *v
		}
		return 0
	}
	add := func(m map[string]*domain.StatGroup, key string, p domain.Project) {
		g := m[key]
		if g == nil {
			g = &domain.StatGroup{K: key}
			m[key] = g
		}
		g.N++
		g.Funding += fv(p.CentralFunding)
		g.Paid += fv(p.TotalPaid)
		g.EngC += fv(p.EngContract)
		g.EngP += fv(p.EngPaid)
		g.SupC += fv(p.SupContract)
		g.SupP += fv(p.SupPaid)
		g.DesC += fv(p.DesContract)
		g.DesP += fv(p.DesPaid)
	}
	fin := func(g *domain.StatGroup) { g.Pending = g.Funding - g.Paid }
	toslice := func(m map[string]*domain.StatGroup) []domain.StatGroup {
		out := []domain.StatGroup{}
		for _, g := range m {
			fin(g)
			out = append(out, *g)
		}
		return out
	}
	byUnit := map[string]*domain.StatGroup{}
	byType := map[string]*domain.StatGroup{}
	byYear := map[string]*domain.StatGroup{}
	byStatus := map[string]*domain.StatGroup{}
	var tot domain.StatGroup
	for _, p := range projects {
		add(byUnit, uname[p.UnitID], p)
		tp := p.Ptype
		if tp == "" {
			tp = "其他"
		}
		add(byType, tp, p)
		yr := "未知"
		if len(p.SignDate) >= 4 {
			yr = p.SignDate[:4]
		}
		add(byYear, yr, p)
		st := p.Status
		if st == "" {
			st = "未定"
		}
		add(byStatus, st, p)
		tot.N++
		tot.Funding += fv(p.CentralFunding)
		tot.Paid += fv(p.TotalPaid)
		tot.EngC += fv(p.EngContract)
		tot.EngP += fv(p.EngPaid)
		tot.SupC += fv(p.SupContract)
		tot.SupP += fv(p.SupPaid)
		tot.DesC += fv(p.DesContract)
		tot.DesP += fv(p.DesPaid)
	}
	tot.Pending = tot.Funding - tot.Paid
	us := toslice(byUnit)
	sort.Slice(us, func(i, j int) bool { return us[i].Funding > us[j].Funding })
	ts := toslice(byType)
	sort.Slice(ts, func(i, j int) bool { return ts[i].Funding > ts[j].Funding })
	ys := toslice(byYear)
	sort.Slice(ys, func(i, j int) bool { return ys[i].K < ys[j].K })
	ss := toslice(byStatus)
	writeJSON(w, map[string]interface{}{
		"total":     tot,
		"by_unit":   us,
		"by_type":   ts,
		"by_year":   ys,
		"by_status": ss,
	})
}

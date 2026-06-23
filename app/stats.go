package main

import (
	"net/http"
	"sort"
)

type gs struct {
	K       string  `json:"k"`
	N       int     `json:"count"`
	Funding float64 `json:"funding"` // 中央指标
	Paid    float64 `json:"paid"`    // 总已支付
	Pending float64 `json:"pending"` // 总待支付=指标-已付
	EngC    float64 `json:"eng_contract"`
	EngP    float64 `json:"eng_paid"`
	SupC    float64 `json:"sup_contract"`
	SupP    float64 `json:"sup_paid"`
	DesC    float64 `json:"des_contract"`
	DesP    float64 `json:"des_paid"`
}

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
	add := func(m map[string]*gs, key string, p Project) {
		g := m[key]
		if g == nil {
			g = &gs{K: key}
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
	fin := func(g *gs) { g.Pending = g.Funding - g.Paid }
	toslice := func(m map[string]*gs) []gs {
		out := []gs{}
		for _, g := range m {
			fin(g)
			out = append(out, *g)
		}
		return out
	}
	byUnit := map[string]*gs{}
	byType := map[string]*gs{}
	byYear := map[string]*gs{}
	byStatus := map[string]*gs{}
	var tot gs
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

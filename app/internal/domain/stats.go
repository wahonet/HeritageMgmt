package domain

// StatGroup 统计聚合行（按单位/类型/年份/状态分组）。
// 字段与 JSON 标签保持与原 stats.go 的 gs 类型一致，API 输出不变。
type StatGroup struct {
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

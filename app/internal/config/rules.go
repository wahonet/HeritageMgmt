package config

// 业务规则：资质校验阈值 + 工程状态推导关键词（从后端硬编码迁出，由 rules.json 驱动）。

import "encoding/json"

// QualThreshold 某保护级别对 设计/施工/监理 资质的最低要求（""=不限）。
type QualThreshold struct {
	Design       string `json:"design"`
	Construction string `json:"construction"`
	Supervision  string `json:"supervision"`
}

// Rules 业务规则集合。
type Rules struct {
	QualThresholds map[string]QualThreshold `json:"qual_thresholds"`
	StatusKeywords map[string][]string      `json:"status_keywords"`
}

// defaultRules 返回与重构前硬编码一致的兜底规则（rules.json 缺失或非法时使用）。
func defaultRules() Rules {
	return Rules{
		QualThresholds: map[string]QualThreshold{
			"国保": {Design: "甲级", Construction: "一级", Supervision: "甲级"},
		},
		StatusKeywords: map[string][]string{
			"已竣工": {"验收", "竣工", "完工"},
			"在建":   {"在建", "施工"},
		},
	}
}

// LoadRules 从磁盘/内嵌读取 rules.json；缺失或非法回退默认（保证行为不退化）。
func LoadRules(appBase string) Rules {
	d := defaultRules()
	b, err := ReadConfigFile(appBase, "rules.json")
	if err != nil {
		return d
	}
	var r Rules
	if json.Unmarshal(b, &r) != nil {
		return d
	}
	if r.QualThresholds == nil {
		r.QualThresholds = d.QualThresholds
	}
	if r.StatusKeywords == nil {
		r.StatusKeywords = d.StatusKeywords
	}
	return r
}

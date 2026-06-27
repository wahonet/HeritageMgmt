package domain

// TypeStatus 某文档类型在工程中的存在状态（缺项检测用）
type TypeStatus struct {
	Code     string `json:"code"`
	Name     string `json:"name"`
	Required bool   `json:"required"`
	Stage    string `json:"stage"`
	Count    int    `json:"count"`
	Has      bool   `json:"has"`
}

// StageOut 工作流阶段的聚合输出（含该阶段类型与文档）
type StageOut struct {
	Code      string       `json:"code"`
	Name      string       `json:"name"`
	DocCount  int          `json:"doc_count"`
	Types     []TypeStatus `json:"types"`
	Documents []Document   `json:"documents"`
}

// ProjectDetail 单个工程的完整分析结果（取代原 map[string]interface{}，
// JSON 字段与原约定一致，前端无需改动）。
type ProjectDetail struct {
	Project         *Project     `json:"project"`
	UnitLevel       string       `json:"unit_level"`
	Documents       []Document   `json:"documents"`
	TypeStatus      []TypeStatus `json:"type_status"`
	Stages          []StageOut   `json:"stages"`
	MissingRequired []string     `json:"missing_required"`
	MissingOptional []string     `json:"missing_optional"`
	Completeness    int          `json:"completeness"`
	QualWarnings    []string     `json:"qual_warnings"`
}

package domain

// DocType 文档分类定义（来自 config/doc_types.json）
type DocType struct {
	Code     string   `json:"code"`
	Name     string   `json:"name"`
	Keywords []string `json:"keywords"`
	Stage    string   `json:"stage"`
	Required bool     `json:"required"`
	Desc     string   `json:"desc"`
}

// DocTypeCfg 文档分类配置集合
type DocTypeCfg struct {
	Types       []DocType `json:"types"`
	UnknownCode string    `json:"unknown_code"`
	UnknownName string    `json:"unknown_name"`
}

// Stage 工作流阶段
type Stage struct {
	Code string   `json:"code"`
	Name string   `json:"name"`
	Docs []string `json:"docs"`
}

// UnitRule 文物单位识别规则
type UnitRule struct {
	Unit     string   `json:"unit"`
	Level    string   `json:"level"`
	Category string   `json:"category"`
	Keywords []string `json:"keywords"`
}

// TypeRule 工程类型识别规则
type TypeRule struct {
	Type     string   `json:"type"`
	Keywords []string `json:"keywords"`
}

// Workflow 工作流配置（来自 config/workflow.json）
type Workflow struct {
	Stages []Stage `json:"stages"`
	Units  struct {
		Rules []UnitRule `json:"rules"`
	} `json:"units"`
	ProjectTypes struct {
		Rules []TypeRule `json:"rules"`
	} `json:"project_types"`
}

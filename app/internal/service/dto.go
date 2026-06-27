package service

// HTTP 层数据传输对象（DTO）：以强类型 struct 取代手拼 map / marshal→unmarshal 扁平化。

import "heritage-mgmt/internal/domain"

// projectListItem 是 GET /api/projects 的列表项：工程全字段 + 列表附带字段。
// 取代原先 json.Marshal→Unmarshal 的扁平化 hack（外层标签覆盖内嵌的 omitempty 同名字段）。
type projectListItem struct {
	domain.Project
	UnitName        string   `json:"unit_name"`
	DocCount        int      `json:"doc_count"`
	Completeness    int      `json:"completeness"`
	MissingRequired []string `json:"missing_required"`
}

// unitListItem 是 GET /api/units 的列表项：单位字段 + 统计附带字段。
type unitListItem struct {
	domain.Unit
	ProjectCount   int     `json:"project_count"`
	DocCount       int     `json:"doc_count"`
	CentralFunding float64 `json:"central_funding"`
}

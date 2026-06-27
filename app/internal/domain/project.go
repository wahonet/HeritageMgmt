// Package domain holds the core data types shared across all layers.
// It has zero dependencies on other internal packages — every other
// layer (store, service, httpapi, adapters) may import it freely.
package domain

// Unit 文物保护单位
type Unit struct {
	ID       int64  `json:"id"`
	Name     string `json:"name"`
	Level    string `json:"level"`
	Category string `json:"category"`
	Sort     int    `json:"sort"`
}

// Project 工程（含财务字段、参建单位资质、软删除标记等）
type Project struct {
	ID               int64    `json:"id"`
	UnitID           int64    `json:"unit_id"`
	Seq              *int64   `json:"seq"`
	Name             string   `json:"name"`
	Ptype            string   `json:"ptype"`
	ApprovalNo       string   `json:"approval_no"`
	SignDate         string   `json:"sign_date"`
	CompleteDate     string   `json:"complete_date"`
	AcceptDate       string   `json:"accept_date"`
	ContractEnd      string   `json:"contract_end"`
	OwnerUnit        string   `json:"owner_unit"`
	ContractNo       string   `json:"contract_no"`
	ContractSignDate string   `json:"contract_sign_date"`
	CentralFunding   *float64 `json:"central_funding"`
	EngContract      *float64 `json:"eng_contract"`
	EngPaid          *float64 `json:"eng_paid"`
	SupContract      *float64 `json:"sup_contract"`
	SupPaid          *float64 `json:"sup_paid"`
	DesContract      *float64 `json:"des_contract"`
	DesPaid          *float64 `json:"des_paid"`
	ExpertFee        *float64 `json:"expert_fee"`
	TotalPaid        *float64 `json:"total_paid"`
	Status           string   `json:"status"`
	ProgressNote     string   `json:"progress_note"`
	SourceDir        string   `json:"source_dir"`
	Folder           string   `json:"folder"`
	Created          string   `json:"created"`
	// 参建单位与资质
	ConstructionUnit string `json:"construction_unit"`
	ConstructionQual string `json:"construction_qual"`
	DesignUnit       string `json:"design_unit"`
	DesignQual       string `json:"design_qual"`
	SupervisionUnit  string `json:"supervision_unit"`
	SupervisionQual  string `json:"supervision_qual"`
	// 附带展示字段
	UnitName string `json:"unit_name,omitempty"`
	DocCount int    `json:"doc_count,omitempty"`
}

// Document 文档（归档文件）
type Document struct {
	ID          int64  `json:"id"`
	ProjectID   int64  `json:"project_id"`
	DocType     string `json:"doc_type"`
	DocTypeName string `json:"doc_type_name"`
	Title       string `json:"title"`
	OrigName    string `json:"orig_name"`
	FilePath    string `json:"file_path"`
	FileExt     string `json:"file_ext"`
	FileSize    int64  `json:"file_size"`
	Uploaded    string `json:"uploaded"`
	Source      string `json:"source"`
}

// LogEntry 操作日志条目
type LogEntry struct {
	ID     int64  `json:"id"`
	Ts     string `json:"ts"`
	Action string `json:"action"`
	Target string `json:"target"`
	Detail string `json:"detail"`
}

// RecycledProject 回收站中的工程（GET /api/recycle 列表项）
type RecycledProject struct {
	ID       int64  `json:"id"`
	Name     string `json:"name"`
	Folder   string `json:"folder"`
	Ptype    string `json:"ptype"`
	Status   string `json:"status"`
	UnitName string `json:"unit_name"`
}

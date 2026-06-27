package service

// 仓储接口（Repository Ports）——遵循接口隔离（ISP），定义在 service 消费侧。
// service 依赖这些接口而非具体 *sql.DB，从而可注入内存替身做无 IO 单测。
// 实现见各 *_repo.go（均为 *Store 方法），Store 同时满足全部四个接口。

import "heritage-mgmt/internal/domain"

// ProjectRepository 工程聚合的仓储端口。
type ProjectRepository interface {
	GetProject(id int64) (*domain.Project, error)
	ListProjects(unitID int64, status, q string) ([]domain.Project, error)
	ProjectName(id int64) string
	ProjectDocTypes(pid int64) map[string]bool
	UpdateProjectFields(id int64, sets string, vals []interface{}) error
	CreateProject(unitID int64, name, ptype, status string) (int64, error)
	SetProjectFolder(id int64, folder string) error
	RestoreProjectRecord(id int64) error
	PurgeProjectRecord(id int64) error
	ProjectIDsByUnit(uid int64) []int64
	ListRecycled() ([]domain.RecycledProject, error)
	SoftDeleteProject(id int64) error
	CountProjects() int
}

// UnitRepository 文物单位聚合的仓储端口。
type UnitRepository interface {
	ListUnits() ([]domain.Unit, error)
	UnitLevel(unitID int64) string
	UnitName(unitID int64) string
	UnitStats(unitID int64) (projCount, docCount int, funding float64)
	CreateUnit(name, level string, sort int) (int64, error)
	DeleteUnitRecords(uid int64)
}

// DocumentRepository 文档聚合的仓储端口。
type DocumentRepository interface {
	ListDocuments(projectID int64) ([]domain.Document, error)
	CountDocs(projectID int64) int
	DocByID(id int64) (*domain.Document, error)
	DeleteDocument(id int64)
	InsertDocument(d domain.Document) error
	DeleteDocsByType(pid int64, docType string) (int64, error)
}

// LogRepository 操作日志的仓储端口。
type LogRepository interface {
	InsertLog(action, target, detail string)
	ListLogs(limit int) ([]domain.LogEntry, error)
}

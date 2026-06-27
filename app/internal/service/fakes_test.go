package service

// 测试用内存仓储：同时实现 ProjectRepository/UnitRepository/DocumentRepository/LogRepository。
// 证明步骤7的 DI 架构可注入替身做无 IO 单测（重构前无法实现）。

import "heritage-mgmt/internal/domain"

type fakeStore struct {
	proj  *domain.Project
	projs []domain.Project
	docs  []domain.Document
	units []domain.Unit
	level string // UnitLevel 返回值
	logs  []domain.LogEntry
}

// ---- ProjectRepository ----
func (f *fakeStore) GetProject(int64) (*domain.Project, error)            { return f.proj, nil }
func (f *fakeStore) ListProjects(int64, string, string) ([]domain.Project, error) { return f.projs, nil }
func (f *fakeStore) ProjectName(int64) string                             { return "" }
func (f *fakeStore) ProjectDocTypes(int64) map[string]bool                { return nil }
func (f *fakeStore) UpdateProjectFields(int64, string, []interface{}) error { return nil }
func (f *fakeStore) CreateProject(int64, string, string, string) (int64, error) { return 0, nil }
func (f *fakeStore) SetProjectFolder(int64, string) error                 { return nil }
func (f *fakeStore) RestoreProjectRecord(int64) error                     { return nil }
func (f *fakeStore) PurgeProjectRecord(int64) error                       { return nil }
func (f *fakeStore) ProjectIDsByUnit(int64) []int64                       { return nil }
func (f *fakeStore) ListRecycled() ([]domain.RecycledProject, error)      { return nil, nil }
func (f *fakeStore) SoftDeleteProject(int64) error                        { return nil }
func (f *fakeStore) CountProjects() int                                   { return len(f.projs) }

// ---- UnitRepository ----
func (f *fakeStore) ListUnits() ([]domain.Unit, error)      { return f.units, nil }
func (f *fakeStore) UnitLevel(int64) string                 { return f.level }
func (f *fakeStore) UnitName(int64) string                  { return "" }
func (f *fakeStore) UnitStats(int64) (int, int, float64)    { return 0, 0, 0 }
func (f *fakeStore) CreateUnit(string, string, int) (int64, error) { return 0, nil }
func (f *fakeStore) DeleteUnitRecords(int64)                {}

// ---- DocumentRepository ----
func (f *fakeStore) ListDocuments(int64) ([]domain.Document, error) { return f.docs, nil }
func (f *fakeStore) CountDocs(int64) int                            { return len(f.docs) }
func (f *fakeStore) DocByID(int64) (*domain.Document, error)        { return nil, nil }
func (f *fakeStore) DeleteDocument(int64)                           {}
func (f *fakeStore) InsertDocument(domain.Document) error           { return nil }
func (f *fakeStore) DeleteDocsByType(int64, string) (int64, error)  { return 0, nil }

// ---- LogRepository ----
func (f *fakeStore) InsertLog(string, string, string)               {}
func (f *fakeStore) ListLogs(int) ([]domain.LogEntry, error)        { return f.logs, nil }

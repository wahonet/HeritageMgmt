package service

// 业务编排：工程/单位的删除、回收站恢复与彻底删除（协调 DB 记录与磁盘文件）。
// RecycleService 依赖注入 ProjectRepository/UnitRepository 接口与 *config.Config，不再访问包级全局。

import (
	"os"
	"path/filepath"

	"heritage-mgmt/internal/config"
	"heritage-mgmt/internal/domain"
)

// RecycleService 编排删除/回收站生命周期。
type RecycleService struct {
	projects ProjectRepository
	units    UnitRepository
	cfg      *config.Config
}

// recycleFolderToBin 将工程归档目录移入回收站(保留文件)；目录不存在则忽略。
func (svc *RecycleService) recycleFolderToBin(folder string) {
	if folder == "" {
		return
	}
	src := filepath.Join(svc.cfg.ProjectsDir, folder)
	if _, err := os.Stat(src); err != nil {
		return
	}
	bin := filepath.Join(svc.cfg.DataDir, "recycle")
	os.MkdirAll(bin, 0755)
	os.Rename(src, filepath.Join(bin, folder))
}

// RecycleProject 软删除工程并将其文件移入回收站。
func (svc *RecycleService) RecycleProject(proj *domain.Project) error {
	if err := svc.projects.SoftDeleteProject(proj.ID); err != nil {
		return err
	}
	svc.recycleFolderToBin(proj.Folder)
	return nil
}

// RestoreProject 从回收站恢复工程：取消软删除标记并把文件搬回。
func (svc *RecycleService) RestoreProject(proj *domain.Project) {
	svc.projects.RestoreProjectRecord(proj.ID)
	recyclePath := filepath.Join(svc.cfg.DataDir, "recycle", proj.Folder)
	if _, err := os.Stat(recyclePath); err == nil {
		os.Rename(recyclePath, filepath.Join(svc.cfg.ProjectsDir, proj.Folder))
	}
}

// PurgeProject 彻底删除工程：删除 DB 记录与回收站文件。
func (svc *RecycleService) PurgeProject(proj *domain.Project) {
	svc.projects.PurgeProjectRecord(proj.ID)
	os.RemoveAll(filepath.Join(svc.cfg.DataDir, "recycle", proj.Folder))
}

// DeleteUnitCascade 删除单位：其下工程文件移入回收站，再删除单位与工程/文档的 DB 记录。
// 返回受影响的工程数。
func (svc *RecycleService) DeleteUnitCascade(uid int64) int {
	pids := svc.projects.ProjectIDsByUnit(uid)
	for _, pid := range pids {
		if proj, _ := svc.projects.GetProject(pid); proj != nil {
			svc.recycleFolderToBin(proj.Folder)
		}
	}
	svc.units.DeleteUnitRecords(uid)
	return len(pids)
}

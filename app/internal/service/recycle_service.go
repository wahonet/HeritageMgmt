package service

// 业务编排：工程/单位的删除、回收站恢复与彻底删除（协调 DB 记录与磁盘文件）。
// RecycleService 依赖注入 ProjectRepository/UnitRepository 接口与 *config.Config，不再访问包级全局。
// 文件移动失败必须返回错误，避免出现"DB 已删但文件仍在原位/回收站"的不一致。

import (
	"fmt"
	"os"
	"path/filepath"
	"time"

	"heritage-mgmt/internal/config"
	"heritage-mgmt/internal/domain"
)

// RecycleService 编排删除/回收站生命周期。
type RecycleService struct {
	projects ProjectRepository
	units    UnitRepository
	cfg      *config.Config
}

// recycleBin 回收站根目录。
func (svc *RecycleService) recycleBin() string {
	return filepath.Join(svc.cfg.DataDir, "recycle")
}

// recycleFolderToBin 将工程归档目录移入回收站(保留文件)；源目录不存在视为已移走，非错误。
func (svc *RecycleService) recycleFolderToBin(folder string) error {
	if folder == "" {
		return nil
	}
	src := filepath.Join(svc.cfg.ProjectsDir, folder)
	if _, err := os.Stat(src); err != nil {
		return nil
	}
	bin := svc.recycleBin()
	if err := os.MkdirAll(bin, 0755); err != nil {
		return err
	}
	dst := filepath.Join(bin, folder)
	// 回收站内若已存在同名（多次删除同目录），追加时间戳避免覆盖。
	if _, err := os.Stat(dst); err == nil {
		dst = filepath.Join(bin, fmt.Sprintf("%s_%s", folder, time.Now().Format("20060102_150405")))
	}
	if err := os.Rename(src, dst); err != nil {
		return fmt.Errorf("移入回收站失败: %w", err)
	}
	return nil
}

// RecycleProject 软删除工程并将其文件移入回收站。先移文件再标记软删，保证 DB 与磁盘一致。
func (svc *RecycleService) RecycleProject(proj *domain.Project) error {
	if err := svc.recycleFolderToBin(proj.Folder); err != nil {
		return err
	}
	return svc.projects.SoftDeleteProject(proj.ID)
}

// RestoreProject 从回收站恢复工程：把文件搬回并取消软删除标记。
func (svc *RecycleService) RestoreProject(proj *domain.Project) error {
	recyclePath := filepath.Join(svc.recycleBin(), proj.Folder)
	if _, err := os.Stat(recyclePath); err == nil {
		if err := os.Rename(recyclePath, filepath.Join(svc.cfg.ProjectsDir, proj.Folder)); err != nil {
			return fmt.Errorf("从回收站搬回失败: %w", err)
		}
	}
	return svc.projects.RestoreProjectRecord(proj.ID)
}

// PurgeProject 彻底删除工程：删除回收站文件与 DB 记录。
func (svc *RecycleService) PurgeProject(proj *domain.Project) error {
	if err := os.RemoveAll(filepath.Join(svc.recycleBin(), proj.Folder)); err != nil {
		return fmt.Errorf("删除回收站文件失败: %w", err)
	}
	return svc.projects.PurgeProjectRecord(proj.ID)
}

// DeleteUnitCascade 删除单位：其下工程逐一软删入回收站（可恢复），再删除单位记录本身。
// 工程恢复时单位已不在，UI 显示"单位已删除"。返回受影响的工程数。
func (svc *RecycleService) DeleteUnitCascade(uid int64) (int, error) {
	pids := svc.projects.ProjectIDsByUnit(uid)
	for _, pid := range pids {
		proj, err := svc.projects.GetProject(pid)
		if err != nil || proj == nil {
			continue
		}
		if err := svc.RecycleProject(proj); err != nil {
			return 0, err
		}
	}
	if err := svc.units.DeleteUnit(uid); err != nil {
		return 0, err
	}
	return len(pids), nil
}

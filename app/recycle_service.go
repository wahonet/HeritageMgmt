package main

// 业务编排：工程/单位的删除、回收站恢复与彻底删除（协调 DB 记录与磁盘文件）。

import (
	"os"
	"path/filepath"

	"heritage-mgmt/internal/domain"
)

// recycleFolderToBin 将工程归档目录移入回收站(保留文件)；目录不存在则忽略。
func recycleFolderToBin(folder string) {
	if folder == "" {
		return
	}
	src := filepath.Join(projectsDir, folder)
	if _, err := os.Stat(src); err != nil {
		return
	}
	bin := filepath.Join(dataDir, "recycle")
	os.MkdirAll(bin, 0755)
	os.Rename(src, filepath.Join(bin, folder))
}

// RecycleProject 软删除工程并将其文件移入回收站。
func RecycleProject(proj *domain.Project) error {
	if err := SoftDeleteProject(proj.ID); err != nil {
		return err
	}
	recycleFolderToBin(proj.Folder)
	return nil
}

// RestoreProject 从回收站恢复工程：取消软删除标记并把文件搬回。
func RestoreProject(proj *domain.Project) {
	RestoreProjectRecord(proj.ID)
	recyclePath := filepath.Join(dataDir, "recycle", proj.Folder)
	if _, err := os.Stat(recyclePath); err == nil {
		os.Rename(recyclePath, filepath.Join(projectsDir, proj.Folder))
	}
}

// PurgeProject 彻底删除工程：删除 DB 记录与回收站文件。
func PurgeProject(proj *domain.Project) {
	PurgeProjectRecord(proj.ID)
	os.RemoveAll(filepath.Join(dataDir, "recycle", proj.Folder))
}

// DeleteUnitCascade 删除单位：其下工程文件移入回收站，再删除单位与工程/文档的 DB 记录。
// 返回受影响的工程数。
func DeleteUnitCascade(uid int64) int {
	pids := ProjectIDsByUnit(uid)
	for _, pid := range pids {
		if proj, _ := GetProject(pid); proj != nil {
			recycleFolderToBin(proj.Folder)
		}
	}
	DeleteUnitRecords(uid)
	return len(pids)
}

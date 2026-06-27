package main

// 业务编排：新建工程（可选新建单位）并建立归档目录。

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

// CreateProjectInput 新建工程向导的输入
type CreateProjectInput struct {
	Name    string
	UnitID  int64
	NewUnit string
	Level   string
	Ptype   string
	Status  string
}

// CreateProjectWizard 新建工程（可选先新建单位），创建归档目录，返回工程ID与目录名。
func CreateProjectWizard(in CreateProjectInput) (int64, string, error) {
	name := strings.TrimSpace(in.Name)
	if name == "" {
		return 0, "", fmt.Errorf("工程名称不能为空")
	}
	unitID := in.UnitID
	if nu := strings.TrimSpace(in.NewUnit); nu != "" {
		id, err := CreateUnit(nu, in.Level, 99)
		if err != nil {
			return 0, "", fmt.Errorf("创建单位失败: %v", err)
		}
		unitID = id
	}
	if unitID == 0 {
		return 0, "", fmt.Errorf("请选择或新建文物单位")
	}
	status := in.Status
	if status == "" {
		status = "前期"
	}
	pid, err := CreateProject(unitID, name, in.Ptype, status)
	if err != nil {
		return 0, "", err
	}
	folder := fmt.Sprintf("P%04d", pid)
	SetProjectFolder(pid, folder)
	os.MkdirAll(filepath.Join(projectsDir, folder), 0755)
	return pid, folder, nil
}

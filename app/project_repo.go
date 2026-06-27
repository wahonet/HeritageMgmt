package main

// 数据层：工程(projects)相关查询与维护（均为 *Store 方法，满足 ProjectRepository）。

import (
	"database/sql"
	"time"

	"heritage-mgmt/internal/domain"
)

func (s *Store) GetProject(id int64) (*domain.Project, error) {
	row := s.db.QueryRow("SELECT "+projectCols+" FROM projects WHERE id=?", id)
	return scanProject(row)
}

func (s *Store) ListProjects(unitID int64, status, q string) ([]domain.Project, error) {
	sqlStr := "SELECT " + projectCols + " FROM projects WHERE COALESCE(deleted,0)=0"
	var args []interface{}
	if unitID > 0 {
		sqlStr += " AND unit_id=?"
		args = append(args, unitID)
	}
	if status != "" {
		sqlStr += " AND status=?"
		args = append(args, status)
	}
	if q != "" {
		sqlStr += " AND (name LIKE ? OR approval_no LIKE ?)"
		args = append(args, "%"+q+"%", "%"+q+"%")
	}
	sqlStr += " ORDER BY unit_id,seq,id"
	rows, err := s.db.Query(sqlStr, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []domain.Project
	for rows.Next() {
		p, err := scanProject(rows)
		if err != nil {
			return nil, err
		}
		out = append(out, *p)
	}
	return out, nil
}

func (s *Store) ProjectName(id int64) string {
	var name string
	s.db.QueryRow("SELECT name FROM projects WHERE id=?", id).Scan(&name)
	return name
}

// CountProjects 工程总数（供启动时判断是否需要自动导入）。
func (s *Store) CountProjects() int {
	var n int
	s.db.QueryRow("SELECT COUNT(*) FROM projects").Scan(&n)
	return n
}

// ProjectDocTypes 查某工程已有的文档类型集合
func (s *Store) ProjectDocTypes(pid int64) map[string]bool {
	m := map[string]bool{}
	rows, _ := s.db.Query("SELECT DISTINCT doc_type FROM documents WHERE project_id=?", pid)
	if rows != nil {
		defer rows.Close()
		for rows.Next() {
			var c string
			rows.Scan(&c)
			m[c] = true
		}
	}
	return m
}

// UpdateProjectFields 按字段更新工程。sets 为 "a=?,b=?"，vals 为对应值（不含 id，函数内追加）。
func (s *Store) UpdateProjectFields(id int64, sets string, vals []interface{}) error {
	vals = append(vals, id)
	_, err := s.db.Exec("UPDATE projects SET "+sets+" WHERE id=?", vals...)
	return err
}

// CreateProject 新建工程，返回新ID
func (s *Store) CreateProject(unitID int64, name, ptype, status string) (int64, error) {
	res, err := s.db.Exec(`INSERT INTO projects(unit_id,name,ptype,status,created) VALUES(?,?,?,?,?)`,
		unitID, name, ptype, status, time.Now().Format("2006-01-02 15:04:05"))
	if err != nil {
		return 0, err
	}
	return res.LastInsertId()
}

// SetProjectFolder 设置工程归档目录名
func (s *Store) SetProjectFolder(id int64, folder string) error {
	_, err := s.db.Exec("UPDATE projects SET folder=? WHERE id=?", folder, id)
	return err
}

// RestoreProjectRecord 取消软删除标记（从回收站恢复）
func (s *Store) RestoreProjectRecord(id int64) error {
	_, err := s.db.Exec("UPDATE projects SET deleted=0 WHERE id=?", id)
	return err
}

// PurgeProjectRecord 彻底删除工程及其文档记录
func (s *Store) PurgeProjectRecord(id int64) error {
	if _, err := s.db.Exec("DELETE FROM documents WHERE project_id=?", id); err != nil {
		return err
	}
	_, err := s.db.Exec("DELETE FROM projects WHERE id=?", id)
	return err
}

// ProjectIDsByUnit 返回某单位下未删除的工程ID列表
func (s *Store) ProjectIDsByUnit(uid int64) []int64 {
	var pids []int64
	rows, _ := s.db.Query("SELECT id FROM projects WHERE unit_id=? AND COALESCE(deleted,0)=0", uid)
	if rows != nil {
		defer rows.Close()
		for rows.Next() {
			var pid int64
			rows.Scan(&pid)
			pids = append(pids, pid)
		}
	}
	return pids
}

// ListRecycled 列出回收站(已软删除)的工程
func (s *Store) ListRecycled() ([]domain.RecycledProject, error) {
	rows, err := s.db.Query(`SELECT p.id,p.name,p.folder,p.ptype,p.status,u.name FROM projects p LEFT JOIN units u ON u.id=p.unit_id WHERE COALESCE(p.deleted,0)=1 ORDER BY p.id DESC`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var items []domain.RecycledProject
	for rows.Next() {
		var it domain.RecycledProject
		var folder, ptype, status, uname sql.NullString
		rows.Scan(&it.ID, &it.Name, &folder, &ptype, &status, &uname)
		it.Folder = folder.String
		it.Ptype = ptype.String
		it.Status = status.String
		it.UnitName = uname.String
		if it.UnitName == "" {
			it.UnitName = "(单位已删除)"
		}
		items = append(items, it)
	}
	return items, nil
}

// SoftDeleteProject 软删除工程(放入回收站)
func (s *Store) SoftDeleteProject(id int64) error {
	_, err := s.db.Exec("UPDATE projects SET deleted=1 WHERE id=?", id)
	return err
}

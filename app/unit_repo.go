package main

// 数据层：文物单位(units)相关查询（均为 *Store 方法，满足 UnitRepository）。

import "heritage-mgmt/internal/domain"

func (s *Store) ListUnits() ([]domain.Unit, error) {
	rows, err := s.db.Query("SELECT id,name,level,category,sort FROM units ORDER BY sort,id")
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []domain.Unit
	for rows.Next() {
		var u domain.Unit
		rows.Scan(&u.ID, &u.Name, &u.Level, &u.Category, &u.Sort)
		out = append(out, u)
	}
	return out, nil
}

func (s *Store) UnitLevel(unitID int64) string {
	var lvl string
	s.db.QueryRow("SELECT level FROM units WHERE id=?", unitID).Scan(&lvl)
	return lvl
}

func (s *Store) UnitName(unitID int64) string {
	var name string
	s.db.QueryRow("SELECT name FROM units WHERE id=?", unitID).Scan(&name)
	return name
}

// UnitStats 查某单位的工程数/文档数/中央指标合计
func (s *Store) UnitStats(unitID int64) (projCount, docCount int, funding float64) {
	s.db.QueryRow("SELECT COUNT(*) FROM projects WHERE unit_id=?", unitID).Scan(&projCount)
	s.db.QueryRow("SELECT COUNT(*) FROM documents d JOIN projects p ON p.id=d.project_id WHERE p.unit_id=?", unitID).Scan(&docCount)
	s.db.QueryRow("SELECT COALESCE(SUM(central_funding),0) FROM projects WHERE unit_id=?", unitID).Scan(&funding)
	return
}

// CreateUnit 新建文物单位，返回新ID
func (s *Store) CreateUnit(name, level string, sort int) (int64, error) {
	res, err := s.db.Exec("INSERT INTO units(name,level,sort) VALUES(?,?,?)", name, level, sort)
	if err != nil {
		return 0, err
	}
	return res.LastInsertId()
}

// DeleteUnitRecords 删除单位及其下工程/文档的DB记录（文件需先另行移入回收站）
func (s *Store) DeleteUnitRecords(uid int64) {
	s.db.Exec("DELETE FROM documents WHERE project_id IN (SELECT id FROM projects WHERE unit_id=?)", uid)
	s.db.Exec("DELETE FROM projects WHERE unit_id=?", uid)
	s.db.Exec("DELETE FROM units WHERE id=?", uid)
}

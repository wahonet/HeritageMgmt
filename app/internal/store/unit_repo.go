package store

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
		if err := rows.Scan(&u.ID, &u.Name, &u.Level, &u.Category, &u.Sort); err != nil {
			return nil, err
		}
		out = append(out, u)
	}
	if err := rows.Err(); err != nil {
		return nil, err
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
	s.db.QueryRow("SELECT COUNT(*) FROM projects WHERE unit_id=? AND COALESCE(deleted,0)=0", unitID).Scan(&projCount)
	s.db.QueryRow("SELECT COUNT(*) FROM documents d JOIN projects p ON p.id=d.project_id WHERE p.unit_id=? AND COALESCE(p.deleted,0)=0", unitID).Scan(&docCount)
	s.db.QueryRow("SELECT COALESCE(SUM(central_funding),0) FROM projects WHERE unit_id=? AND COALESCE(deleted,0)=0", unitID).Scan(&funding)
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

// DeleteUnit 删除单位记录（其下工程须已软删入回收站，故此处不连带删 projects/documents）。
func (s *Store) DeleteUnit(uid int64) error {
	_, err := s.db.Exec("DELETE FROM units WHERE id=?", uid)
	return err
}

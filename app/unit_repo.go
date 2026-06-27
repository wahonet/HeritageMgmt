package main

// 数据层：文物单位(units)相关查询。

import "heritage-mgmt/internal/domain"

func ListUnits() ([]domain.Unit, error) {
	rows, err := db.Query("SELECT id,name,level,category,sort FROM units ORDER BY sort,id")
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

func unitLevel(unitID int64) string {
	var lvl string
	db.QueryRow("SELECT level FROM units WHERE id=?", unitID).Scan(&lvl)
	return lvl
}

func UnitName(unitID int64) string {
	var name string
	db.QueryRow("SELECT name FROM units WHERE id=?", unitID).Scan(&name)
	return name
}

// UnitStats 查某单位的工程数/文档数/中央指标合计
func UnitStats(unitID int64) (projCount, docCount int, funding float64) {
	db.QueryRow("SELECT COUNT(*) FROM projects WHERE unit_id=?", unitID).Scan(&projCount)
	db.QueryRow("SELECT COUNT(*) FROM documents d JOIN projects p ON p.id=d.project_id WHERE p.unit_id=?", unitID).Scan(&docCount)
	db.QueryRow("SELECT COALESCE(SUM(central_funding),0) FROM projects WHERE unit_id=?", unitID).Scan(&funding)
	return
}

// CreateUnit 新建文物单位，返回新ID
func CreateUnit(name, level string, sort int) (int64, error) {
	res, err := db.Exec("INSERT INTO units(name,level,sort) VALUES(?,?,?)", name, level, sort)
	if err != nil {
		return 0, err
	}
	return res.LastInsertId()
}

// DeleteUnitRecords 删除单位及其下工程/文档的DB记录（文件需先另行移入回收站）
func DeleteUnitRecords(uid int64) {
	db.Exec("DELETE FROM documents WHERE project_id IN (SELECT id FROM projects WHERE unit_id=?)", uid)
	db.Exec("DELETE FROM projects WHERE unit_id=?", uid)
	db.Exec("DELETE FROM units WHERE id=?", uid)
}
